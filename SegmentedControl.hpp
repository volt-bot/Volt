#pragma once
// SegmentedControl.hpp -- carousel/segmented-control widget.
//
// Layout and hit-testing math live in SegmentedControlCore.hpp (namespace
// SegCtrl) Scrolling/momentum/rubber-band/snap animation
// is delegated entirely to AnimCore.hpp's Anim::ScrollController -- a
// generic, reusable primitive shared by
// any scrollable IView, not duplicated per-widget. This class is now just
// the SDL/framework-facing shell: translating events into calls on
// ScrollController, and translating its position() back into TextBox
// positions and hit-test coordinates.
//
// Integration: update(currentTicksMs) must be called once per frame by your
// app's main loop, passing the CURRENT ABSOLUTE tick count -- i.e. exactly
// (float)SDL_GetTicks(), no conversion needed. This class calls
// adaptiveVsyncHD.startRedrawSession()/stopRedrawSession() automatically
// while animating, matching the same pattern EditBox/Cursor/RunningText use
// elsewhere in this framework.

#include "SegmentedControlCore.hpp"
#include "AnimCore.hpp"
#include <cmath>
#include <vector>

class SegmentedControl final : public Context, public IView {
public:
	enum class TransitionStatus {
		OK,
		WAIT, // Holds the transition, keeps UI active, and polls every frame
		CANCEL
	};

	struct Attributes {
		SDL_FRect bounds;
		SDL_Color bgColor = { 50, 50, 50, 255 };
		SDL_Color textColor = { 255, 255, 255, 255 };
		SDL_Color activeColor = { 0, 120, 215, 255 };
		float itemPadding = 10.f;   // percentage, converted to pixels ONCE in Build()
		float vertPadding = 5.f;    // percentage
		int indicatorHeight = 3;
		float friction = 0.9f;
		float cornerRadius = 0.f;   // percentage
		float textCornerRadius = 100.f;
		std::vector<std::string> items;
		std::size_t maxVisibleItems = 5;
		std::size_t selectedIndex = 0;

		// Pre-transition condition check (currentIndex, targetIndex) -> TransitionStatus
		std::function<TransitionStatus(std::size_t, std::size_t)> onPreTransition = nullptr;

		// onSelect callback(selected textbox, selected index)
		std::function<void(TextBox&, std::size_t)> onSelect = nullptr;
	};

	SegmentedControl() {}

	void Build(Context* _context, const SegmentedControl::Attributes& _attr) {
		setContext(_context);
		adaptiveVsyncHD.setAdaptiveVsync(adaptiveVsync);
		attr = _attr;
		bounds = attr.bounds;
		cv = this;

		Anim::ValueConfig cfg;
		cfg.friction = attr.friction;
		scroll_ = Anim::ScrollController(cfg);

		// Percentage -> pixels, converted exactly once, here. Every
		// downstream user (layout, hit-testing, snapping) reads this
		// already-converted pixel value directly.
		itemPaddingPx = to_cust(_attr.itemPadding, bounds.w);

		texture = CreateUniqueTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
			SDL_TEXTUREACCESS_TARGET, (int)bounds.w, (int)bounds.h);

		rebuildLayout();

