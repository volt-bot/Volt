#pragma once
// SegmentedControlCore.hpp

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
	// ever sees it.
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

	// max(0, contentWidth - viewportWidth)
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

} // namespace SegCtrl
