#pragma once
// SegmentedControlCore.hpp -- pure logic, zero SDL/framework dependency.

#include <algorithm>
#include <cstddef>
#include <vector>

namespace SegCtrl {

	struct ItemLayout {
		float baseX = 0.f;  // unscrolled x position within the control's own content space
		float width = 0.f;  // this item's box width (excludes padding)
	};

	// Computes each item's fixed (unscrolled) x position and the resulting
	// max scroll range, given a uniform item width (matching how this widget
	// actually lays things out -- every item gets bounds.w / maxVisibleItems,
	// not a per-text measured width).
	//
	// paddingPx must already be in PIXELS, not a percentage -- converting a
	// percentage happens exactly once, by the caller, before this function
	// ever sees it. (The original bug converted twice: once by the caller,
	// once again inside the per-item loop.)
	inline std::vector<ItemLayout> computeLayout(
		std::size_t itemCount, float controlWidth, std::size_t maxVisibleItems, float paddingPx)
	{
		std::vector<ItemLayout> out;
		out.reserve(itemCount);
		if (maxVisibleItems == 0) maxVisibleItems = 1;
		float itemW = controlWidth / static_cast<float>(maxVisibleItems);
		for (std::size_t i = 0; i < itemCount; ++i) {
			ItemLayout item;
			item.width = itemW;
			item.baseX = static_cast<float>(i) * (itemW + paddingPx * 2.f);
			out.push_back(item);
		}
		return out;
	}

	// Total scrollable content width implied by a layout (last item's right
	// edge, including its trailing padding).
	inline float computeContentWidth(const std::vector<ItemLayout>& layout, float paddingPx) {
		if (layout.empty()) return 0.f;
		const auto& last = layout.back();
		return last.baseX + last.width + paddingPx * 2.f;
	}

	// max(0, contentWidth - viewportWidth). This was the original bug's most
	// visible consequence: calculateLayout() was never called and its body
	// was entirely commented out, so this was always literally 0, which
	// made every scroll position except exactly 0 look "out of bounds" to
	// the rubber-band clamp below.
	inline float computeMaxScroll(float contentWidth, float viewportWidth) {
		return std::max(0.f, contentWidth - viewportWidth);
	}

	// Clamps a scroll position into [0, maxScroll]. Used both for hard
	// clamping and as the target the rubber-band lerp approaches.
	inline float clampScroll(float scrollX, float maxScroll) {
		return std::clamp(scrollX, 0.f, maxScroll);
	}

	// Given a click x-coordinate ALREADY converted into the control's own
	// unscrolled content space (i.e. caller has already done
	// (absoluteClickX - controlScreenX) + scrollX -- see hitTestToIndex
	// below, which does that conversion for you), returns the index of the
	// item under that point, or -1 if none.
	inline int resolveItemIndexFromContentX(
		const std::vector<ItemLayout>& layout, float paddingPx, float contentX)
	{
		for (std::size_t i = 0; i < layout.size(); ++i) {
			float start = layout[i].baseX;
			float end = start + layout[i].width + paddingPx * 2.f;
			if (contentX >= start && contentX <= end) return static_cast<int>(i);
		}
		return -1;
	}

	// End-to-end hit test: converts an ABSOLUTE screen-space click x into an
	// item index. This is the fix for the original bug where
	// `event->button.x + m_scrollX` never subtracted the control's own
	// screen position -- every tap on a control not sitting at screen x==0
	// resolved against the wrong item.
	inline int hitTestToIndex(
		const std::vector<ItemLayout>& layout, float paddingPx,
		float absoluteClickX, float controlScreenX, float scrollX)
	{
		float contentX = (absoluteClickX - controlScreenX) + scrollX;
		return resolveItemIndexFromContentX(layout, paddingPx, contentX);
	}

	// Target scroll position that centers `index` in the viewport, clamped
	// to a valid scroll range.
	inline float computeSnapTarget(
		const std::vector<ItemLayout>& layout, float paddingPx,
		std::size_t index, float viewportWidth, float maxScroll)
	{
		if (index >= layout.size()) return 0.f;
		float itemCenter = layout[index].baseX + layout[index].width / 2.f + paddingPx;
		float target = itemCenter - viewportWidth / 2.f;
		return clampScroll(target, maxScroll);
	}

	// One frame's worth of exponential-decay lerp toward `target`, framerate-
	// independent via deltaTime. Returns the new value. `speed` is a rate
	// constant (higher = snappier); values around 8-12 feel responsive
	// without being an instant jump.
	inline float lerpTowards(float current, float target, float deltaTime, float speed) {
		float t = std::clamp(deltaTime * speed, 0.f, 1.f);
		return current + (target - current) * t;
	}

	// True once `current` is close enough to `target` that further lerping
	// is imperceptible -- used to decide when to stop requesting redraws and
	// snap exactly, rather than asymptotically approaching forever.
	inline bool isSettled(float current, float target, float epsilon = 0.25f) {
		return std::abs(current - target) < epsilon;
	}

	// Converts "current absolute tick count in ms" + "last call's tick count"
	// into a robust delta-time in SECONDS, suitable for feeding lerpTowards()
	// and friction/momentum math. This exists because the natural, easy-to-
	// reach-for call site is `update(SDL_GetTicks())` -- passing an ABSOLUTE,
	// ever-growing counter -- not a genuine per-frame delta. Every piece of
	// deltaTime-based math in this file assumes a small (~0.016s) value; an
	// absolute tick count is many orders of magnitude larger, and the
	// difference matters a lot depending on which formula it hits:
	//   - lerpTowards() clamps its `t` to [0,1], so a huge deltaTime just
	//     saturates to an instant jump -- silently wrong (no animation),
	//     but at least bounded.
	//   - the momentum formula (`scrollX += velocity * deltaTime * 60`) and
	//     the rubber-band `std::lerp(a, b, deltaTime * 10)` are NOT clamped
	//     -- std::lerp extrapolates freely outside t=[0,1] -- so a huge
	//     deltaTime blows scrollX up to an enormous, effectively garbage
	//     value. This is what "manual scroll goes to incorrect positions"
	//     looks like: one update() call after a drag leaves nonzero
	//     velocity is enough to send scrollX into the millions.
	//
	// Handles the two edge cases that make "just diff two tick counts"
	// unsafe on its own:
	//   - First call ever (no previous tick to diff against): returns 0 so
	//     nothing moves on that call, rather than computing a huge delta
	//     against an uninitialized/zero previous value.
	//   - Clock went backwards or stalled (debugger pause, app backgrounded
	//     for a long time): clamps the result to maxDeltaSeconds so a single
	//     catch-up frame can't overshoot wildly (a standard "spiral of
	//     death" guard in frame-timed update loops).
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
		if (deltaMs < 0.f) return 0.f; // clock went backwards -- treat as no time passed
		float deltaSeconds = deltaMs / 1000.f;
		return std::min(deltaSeconds, maxDeltaSeconds);
	}

} // namespace SegCtrl