		m_selectedIndex = std::min(attr.selectedIndex, attr.items.empty() ? 0 : attr.items.size() - 1);
		scroll_.setPosition(SegCtrl::computeSnapTarget(layout_, itemPaddingPx, m_selectedIndex, bounds.w, m_maxScroll));
		applyScrollToTextAreas();
	}

	bool handleEvent() override final {
		auto contains = [](const SDL_FRect& r, float x, float y) {
			return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
			};

		switch (event->type) {
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
			if (event->button.button == SDL_BUTTON_LEFT && contains(bounds, event->button.x, event->button.y)) {
				scroll_.beginDrag();
				m_lastMouseX = event->button.x;
				m_dragDistance = 0.f;
			}
			break;

		case SDL_EVENT_MOUSE_MOTION:
			if (scroll_.isDragging()) {
				float deltaX = event->motion.x - m_lastMouseX;
				m_lastMouseX = event->motion.x;
				m_dragDistance += std::abs(deltaX);
				scroll_.dragBy(-deltaX); // content moves opposite the finger, standard carousel convention
				applyScrollToTextAreas();
			}
			break;

		case SDL_EVENT_MOUSE_BUTTON_UP:
			if (scroll_.isDragging()) {
				bool wasTap = scroll_.endDrag(m_dragDistance);

				if (wasTap) {
					// hitTestToIndex converts the ABSOLUTE screen-space click
					// into the control's own content space by subtracting the
					// control's own screen position (pv->getRealX() + bounds.x)
					// before adding scroll.
					float controlScreenX = pv->getRealX() + bounds.x;
					int idx = SegCtrl::hitTestToIndex(layout_, itemPaddingPx,
						event->button.x, controlScreenX, scroll_.position());

					if (idx >= 0 && static_cast<std::size_t>(idx) != m_selectedIndex) {
						attemptTransition(static_cast<std::size_t>(idx), true, false);
					}
				}
				else {
					// A genuine drag release: momentum/rubber-band may still
					// need to run. beginAnimating() is a no-op if nothing
					// actually needs to keep ticking.
					beginAnimatingIfNeeded();
				}
			}
			break;
		}
		return false;
	}

	// Must be called once per frame by the app's main loop, passing the
	// CURRENT ABSOLUTE tick count in milliseconds -- (float)SDL_GetTicks().
	void update(float currentTicksMs) {
		if (m_isWaitingTransition) {
			attemptTransition(m_pendingTargetIndex, m_pendingAnimate, m_pendingTriggerCallback);
		}

		if (scroll_.isDragging()) return;

		scroll_.update(currentTicksMs);
		applyScrollToTextAreas();

		if (redraw_session_active_ && !scroll_.isAnimating() && !m_isWaitingTransition) {
			adaptiveVsyncHD.stopRedrawSession();
			redraw_session_active_ = false;
			if (attr.onSelect) {
				attr.onSelect(textAreas[m_selectedIndex], m_selectedIndex);
			}
		}
	}

	void onUpdate() override final { update((float)SDL_GetTicks()); }

	// --- Diagnostic accessors -- harmless, read-only, useful for any
	// consumer inspecting the widget's state, and used by regression tests. ---
	[[nodiscard]] std::size_t getTextAreaCount() const { return textAreas.size(); }
	[[nodiscard]] float getItemPaddingPx() const { return itemPaddingPx; }
	[[nodiscard]] float getMaxScroll() const { return m_maxScroll; }
	[[nodiscard]] std::size_t getSelectedIndex() const { return m_selectedIndex; }
	[[nodiscard]] float getScrollXForTest() const { return scroll_.position(); }
	[[nodiscard]] float getTargetScrollX() const { return scroll_.target(); }
	[[nodiscard]] bool isAnimatingSnap() const { return scroll_.isEasing(); }
	[[nodiscard]] int getRedrawSessionCount() const { return redraw_session_active_ ? 1 : 0; }
	TextBox& getSelectedTextBox() { return textAreas[m_selectedIndex]; }

	/// @brief change the currently selected item.
	/// @param index The index of the item to select.
	/// @param animate If true, smooth-scrolls (snaps) to the target item; if false, jumps immediately.
	/// @param triggerCallback If true, invokes the `onSelect` callback if configured.
	void setSelected(std::size_t index, bool animate = true, bool triggerCallback = false) {
		if (textAreas.empty() || index >= textAreas.size()) {
			return; // Guard against out-of-bounds indices or uninitialized items
		}
		attemptTransition(index, animate, triggerCallback);
	}

	void draw() override final {
		CacheRenderTarget crt(renderer);
		SDL_SetRenderTarget(renderer, texture.get());
		SDL_SetRenderDrawColor(renderer, attr.bgColor.r, attr.bgColor.g, attr.bgColor.b, attr.bgColor.a);
		SDL_RenderClear(renderer);
		for (auto& ta : textAreas) {
			ta.draw();
		}
		crt.release(renderer);
		transformToRoundedTexture(renderer, texture.get(), attr.cornerRadius);
		SDL_RenderTexture(renderer, texture.get(), nullptr, &bounds);
	}

