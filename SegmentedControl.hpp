#pragma once
// SegmentedControl.hpp -- carousel/segmented-control widget.

#include "SegmentedControlCore.hpp"
#include <cmath>
#include <vector>

class SegmentedControl : public Context, public IView {
public:
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
		std::vector<std::string> items;
		std::size_t maxVisibleItems = 5;
		std::size_t selectedIndex = 0;
	};

	SegmentedControl() {}

	void Build(Context* _context, const SegmentedControl::Attributes& _attr) {
		setContext(_context);
		adaptiveVsyncHD.setAdaptiveVsync(adaptiveVsync);
		attr = _attr;
		bounds = attr.bounds;
		cv = this;

		// Every downstream user of padding (layout, hit-testing, snapping)
		// reads this already-converted pixel value directly.
		itemPaddingPx = to_cust(_attr.itemPadding, bounds.w);

		texture = CreateUniqueTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
			SDL_TEXTUREACCESS_TARGET, (int)bounds.w, (int)bounds.h);

		rebuildLayout();

		m_selectedIndex = std::min(attr.selectedIndex, attr.items.empty() ? 0 : attr.items.size() - 1);
		m_scrollX = SegCtrl::computeSnapTarget(layout_, itemPaddingPx, m_selectedIndex, bounds.w, m_maxScroll);
		m_targetScrollX = m_scrollX;
		applyScrollToTextAreas();
	}

	bool handleEvent() override final {
		auto contains = [](const SDL_FRect& r, float x, float y) {
			return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
			};

		switch (event->type) {
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
			if (event->button.button == SDL_BUTTON_LEFT && contains(bounds, event->button.x, event->button.y)) {
				m_isDragging = true;
				m_isAnimatingSnap = false; // a fresh drag interrupts any in-flight snap
				m_lastMouseX = event->button.x;
				m_dragDistance = 0.f;
				m_velocity = 0.f;
			}
			break;

		case SDL_EVENT_MOUSE_MOTION:
			if (m_isDragging) {
				float deltaX = event->motion.x - m_lastMouseX;
				m_lastMouseX = event->motion.x;
				m_dragDistance += std::abs(deltaX);
				m_velocity = -deltaX;

				// position. Update it, clamp it, and derive every TextBox's
				// bounds.x from it via applyScrollToTextAreas().
				m_scrollX = SegCtrl::clampScroll(m_scrollX - deltaX, m_maxScroll);
				applyScrollToTextAreas();
			}
			break;

		case SDL_EVENT_MOUSE_BUTTON_UP:
			if (m_isDragging) {
				m_isDragging = false;

				if (m_dragDistance < 10.0f) {
					// hitTestToIndex converts the ABSOLUTE
					// screen-space click into the control's own content
					// space by subtracting the control's own screen
					// position (pv->getRealX() + bounds.x) before adding scroll
					float controlScreenX = pv->getRealX() + bounds.x;
					int idx = SegCtrl::hitTestToIndex(layout_, itemPaddingPx,
						event->button.x, controlScreenX, m_scrollX);
					if (idx >= 0) {
						m_selectedIndex = static_cast<std::size_t>(idx);
						attr.selectedIndex = m_selectedIndex;
						snapToSelected();
					}
				}
			}
			break;
		}
		return false;
	}

	// Must be called once per frame.
	void update(float deltaTime) {
		if (m_isDragging) return;

		if (m_isAnimatingSnap) {
			// The actual animation: m_scrollX eases toward
			// m_targetScrollX over multiple frames instead of jumping there
			// in one synchronous call.
			m_scrollX = SegCtrl::lerpTowards(m_scrollX, m_targetScrollX, deltaTime, kSnapLerpSpeed);
			applyScrollToTextAreas();

			if (SegCtrl::isSettled(m_scrollX, m_targetScrollX)) {
				m_scrollX = m_targetScrollX;
				applyScrollToTextAreas();
				m_isAnimatingSnap = false;
				adaptiveVsyncHD.stopRedrawSession(); // [FIX BUG 8]
				redraw_session_active_ = false;
			}
			return;
		}

		// Momentum/friction after a drag release (unrelated to snap-lerp --
		// mutually exclusive with it via the early return above).
		bool moved = false;
		if (std::abs(m_velocity) > 0.1f) {
			m_scrollX += m_velocity * deltaTime * 60.0f;
			m_velocity *= attr.friction;
			moved = true;
		}

		float clamped = SegCtrl::clampScroll(m_scrollX, m_maxScroll);
		if (clamped != m_scrollX) {
			m_scrollX = std::lerp(m_scrollX, clamped, deltaTime * 10.0f);
			moved = true;
		}

		if (moved) applyScrollToTextAreas();
	}

	void onUpdate() override final {
		float tcks = app->getFPS();
		update(tcks);
	}

	// --- Diagnostic accessors -- harmless, read-only, useful for any
	// consumer inspecting the widget's state (e.g. a debug overlay), and
	// used by this file's regression tests. ---
	[[nodiscard]] std::size_t getTextAreaCountForTest() const { return textAreas.size(); }
	[[nodiscard]] float getItemPaddingPxForTest() const { return itemPaddingPx; }
	[[nodiscard]] float getMaxScrollForTest() const { return m_maxScroll; }
	[[nodiscard]] std::size_t getSelectedIndexForTest() const { return m_selectedIndex; }
	[[nodiscard]] float getScrollXForTest() const { return m_scrollX; }
	[[nodiscard]] float getTargetScrollXForTest() const { return m_targetScrollX; }
	[[nodiscard]] bool isAnimatingSnapForTest() const { return m_isAnimatingSnap; }
	[[nodiscard]] int getRedrawSessionCountForTest() const { return redraw_session_active_ ? 1 : 0; }

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
	void rebuildLayout() {
		layout_ = SegCtrl::computeLayout(attr.items.size(), bounds.w, attr.maxVisibleItems, itemPaddingPx);
		float contentWidth = SegCtrl::computeContentWidth(layout_, itemPaddingPx);
		m_maxScroll = SegCtrl::computeMaxScroll(contentWidth, bounds.w); // [FIX BUG 2]

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
				.cornerRadius = 100.f,
				.outline = 0.f,
				.useHaptics = true,
				.outlineColor = { 25, 40, 45, 0xff },
				});
		}
	}

	// The ONLY place TextBox positions are ever written. Always
	// derives bounds.x from the fixed layout_[i].baseX minus the CURRENT
	// m_scrollX -- called after every change to m_scrollX (drag motion,
	// momentum, snap-lerp), never mutated incrementally/directly elsewhere.
	void applyScrollToTextAreas() {
		for (std::size_t i = 0; i < textAreas.size() && i < layout_.size(); ++i) {
			float targetX = layout_[i].baseX - m_scrollX;
			float delta = targetX - textAreas[i].bounds.x;
			if (delta != 0.f) textAreas[i].updatePosBy(delta, 0.f);
		}
	}

	void snapToSelected() {
		// Only set the TARGET here. update() does the actual
		// animating, every frame, until it settles.
		m_targetScrollX = SegCtrl::computeSnapTarget(layout_, itemPaddingPx, m_selectedIndex, bounds.w, m_maxScroll);
		if (!SegCtrl::isSettled(m_scrollX, m_targetScrollX)) {
			m_isAnimatingSnap = true;
			adaptiveVsyncHD.startRedrawSession();
			redraw_session_active_ = true;
		}
	}

	std::vector<SegCtrl::ItemLayout> layout_;
	std::size_t m_selectedIndex = 0;

	float itemPaddingPx = 0.f; // converted exactly once, in Build()

	// Scrolling / animation state
	float m_scrollX = 0.0f;
	float m_targetScrollX = 0.0f;
	float m_velocity = 0.0f;
	float m_maxScroll = 0.0f;
	bool m_isAnimatingSnap = false;
	bool redraw_session_active_ = false; // tracked ourselves; independent of AdaptiveVsyncHandler's own internals

	static constexpr float kSnapLerpSpeed = 8.f;

	// Interaction state
	bool m_isDragging = false;
	float m_lastMouseX = 0.f;
	float m_dragDistance = 0.f;

	SegmentedControl::Attributes attr{};
	std::vector<TextBox> textAreas;
	UniqueTexture texture;
	AdaptiveVsyncHandler adaptiveVsyncHD;
};
