#pragma once
// AnimCore.hpp -- generic, reusable animation primitives for Volt.
//
// Two layers:
//
//   Anim::Value -- a single animated float, usable for ANYTHING: a scroll
//   position, a fade opacity, a rotation angle, one channel of a color, a
//   view's x/y offset during a slide-in transition, etc. Supports four
//   things, all driven by the same robust tick-based update():
//     - instant set (no animation)
//     - ease toward a target (exponential-decay lerp, like a "snap")
//     - momentum + friction (velocity-driven, decays over time)
//     - rubber-band bounds (elastic overscroll past [min,max] that eases
//       back once released)
//
//   Anim::ScrollController -- a thin convenience wrapper around Anim::Value
//   specifically for the drag-to-scroll interaction pattern (pointer
//   down/move/up -> velocity tracking -> momentum -> rubber-band -> optional
//   discrete snap points). This is what an IView-derived scrollable view
//   (SegmentedControl, CellBlock, a horizontal image carousel, anything)
//   actually owns and forwards its own handleEvent()/update() calls into --
//   it's axis-agnostic (just a scalar), so the owning view decides whether
//   that scalar maps to x or y.
//
// Neither type inherits from IView or Context, and neither assumes anything
// about SDL beyond "you'll give me float coordinates and an absolute tick
// count." That's deliberate: composition (an IView-derived class HOLDS one
// of these as a member) works for any view type without multiple-inheritance
// complexity or coupling this file to the rest of the framework.

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace Anim {

	// =========================================================================
	// Tick -> delta-seconds conversion, shared by everything below.
	// =========================================================================
	// Converts "current absolute tick count in ms" + "last call's tick count"
	// into a robust delta-time in SECONDS. See SegmentedControlCore.hpp's
	// original version of this function for the full history of why this
	// exists (in short: `update(SDL_GetTicks())` is the natural call site,
	// not `update(deltaSeconds)`, and treating an absolute counter as a
	// per-frame delta silently produces either an instant jump or a
	// catastrophic multi-million-pixel overshoot depending on which formula
	// it hits). This copy is identical in behavior; it lives here too so
	// AnimCore.hpp has no dependency on SegmentedControlCore.hpp.
	inline float ticksToDeltaSeconds(float currentTicksMs, float& lastTicksMs, bool& hasLastTick,
		float maxDeltaSeconds = 0.1f)
	{
		if (!hasLastTick) {
			hasLastTick = true;
			lastTicksMs = currentTicksMs;
			return 0.f;
		}
		float deltaMs = currentTicksMs - lastTicksMs;
		lastTicksMs = currentTicksMs;
		if (deltaMs < 0.f) return 0.f;
		float deltaSeconds = deltaMs / 1000.f;
		return std::min(deltaSeconds, maxDeltaSeconds);
	}

	inline float lerpTowards(float current, float target, float deltaTime, float speed) {
		float t = std::clamp(deltaTime * speed, 0.f, 1.f);
		return current + (target - current) * t;
	}

	inline bool isSettled(float current, float target, float epsilon = 0.25f) {
		return std::abs(current - target) < epsilon;
	}

	// =========================================================================
	// Anim::Value -- one animated float, four modes.
	// =========================================================================
	struct ValueConfig {
		float friction = 0.9f;              // momentum decay per-frame multiplier
		float velocityStopThreshold = 0.1f; // below this, momentum is considered "stopped"
		float rubberBandLerpSpeed = 10.f;    // how fast an out-of-bounds value eases back in
		float easeLerpSpeed = 8.f;           // how fast ease-to-target approaches its target
		float settleEpsilon = 0.25f;        // "close enough" distance to a target/bound
		float maxDeltaSeconds = 0.1f;        // stall guard, see ticksToDeltaSeconds
		float maxOverscroll = 60.f;          // asymptotic cap on how far a live drag can push past a bound
	};

	class Value {
	public:
		explicit Value(ValueConfig cfg = {}) : cfg_(cfg) {}

		void setBounds(float minValue, float maxValue) {
			min_ = minValue;
			max_ = maxValue;
			hasBounds_ = true;
		}
		void clearBounds() { hasBounds_ = false; }

		// Instant, unanimated set -- e.g. initial layout, or a hard reset.
		void set(float v) {
			current_ = v;
			target_ = v;
			velocity_ = 0.f;
			easing_ = false;
			rawOverscrollAttempt_ = 0.f;
		}

		[[nodiscard]] float get() const { return current_; }
		[[nodiscard]] float target() const { return target_; }

		// Nudges the value by `delta` right now (e.g. one frame of drag
		// movement). Past a bound, resistance is NOT a flat damping factor
		// -- a flat factor still lets a long enough drag push arbitrarily
		// far past the edge, just more slowly. Instead this tracks how much
		// raw (unresisted) drag has been ATTEMPTED past the current bound
		// and maps that through tanh(), which asymptotically saturates: the
		// resulting overscroll converges to cfg_.maxOverscroll regardless
		// of how much further the user keeps dragging (verified
		// empirically before wiring this in: 100px of raw over-drag reaches
		// ~93% of the cap, 500px+ is indistinguishable from the cap itself).
		// Also records the delta as instantaneous velocity, so a subsequent
		// release() can hand off to momentum smoothly.
		void dragBy(float delta) {
			easing_ = false; // a live drag always interrupts any in-flight ease-to-target

			if (!hasBounds_) {
				current_ += delta;
				velocity_ = delta;
				return;
			}

			bool pushingPastMin = (current_ <= min_ && delta < 0.f);
			bool pushingPastMax = (current_ >= max_ && delta > 0.f);

			if (!pushingPastMin && !pushingPastMax) {
				// In-bounds: once this drag crosses INTO overscroll territory
				// this frame, the remainder of `delta` still gets resisted --
				// but the common case (fully in-bounds) is a plain 1:1 move.
				current_ += delta;
				velocity_ = delta;
				rawOverscrollAttempt_ = 0.f; // back in bounds -- reset the accumulator
				return;
			}

			rawOverscrollAttempt_ += std::abs(delta);
			float bound = pushingPastMin ? min_ : max_;
			float sign = pushingPastMin ? -1.f : 1.f;
			float overscroll = cfg_.maxOverscroll * std::tanh(rawOverscrollAttempt_ / cfg_.maxOverscroll);
			float newPos = bound + sign * overscroll;
			velocity_ = newPos - current_;
			current_ = newPos;
		}

		// Hands off to momentum with the given initial velocity (typically
		// the last drag delta, or an explicit flick velocity).
		void release(float initialVelocity) {
			velocity_ = initialVelocity;
			easing_ = false;
			rawOverscrollAttempt_ = 0.f;
		}

		// Eases toward `target` over multiple update() calls rather than
		// jumping there instantly. Cancels any in-flight momentum.
		void easeTo(float target) {
			target_ = target;
			velocity_ = 0.f;
			easing_ = !isSettled(current_, target_, cfg_.settleEpsilon);
			rawOverscrollAttempt_ = 0.f;
		}

		[[nodiscard]] bool isEasing() const { return easing_; }

		// True if ANYTHING is still moving -- momentum, rubber-band
		// correction, or an ease-to-target. Use this to decide whether to
		// keep requesting redraws.
		[[nodiscard]] bool isAnimating() const {
			return easing_ || std::abs(velocity_) > cfg_.velocityStopThreshold || isOutOfBounds();
		}

		[[nodiscard]] bool isOutOfBounds() const {
			return hasBounds_ && (current_ < min_ - cfg_.settleEpsilon || current_ > max_ + cfg_.settleEpsilon);
		}

		// Must be called once per frame while isAnimating() is true, passing
		// the CURRENT ABSOLUTE TICK COUNT ((float)SDL_GetTicks(), no
		// conversion needed). Internally computes its own robust per-frame
		// delta -- see ticksToDeltaSeconds's header comment for exactly why
		// that matters (an idle gap between animations, e.g. while nothing
		// requested redraws, must not be misread as a single giant frame).
		void update(float currentTicksMs) {
			float dt = ticksToDeltaSeconds(currentTicksMs, lastTicksMs_, hasLastTick_, cfg_.maxDeltaSeconds);

			if (easing_) {
				current_ = lerpTowards(current_, target_, dt, cfg_.easeLerpSpeed);
				if (isSettled(current_, target_, cfg_.settleEpsilon)) {
					current_ = target_;
					easing_ = false;
				}
				return;
			}

			if (std::abs(velocity_) > cfg_.velocityStopThreshold) {
				current_ += velocity_ * dt * 60.0f; // *60 keeps the same feel as a per-frame-at-60fps velocity unit
				velocity_ *= cfg_.friction;
			}

			if (hasBounds_) {
				float clamped = std::clamp(current_, min_, max_);
				if (clamped != current_) {
					current_ = lerpTowards(current_, clamped, dt, cfg_.rubberBandLerpSpeed);
				}
			}
		}

		// Forces the tick-tracking baseline to reset, so the NEXT update()
		// call establishes a fresh delta origin (returning delta=0 for that
		// one call) instead of computing a stale-gap delta against whenever
		// update() last happened to run. Call this whenever you're about to
		// (re)start requesting continuous redraws after a period of NOT
		// calling update() -- e.g. right before easeTo()/release() if your
		// view only calls update() while animating. Skipping this is
		// exactly the bug that made SegmentedControl's snap-to-center jump
		// straight to its target instead of animating: the first post-idle
		// frame saw a large clamped delta and covered ~80% of the distance
		// in one step.
		void resetTickBaseline() { hasLastTick_ = false; }

	private:
		ValueConfig cfg_;
		float current_ = 0.f;
		float target_ = 0.f;
		float velocity_ = 0.f;
		bool easing_ = false;

		float min_ = 0.f, max_ = 0.f;
		bool hasBounds_ = false;
		float rawOverscrollAttempt_ = 0.f; // accumulator for the asymptotic drag-resistance formula

		float lastTicksMs_ = 0.f;
		bool hasLastTick_ = false;
	};

	// =========================================================================
	// Anim::ScrollController -- drag/momentum/rubber-band/optional-snap.
	// =========================================================================
	// Wraps a single Anim::Value with the specific pointer-event sequencing
	// every drag-to-scroll view needs: beginDrag() on pointer-down,
	// dragBy(delta) per pointer-move, endDrag() on pointer-up (which decides
	// whether the release was a tap -- caller's job to check drag distance --
	// or a real drag that should hand off to momentum). snapTo() is separate
	// and optional: call it whenever YOUR view decides a discrete target
	// position is wanted (centering a selected item, snapping to the
	// nearest row, etc.) -- a free-scrolling grid that never snaps simply
	// never calls it.
	class ScrollController {
	public:
		explicit ScrollController(ValueConfig cfg = {}) : value_(cfg) {}

		void setBounds(float minValue, float maxValue) { value_.setBounds(minValue, maxValue); }
		void setPosition(float v) { value_.set(v); }
		[[nodiscard]] float position() const { return value_.get(); }
		[[nodiscard]] float target() const { return value_.target(); }
		[[nodiscard]] bool isAnimating() const { return value_.isAnimating(); }
		[[nodiscard]] bool isEasing() const { return value_.isEasing(); }

		void beginDrag() {
			dragging_ = true;
			value_.release(0.f); // cancel any in-flight momentum/ease; a fresh drag always takes over
		}

		// `delta` is in the SAME sign convention you want position() to
		// move in -- if your view treats "drag right = content moves right",
		// pass the raw pointer delta; if it should feel like "dragging the
		// content" (finger moves right, content follows), pass it as-is;
		// most scrollable views want the CONTENT to move opposite the
		// finger for a horizontal carousel (finger right -> reveal earlier
		// items -> scrollX decreases) or WITH the finger for a vertical
		// list (finger down -> reveal earlier items -> scrollY decreases
		// too, same convention) -- decide the sign at the call site, this
		// class doesn't assume an axis or direction.
		void dragBy(float delta) {
			if (!dragging_) return;
			value_.dragBy(delta);
		}

		// Returns true if this should be treated as a tap rather than a
		// drag (caller decides the threshold and what a tap means for their
		// view -- ScrollController doesn't know about item hit-testing).
		[[nodiscard]] bool endDrag(float totalDragDistance, float tapThreshold = 10.0f) {
			dragging_ = false;
			bool wasTap = totalDragDistance < tapThreshold;
			if (wasTap) {
				value_.release(0.f); // a tap leaves no residual momentum
			}
			// A real drag's momentum is already loaded via the last
			// dragBy()'s velocity tracking inside Anim::Value -- nothing
			// further to do here. The caller is responsible for calling
			// resetTickBaseline() (via this class's own passthrough below)
			// if it's about to start requesting continuous redraws after a
			// period of not doing so.
			return wasTap;
		}

		[[nodiscard]] bool isDragging() const { return dragging_; }

		// Animate toward a discrete target (optional -- only call this if
		// your view actually wants discrete snap points; a free-scrolling
		// grid never needs to).
		void snapTo(float target) {
			value_.easeTo(target);
		}

		void resetTickBaseline() { value_.resetTickBaseline(); }

		void update(float currentTicksMs) { value_.update(currentTicksMs); }

	private:
		Value value_;
		bool dragging_ = false;
	};

} // namespace Anim