private:
	static constexpr SDL_Color kOutlineColor = { 25, 40, 45, 0xff };

	void attemptTransition(std::size_t targetIndex, bool animate = true, bool triggerCallback = false) {
		if (targetIndex == m_selectedIndex) {
			m_isWaitingTransition = false;
			return;
		}

		if (attr.onPreTransition) {
			TransitionStatus status = attr.onPreTransition(m_selectedIndex, targetIndex);

			if (status == TransitionStatus::CANCEL) {
				m_isWaitingTransition = false;
				return;
			}
			else if (status == TransitionStatus::WAIT) {
				m_isWaitingTransition = true;
				m_pendingTargetIndex = targetIndex;
				m_pendingAnimate = animate;
				m_pendingTriggerCallback = triggerCallback;

				// Ensure the UI stays alive to poll every frame
				if (!redraw_session_active_) {
					adaptiveVsyncHD.startRedrawSession();
					redraw_session_active_ = true;
				}
				return; // Hold here until a future frame evaluates to OK or CANCEL
			}
		}

		// Proceed with the actual transition
		m_isWaitingTransition = false;
		executeTransition(targetIndex, animate, triggerCallback);
	}

	void executeTransition(std::size_t targetIndex, bool animate, bool triggerCallback) {
		setItemHighlighted(m_selectedIndex, false);
		m_selectedIndex = targetIndex;
		attr.selectedIndex = targetIndex;
		setItemHighlighted(m_selectedIndex, true);

		if (animate) {
			snapToSelected();
		}
		else {
			float target = SegCtrl::computeSnapTarget(layout_, itemPaddingPx, m_selectedIndex, bounds.w, m_maxScroll);
			scroll_.setPosition(target);
			applyScrollToTextAreas();
		}

		if (triggerCallback && attr.onSelect) {
			attr.onSelect(textAreas[m_selectedIndex], m_selectedIndex);
		}
	}

	void setItemHighlighted(std::size_t index, bool highlighted) {
		if (index >= textAreas.size()) return;
		SDL_Color bg = highlighted ? attr.activeColor : SDL_Color{ 0, 0, 0, 0 };
		textAreas[index].updateTextColor(bg, kOutlineColor, attr.textColor);
	}

	void rebuildLayout() {
		layout_ = SegCtrl::computeLayout(attr.items.size(), bounds.w, attr.maxVisibleItems, itemPaddingPx);
		float contentWidth = SegCtrl::computeContentWidth(layout_, itemPaddingPx);
		m_maxScroll = SegCtrl::computeMaxScroll(contentWidth, bounds.w);
		scroll_.setBounds(0.f, m_maxScroll);

		textAreas.clear();
		textAreas.reserve(attr.items.size());
		for (std::size_t i = 0; i < attr.items.size(); ++i) {
			SDL_Color txt_bg = (i == attr.selectedIndex) ? attr.activeColor : SDL_Color{ 0, 0, 0, 0 };
			textAreas.emplace_back().Build(this, {
				.rect = { layout_[i].baseX, to_cust(attr.vertPadding, bounds.h),
						  layout_[i].width, to_cust(100.f - (attr.vertPadding * 2.f), bounds.h) },
				.textAttributes = { attr.items[i], attr.textColor, txt_bg },
				.margin = { 5.f, 15.f, 5.f, 35.f },
				.gravity = Gravity::Center,
				.cornerRadius = attr.textCornerRadius,
				.outline = 0.f,
				.useHaptics = true,
				.outlineColor = kOutlineColor,
				});
		}
	}

	void applyScrollToTextAreas() {
		float s = scroll_.position();
		for (std::size_t i = 0; i < textAreas.size() && i < layout_.size(); ++i) {
			float targetX = layout_[i].baseX - s;
			float delta = targetX - textAreas[i].bounds.x;
			if (delta != 0.f) textAreas[i].updatePosBy(delta, 0.f);
		}
	}

	void beginAnimatingIfNeeded() {
		if (!scroll_.isAnimating()) return;
		if (!redraw_session_active_) {
			adaptiveVsyncHD.startRedrawSession();
			redraw_session_active_ = true;
		}
		scroll_.resetTickBaseline();
	}

	void snapToSelected() {
		float target = SegCtrl::computeSnapTarget(layout_, itemPaddingPx, m_selectedIndex, bounds.w, m_maxScroll);
		scroll_.snapTo(target);
		beginAnimatingIfNeeded();
	}

	std::vector<SegCtrl::ItemLayout> layout_;
	std::size_t m_selectedIndex = 0;
	float itemPaddingPx = 0.f;
	float m_maxScroll = 0.0f;

	Anim::ScrollController scroll_;
	bool redraw_session_active_ = false;

	// Polling state variables
	bool m_isWaitingTransition = false;
	std::size_t m_pendingTargetIndex = 0;
	bool m_pendingAnimate = true;
	bool m_pendingTriggerCallback = false;

	float m_lastMouseX = 0.f;
	float m_dragDistance = 0.f;

	SegmentedControl::Attributes attr{};
	std::vector<TextBox> textAreas;
	UniqueTexture texture;
	AdaptiveVsyncHandler adaptiveVsyncHD;
};
