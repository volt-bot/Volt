#pragma once
// EditBox.hpp -- v2, rebuilt on Volt's EditCore.hpp (Utf8TextBuffer + EditCursorModel).
//  - SDL3 keymod flags are named SDL_KMOD_CTRL / SDL_KMOD_SHIFT / SDL_KMOD_GUI.
//    I treat Ctrl OR GUI(Cmd) as "the command modifier" for Mac-friendliness --
//    drop the "|| (mod & SDL_KMOD_GUI)" checks if you don't want that.
//  - SDL_GetClipboardText()/SDL_SetClipboardText()/SDL_HasClipboardText() exist
//    as in stock SDL3 and SDL_GetClipboardText()'s result must be freed with
//    SDL_free().
//  - IME composition (SDL_EVENT_TEXT_EDITING) support below is basic
//    (renders the preedit string with an underline) and is the LEAST
//    verified part of this file, since I have no way to exercise a real IME
//    in this sandbox. Please test with your target input methods.
// -----------------------------------------------------------------------------

#include "EditCore.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

//namespace Volt {

	struct EditBoxAttributes {
		SDL_FRect rect = { 0.f, 0.f, 80.f, 30.f };
		SDL_FRect placeholderRect = { 10.f, 5.f, 80.f, 90.f };
		TextAttributes textAttributes = { "", {0x00, 0x00, 0x00, 0xff}, {0xff, 0xff, 0xff, 0xff} };
		TextAttributes placeholderTextAttributes = { "", {0x00, 0x00, 0x00, 0xff}, {0xff, 0xff, 0xff, 0xff} };
		SDL_FRect margin = { 0.f, 0.f, 100.f, 100.f };
		Font mem_font = Font::RobotoBold;
		std::string fontFile;
		Font placeholder_mem_font = Font::RobotoBold;
		std::string placeholderFontFile;
		FontStyle fontStyle = FontStyle::Normal;
		FontStyle defaultTxtFontStyle = FontStyle::Normal;
		float line_height = 0.f;
		EdgeType edgeType{};
		uint32_t maxlines = 1; // NOTE: preserved for attribute compatibility
		// Max text length in CODE POINTS. Genuinely enforced per-codepoint
		uint32_t maxTextSize = 1024000;
		TextWrapStyle textWrapStyle = TextWrapStyle::MAX_CHARS_PER_LN;
		Gravity gravity = Gravity::Left;
		float cornerRadius = 0.f;
		int customFontstyle = 0x00;
		float lineSpacing = 0.f;
		float outline = 0.f;
		bool highlightOnHover = false;
		SDL_Color outlineColor = { 0x00, 0x00, 0x00, 0x00 };
		SDL_Color cursorColor = { 0x00, 0x00, 0x00, 0xff };
		// selection highlight color. Defaults to a translucent blue, the near-universal convention for text selection.
		SDL_Color selectionColor = { 51, 153, 255, 110 };
		SDL_Color onHoverOutlineColor = { 0x00, 0x00, 0x00, 0x00 };
		SDL_Color onHoverBgColor = { 0x00, 0x00, 0x00, 0x00 };
		SDL_Color onHoverTxtColor = { 0x00, 0x00, 0x00, 0x00 };
		uint32_t cursorSpeed = 500;
	};

	class EditBox : public Context, public IView {
	public:
		EditBox() = default;
		int32_t id = (-1);

		EditBox& registerOnTextInputCallback(std::function<void(EditBox&)> cb) {
			on_text_input_callback_ = std::move(cb);
			return *this;
		}

		EditBox& onTextInputFilter(std::function<void(EditBox&, std::string&)> cb) {
			on_text_input_filter_callback_ = std::move(cb);
			return *this;
		}

		// onKeyPressEnter Fires on SDL_SCANCODE_RETURN / RETURN2 / KP_ENTER while focused.
		EditBox& registerOnEnterCallback(std::function<void(EditBox&)> cb) {
			on_enter_callback_ = std::move(cb);
			return *this;
		}

		EditBox& Build(Context* ctx, EditBoxAttributes& attr) {
			Context::setContext(ctx);
			adaptive_vsync_handler_.setAdaptiveVsync(adaptiveVsync);
			IView::type = "editbox";
			attr_ = attr;
			bounds = attr.rect;

			text_rect_ = {
				bounds.x + to_cust(attr.margin.x, bounds.w),
				bounds.y + to_cust(attr.margin.y, bounds.h),
				to_cust(attr.margin.w, bounds.w),
				to_cust(attr.margin.h, bounds.h),
			};

			line_height_ = std::clamp(attr.line_height, 0.f, 255.f);
			if (line_height_ == 0.f) line_height_ = text_rect_.h;

			corner_radius_ = attr.cornerRadius;
			text_attributes_ = attr.textAttributes;
			font_attributes_ = { attr.fontFile, attr.fontStyle, line_height_ };
			max_text_size_ = attr.maxTextSize;
			selection_color_ = attr.selectionColor;
			placeholder_text_ = attr.placeholderTextAttributes.text;

			cursor_.setContext(getContext());
			cursor_.setColor(attr.cursorColor);
			cursor_.setBlinkTm(attr.cursorSpeed);
			cursor_.setRect({ text_rect_.x, text_rect_.y, to_cust(10.f, text_rect_.h), text_rect_.h });

			TTF_Font* resolved_font = nullptr;
			if (font_attributes_.font_file.empty()) {
				Fonts[attr.mem_font]->font_size = font_attributes_.font_size;
				font_attributes_.font_file = Fonts[attr.mem_font]->font_name;
				resolved_font = FontSystem::Get().getFont(*Fonts[attr.mem_font]);
			}
			else {
				resolved_font = FontSystem::Get().getFont(font_attributes_.font_file, font_attributes_.font_size);
			}
			if (!resolved_font) {
				// CharStore does its own
				// independent lookup for each glyph and would likely also fail
				// the same way, but this is the first, cheapest point to catch
				// and report a misconfigured font instead of rendering nothing
				// with no explanation.
				GLogger.Log(Logger::Level::Error, "EditBox::Build: font resolution failed for the main text font -- typed text will not render.");
			}
			char_store_.setProps(getContext(), font_attributes_, attr.customFontstyle);

			buildPlaceholderTexture(attr);

			outline_color_ = attr.outlineColor;
			on_hover_outline_color_ = attr.onHoverOutlineColor;
			highlight_on_hover_ = attr.highlightOnHover;
			on_hover_bg_color_ = attr.onHoverBgColor;
			outline_rect_.Build(this, bounds, attr.outline, corner_radius_, text_attributes_.bg_color, attr.outlineColor);

			model_.clear();
			layout_dirty_ = true;
			refreshVisual();
			return *this;
		}

		void draw() override {
			outline_rect_.draw();
			if (!has_focus_) {
				if (model_.text().empty())
					RenderTexture(renderer, dfl_txt_texture_.get(), nullptr, &dfl_txt_rect_);
				else
					RenderTexture(renderer, txt_texture_.get(), nullptr, &final_text_rect_);
			}
			else {
				if (!model_.text().empty())
					RenderTexture(renderer, txt_texture_.get(), nullptr, &final_text_rect_);
				else
					RenderTexture(renderer, dfl_txt_texture_.get(), nullptr, &dfl_txt_rect_);
				cursor_.Draw();
			}

			if (on_focus_view_ != nullptr && has_focus_)
				on_focus_view_->draw();
		}

		SDL_FRect& getRect() { return outline_rect_.rect; }

		// Exposes the blinking caret's current on-screen rect. Useful for
		// consumers that need to anchor something to the text cursor (an
		// autocomplete popup, an inline validation tooltip, etc.), and used by
		// this file's regression tests to verify cursor positioning directly.
		SDL_FRect getCursorRect() const { return cursor_.getRect(); }

		std::string getTextOrDefault() const {
			return model_.text().empty() ? placeholder_text_ : model_.text();
		}
		std::string getText() const { return model_.text(); }

		EditBox& setOnFocusView(IView* v) { on_focus_view_ = v; return *this; }
		IView* getOnFocusView() { return on_focus_view_; }
		bool isActive() const { return has_focus_; }

		EditBox& killFocus() {
			if (SDL_TextInputActive(window)) SDL_StopTextInput(window);
			adaptive_vsync_handler_.stopRedrawSession();
			has_focus_ = false;
			is_dragging_ = false;
			composition_text_.clear();
			if (highlight_on_hover_) {
				outline_rect_.outline_color = outline_color_;
				outline_rect_.color = text_attributes_.bg_color;
			}
			if (on_focus_view_ != nullptr) on_focus_view_->hide();
			return *this;
		}

		// Replaces the whole text and puts the cursor at the end
		EditBox& clearAndSetText(const std::string& newText) {
			std::string clipped = clampToMaxCodepoints(newText, max_text_size_);
			model_.setText(clipped);
			layout_dirty_ = true;
			refreshVisual();
			return *this;
		}

		bool handleEvent() override {
			if (on_focus_view_ && has_focus_)
				if (on_focus_view_->handleEvent()) return true;

			bool result = false;
			switch (event->type) {
			case EVT_RENDER_TARGETS_RESET:
				resolveTextureReset();
				result = false;
				break;

			case EVT_WPSC:
				onResize();
				result = true;
				break;

			case EVT_FINGER_DOWN:
				result = handlePointerDown(event->tfinger.x * DisplayInfo::Get().RenderW,
					event->tfinger.y * DisplayInfo::Get().RenderH);
				break;

			case EVT_MOUSE_BTN_DOWN:
				result = handlePointerDown(static_cast<float>(event->button.x), static_cast<float>(event->button.y));
				break;

			case EVT_MOUSE_MOTION:
				handlePointerMotion(static_cast<float>(event->motion.x), static_cast<float>(event->motion.y));
				break;

			case EVT_FINGER_UP:
			case SDL_EVENT_MOUSE_BUTTON_UP:
				is_dragging_ = false;
				break;

			case SDL_EVENT_KEY_DOWN:
				if (has_focus_) result = handleKeyDown();
				break;

			case SDL_EVENT_TEXT_INPUT:
				if (has_focus_) result = handleTextInput();
				break;

			case SDL_EVENT_TEXT_EDITING:
				// Basic IME preedit support -- see the top-of-file note. Renders
				// the composition string with an underline at the cursor.
				if (has_focus_) result = handleTextEditing();
				break;

			default:
				break;
			}
			return result;
		}

		EditBox& setOnHoverOutlineColor(const SDL_Color& c) { outline_rect_.outline_color = c; on_hover_outline_color_ = c; return *this; }
		EditBox& setOutlineColor(const SDL_Color& c) { outline_rect_.outline_color = c; outline_color_ = c; return *this; }

	protected:
		template <typename T>
		bool onClick(T x, T y, unsigned short axis = 0) {
			if (axis == 0) [[likely]] {
				if (x < pv->getRealX() + bounds.x || x >(pv->getRealX() + bounds.x + bounds.w) ||
					y < pv->getRealY() + bounds.y || y >(pv->getRealY() + bounds.y + bounds.h))
					return false;
			}
			else if (axis == 1) {
				if (x < bounds.x || x >(bounds.x + bounds.w)) return false;
			}
			else if (axis == 2) {
				if (y < bounds.y || y >(bounds.y + bounds.h)) return false;
			}
			return true;
		}

	private:
		// fires when the backend has invalidated existing render-target textures
		// (common on Android when the app is backgrounded/foregrounded, or after
		// a GPU device reset). txt_texture_ and dfl_txt_texture_ are both
		// SDL_TEXTUREACCESS_TARGET textures, so their contents -- and on some
		// backends the texture objects themselves -- can no longer be trusted
		// after this event. This forces both to be fully rebuilt:
		//   - the placeholder texture is regenerated from scratch (it's static
		//     content, cheap to redo)
		//   - the text texture is forced through renderVisibleText()'s
		//     (re)allocate path by resetting the cached size sentinel, then
		//     redrawn from the model's current state
		// Deliberately does NOT touch bounds/text_rect_/geometry
		void resolveTextureReset() {
			buildPlaceholderTexture(attr_);
			txt_texture_last_w_ = -1.f;
			txt_texture_last_h_ = -1.f;
			layout_dirty_ = true;
			refreshVisual();
		}

		// =========================================================================
		// Event handlers
		// =========================================================================

		bool handlePointerDown(float x, float y) {
			if (onClick(x, y)) {
				bool already_focused = has_focus_;
				has_focus_ = true;
				if (!already_focused) {
					adaptive_vsync_handler_.startRedrawSession();
					SDL_StartTextInput(window);
					if (on_focus_view_) on_focus_view_->show();
				}
				size_t cp = hitTestToCodepointIndex(x);
				model_.moveTo(cp);
				is_dragging_ = true;
				drag_anchor_cp_ = cp;
				refreshVisual();
				return true;
			}
			if (has_focus_) killFocus();
			return false;
		}

		void handlePointerMotion(float x, float /*y*/) {
			if (!is_dragging_ || !has_focus_) return;
			size_t cp = hitTestToCodepointIndex(x);
			model_.moveTo(drag_anchor_cp_);          // re-anchor
			model_.extendSelectionTo(cp);            // extend from anchor to current x
			refreshVisual();
		}

		bool handleKeyDown() {
			const SDL_Scancode sc = event->key.scancode;
			const bool shift = (event->key.mod & SDL_KMOD_SHIFT) != 0;
			const bool cmd = (event->key.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0;

			cursor_.m_start = SDL_GetTicks(); // any key activity resets the blink phase

			if (sc == SDL_SCANCODE_AC_BACK) {
				killFocus();
				return true;
			}

			bool changed_text = false;
			bool handled = true;

			if (cmd && sc == SDL_SCANCODE_A) {
				model_.selectAll();
			}
			else if (cmd && sc == SDL_SCANCODE_C) {
				copySelectionToClipboard();
			}
			else if (cmd && sc == SDL_SCANCODE_X) {
				if (model_.hasSelection()) {
					copySelectionToClipboard();
					model_.deleteSelection();
					changed_text = true;
				}
			}
			else if (cmd && sc == SDL_SCANCODE_V) {
				changed_text = pasteFromClipboard();
			}
			else if (sc == SDL_SCANCODE_LEFT) {
				model_.moveLeft(shift);
			}
			else if (sc == SDL_SCANCODE_RIGHT) {
				model_.moveRight(shift);
			}
			else if (sc == SDL_SCANCODE_HOME) {
				model_.moveHome(shift);
			}
			else if (sc == SDL_SCANCODE_END) {
				model_.moveEnd(shift);
			}
			else if (sc == SDL_SCANCODE_BACKSPACE) {
				changed_text = model_.backspace();
			}
			else if (sc == SDL_SCANCODE_DELETE) {
				changed_text = model_.deleteForward();
			}
			else if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_RETURN2 || sc == SDL_SCANCODE_KP_ENTER) {
				if (on_enter_callback_) on_enter_callback_(*this);
			}
			else {
				handled = false;
			}

			if (handled) {
				layout_dirty_ = changed_text; // only re-fetch glyphs if text actually changed
				refreshVisual();
				if (changed_text && on_text_input_callback_) on_text_input_callback_(*this);
			}
			return handled;
		}

		bool handleTextInput() {
			std::string incoming = event->text.text; // SDL3: already UTF-8
			if (on_text_input_filter_callback_) on_text_input_filter_callback_(*this, incoming);
			if (incoming.empty()) return true;

			composition_text_.clear(); // committed text supersedes any preedit

			size_t room = (max_text_size_ > model_.buffer().codepointCount())
				? (max_text_size_ - model_.buffer().codepointCount()) : 0;
			std::string clipped = clampToMaxCodepoints(incoming, room + selectionCodepointCount());
			if (clipped.empty()) return true;

			model_.insertAtCursor(clipped);
			layout_dirty_ = true;
			refreshVisual();
			if (on_text_input_callback_) on_text_input_callback_(*this);
			return true;
		}

		bool handleTextEditing() {
			// event->edit.text is the current IME preedit string. An empty
			// string means composition ended/was cancelled without a commit.
			composition_text_ = event->edit.text ? event->edit.text : "";
			layout_dirty_ = true; // preedit is rendered as part of the visible line
			refreshVisual();
			return true;
		}

		void onResize() {
			float next_x = DisplayInfo::Get().toUpdatedWidth(bounds.x);
			float next_y = DisplayInfo::Get().toUpdatedHeight(bounds.y);
			float next_w = DisplayInfo::Get().toUpdatedWidth(bounds.w);
			float next_h = DisplayInfo::Get().toUpdatedHeight(bounds.h);
			if (std::isfinite(next_x) && std::isfinite(next_y) &&
				std::isfinite(next_w) && std::isfinite(next_h) &&
				next_w > 0.f && next_h > 0.f) {

				outline_rect_.handleEvent();

				bounds = { next_x, next_y, next_w, next_h };

				text_rect_ = {
				DisplayInfo::Get().toUpdatedWidth(text_rect_.x),
				DisplayInfo::Get().toUpdatedHeight(text_rect_.y),
				DisplayInfo::Get().toUpdatedWidth(text_rect_.w),
				DisplayInfo::Get().toUpdatedHeight(text_rect_.h),
				};
				dfl_txt_rect_ = {
					DisplayInfo::Get().toUpdatedWidth(dfl_txt_rect_.x),
					DisplayInfo::Get().toUpdatedHeight(dfl_txt_rect_.y),
					DisplayInfo::Get().toUpdatedWidth(dfl_txt_rect_.w),
					DisplayInfo::Get().toUpdatedHeight(dfl_txt_rect_.h),
				};
				final_text_rect_ = {
					DisplayInfo::Get().toUpdatedWidth(final_text_rect_.x),
					DisplayInfo::Get().toUpdatedHeight(final_text_rect_.y),
					DisplayInfo::Get().toUpdatedWidth(final_text_rect_.w),
					DisplayInfo::Get().toUpdatedHeight(final_text_rect_.h),
				};
				auto r = cursor_.getRect();
				cursor_.setRect({
					DisplayInfo::Get().toUpdatedWidth(r.x), DisplayInfo::Get().toUpdatedHeight(r.y),
					DisplayInfo::Get().toUpdatedWidth(r.w), DisplayInfo::Get().toUpdatedHeight(r.h),
					});
			}

			layout_dirty_ = true;
			refreshVisual();
		}

		// =========================================================================
		// Clipboard
		// =========================================================================

		void copySelectionToClipboard() {
			std::string sel = model_.selectedText();
			if (!sel.empty()) SDL_SetClipboardText(sel.c_str());
		}

		// Returns true if anything actually changed (so callers know whether to
		// fire on_text_input_callback_).
		bool pasteFromClipboard() {
			if (!SDL_HasClipboardText()) return false;
			char* clip = SDL_GetClipboardText();
			if (!clip) return false;
			std::string text = clip;
			SDL_free(clip);
			if (text.empty()) return false;

			if (on_text_input_filter_callback_) on_text_input_filter_callback_(*this, text);
			if (text.empty()) return false;

			size_t existing_after_selection = model_.buffer().codepointCount() - selectionCodepointCount();
			size_t room = (max_text_size_ > existing_after_selection) ? (max_text_size_ - existing_after_selection) : 0;
			std::string clipped = clampToMaxCodepoints(text, room);
			if (clipped.empty()) return false;

			model_.insertAtCursor(clipped);
			layout_dirty_ = true;
			return true;
		}

		size_t selectionCodepointCount() const {
			auto sel = model_.selection();
			return sel ? (sel->second - sel->first) : 0;
		}

		static std::string clampToMaxCodepoints(const std::string& s, size_t max_cp) {
			if (max_cp == 0) return {};
			Volt::Utf8TextBuffer tmp(s);
			if (tmp.codepointCount() <= max_cp) return s;
			return s.substr(0, tmp.byteOffsetOf(max_cp));
		}

		// =========================================================================
		// Layout + rendering
		// =========================================================================

		// Converts an absolute screen X (as delivered by an SDL event) into the
		// nearest codepoint boundary index. This is the ONLY place absolute
		// event coordinates get converted into the model's local coordinate
		// space -- everything downstream of this is purely local pixels.
		size_t hitTestToCodepointIndex(float abs_screen_x) const {
			float local_x = (abs_screen_x - screenXOfTextOrigin()) + scroll_offset_px_;
			if (cp_x_offsets_.empty()) return 0;
			// Find the codepoint whose glyph center is closest to local_x.
			for (size_t i = 0; i + 1 < cp_x_offsets_.size(); ++i) {
				float mid = (cp_x_offsets_[i] + cp_x_offsets_[i + 1]) / 2.f;
				if (local_x < mid) return i;
			}
			return cp_x_offsets_.size() - 1; // past the last glyph -> end of text
		}

		// The one absolute-coordinate anchor everything else is measured from.
		float screenXOfTextOrigin() const {
			return pv->getRealX() + text_rect_.x;
		}

		// Recomputes per-codepoint glyph widths/offsets. Only called when the
		// text content itself changed (layout_dirty_), not on every cursor move.
		void rebuildLayout() {
			cp_advances_.clear();
			cp_x_offsets_.clear();

			std::string display_text = model_.text();
			size_t ime_insert_byte = std::string::npos;
			if (!composition_text_.empty()) {
				// Splice the IME preedit string into the display text at the
				// cursor position, purely for layout/rendering purposes -- it is
				// NOT written into model_ until SDL_EVENT_TEXT_INPUT commits it.
				ime_insert_byte = model_.buffer().byteOffsetOf(model_.cursor());
				display_text.insert(ime_insert_byte, composition_text_);
			}

			float x = 0.f;
			size_t byte_pos = 0;
			while (byte_pos < display_text.size()) {
				size_t len = Volt::Utf8TextBuffer::codepointByteLength(display_text, byte_pos);
				std::string glyph_bytes = display_text.substr(byte_pos, len);
				cp_x_offsets_.push_back(x);

				SDL_Texture* tex = char_store_.getChar(glyph_bytes, text_attributes_.text_color);
				float w = 0.f, h = 0.f;
				if (tex) SDL_GetTextureSize(tex, &w, &h);
				cp_advances_.push_back(w > 0.f ? w : (line_height_ / 2.f)); // fallback width for a missing glyph
				x += cp_advances_.back();
				byte_pos += len;
			}
			cp_x_offsets_.push_back(x); // sentinel: end-of-text position

			ime_display_byte_offset_ = ime_insert_byte;
			layout_dirty_ = false;
		}

		// Recomputes scroll offset so the cursor stays visible, then re-renders
		// the visible glyph slice + selection highlight into txt_texture_, and
		// repositions the blinking Cursor. Called after every edit or cursor move.
		void refreshVisual() {
			if (layout_dirty_) rebuildLayout();

			size_t cursor_display_cp = model_.cursor();
			if (ime_display_byte_offset_ != std::string::npos) {
				// While composing, the "cursor" for scroll/positioning purposes
				// is treated as sitting after the preedit string, matching how
				// most platforms draw the caret during IME composition.
				Volt::Utf8TextBuffer tmp(composition_text_);
				cursor_display_cp = model_.cursor() + tmp.codepointCount();
			}
			float cursor_x = (cursor_display_cp < cp_x_offsets_.size())
				? cp_x_offsets_[cursor_display_cp] : (cp_x_offsets_.empty() ? 0.f : cp_x_offsets_.back());

			// 1. Push offset forward or backward if cursor goes out of visible bounds
			if (cursor_x < scroll_offset_px_) scroll_offset_px_ = cursor_x;
			if (cursor_x > scroll_offset_px_ + text_rect_.w) scroll_offset_px_ = cursor_x - text_rect_.w;

			// 2. Pull offset back if text shrinks to prevent stranding text off-screen
			float total_text_width = cp_x_offsets_.empty() ? 0.f : cp_x_offsets_.back();
			float max_scroll = std::max(0.f, total_text_width - text_rect_.w);
			if (scroll_offset_px_ > max_scroll) {
				scroll_offset_px_ = max_scroll;
			}

			// 3. Final safety clamp
			scroll_offset_px_ = std::max(0.f, scroll_offset_px_);

			renderVisibleText();

			cursor_.setPosX(screenXOfTextOrigin() + (cursor_x - scroll_offset_px_));
		}

		void renderVisibleText() {
			if (cp_advances_.empty()) return; // nothing to render; draw() falls back to placeholder

			if (!txt_texture_ || txt_texture_last_w_ != text_rect_.w || txt_texture_last_h_ != text_rect_.h) {
				txt_texture_ = CreateSharedTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
					SDL_TEXTUREACCESS_TARGET, static_cast<int>(text_rect_.w), static_cast<int>(text_rect_.h));
				SDL_SetTextureBlendMode(txt_texture_.get(), SDL_BLENDMODE_BLEND);
				txt_texture_last_w_ = text_rect_.w;
				txt_texture_last_h_ = text_rect_.h;
			}

			CacheRenderTarget crt_(renderer);
			SDL_SetRenderTarget(renderer, txt_texture_.get());
			RenderClear(renderer, 0, 0, 0, 0);

			// Selection highlight, drawn first so glyphs render on top of it.
			auto sel = model_.selection();
			if (sel) {
				float x0 = cp_x_offsets_[sel->first] - scroll_offset_px_;
				float x1 = cp_x_offsets_[sel->second] - scroll_offset_px_;
				SDL_FRect hl{ x0, 0.f, x1 - x0, text_rect_.h };
				CacheRenderColor(renderer);
				SDL_SetRenderDrawColor(renderer, selection_color_.r, selection_color_.g, selection_color_.b, selection_color_.a);
				SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
				SDL_RenderFillRect(renderer, &hl);
				RestoreCachedRenderColor(renderer);
			}

			std::string display_text = model_.text();
			if (!composition_text_.empty() && ime_display_byte_offset_ != std::string::npos) {
				display_text.insert(ime_display_byte_offset_, composition_text_);
			}

			size_t byte_pos = 0;
			for (size_t i = 0; i < cp_advances_.size(); ++i) {
				float glyph_x = cp_x_offsets_[i] - scroll_offset_px_;
				size_t len = Volt::Utf8TextBuffer::codepointByteLength(display_text, byte_pos);
				if (glyph_x + cp_advances_[i] >= 0.f && glyph_x <= text_rect_.w) {
					std::string glyph_bytes = display_text.substr(byte_pos, len);
					SDL_Texture* tex = char_store_.getChar(glyph_bytes, text_attributes_.text_color);
					if (tex) {
						SDL_FRect dst{ glyph_x, 0.f, cp_advances_[i], text_rect_.h };
						RenderTexture(renderer, tex, nullptr, &dst);
					}
				}
				byte_pos += len;
			}

			// Basic IME preedit underline -- see top-of-file note on IME support.
			if (ime_display_byte_offset_ != std::string::npos && !composition_text_.empty()) {
				Volt::Utf8TextBuffer comp(composition_text_);
				size_t start_cp = model_.cursor();
				size_t end_cp = start_cp + comp.codepointCount();
				if (end_cp < cp_x_offsets_.size()) {
					float ux0 = cp_x_offsets_[start_cp] - scroll_offset_px_;
					float ux1 = cp_x_offsets_[end_cp] - scroll_offset_px_;
					SDL_FRect underline{ ux0, text_rect_.h - 2.f, ux1 - ux0, 2.f };
					CacheRenderColor(renderer);
					SDL_SetRenderDrawColor(renderer, text_attributes_.text_color.r, text_attributes_.text_color.g,
						text_attributes_.text_color.b, text_attributes_.text_color.a);
					SDL_RenderFillRect(renderer, &underline);
					RestoreCachedRenderColor(renderer);
				}
			}

			crt_.release(renderer);
			final_text_rect_ = text_rect_;
		}

		void buildPlaceholderTexture(const EditBoxAttributes& attr) {
			if (attr.placeholderTextAttributes.text.empty()) return;

			dfl_txt_rect_ = {
				bounds.x + to_cust(attr.placeholderRect.x, bounds.w),
				bounds.y + to_cust(attr.placeholderRect.y, bounds.h),
				to_cust(attr.placeholderRect.w, bounds.w),
				to_cust(attr.placeholderRect.h, bounds.h),
			};

			FontAttributes plhFontAttr{ attr.placeholderFontFile, attr.defaultTxtFontStyle, dfl_txt_rect_.h };
			TTF_Font* placeholder_font = nullptr;
			if (plhFontAttr.font_file.empty()) {
				Fonts[attr.placeholder_mem_font]->font_size = plhFontAttr.font_size;
				plhFontAttr.font_file = Fonts[attr.placeholder_mem_font]->font_name;
				placeholder_font = FontSystem::Get().getFont(*Fonts[attr.placeholder_mem_font]);
			}
			else {
				placeholder_font = FontSystem::Get().getFont(plhFontAttr.font_file, plhFontAttr.font_size);
			}
			if (!placeholder_font) {
				GLogger.Log(Logger::Level::Error, "EditBox: placeholder font resolution failed -- placeholder text will not render.");
				return;
			}

			FontSystem::Get().setFontAttributes({ plhFontAttr.font_file.c_str(), plhFontAttr.font_style, plhFontAttr.font_size }, 0);
			auto textTex = FontSystem::Get().genTextTextureUnique(renderer, attr.placeholderTextAttributes.text.c_str(),
				attr.placeholderTextAttributes.text_color);
			if (!textTex.has_value()) return;

			float test_w = 0.f, test_h = 0.f;
			SDL_GetTextureSize(textTex.value().get(), &test_w, &test_h);
			if (static_cast<int>(test_w) < dfl_txt_rect_.w) dfl_txt_rect_.w = test_w;
			if (test_h >= dfl_txt_rect_.h) {
				// Restored: the original nudged the rect up by the font's
				// descent when the glyph texture is taller than the configured
				// box height, so descenders (g, y, p, j) aren't clipped against
				// the bottom edge. This rewrite had dropped the adjustment
				// entirely, which can make placeholder text render partially
				// or fully outside dfl_txt_rect_'s visible area for fonts/sizes
				// with a large descent relative to placeholderRect.h.
				dfl_txt_rect_.y += static_cast<float>(TTF_GetFontDescent(placeholder_font));
				dfl_txt_rect_.h = test_h;
			}

			SDL_FRect src{ 0.f, 0.f, dfl_txt_rect_.w, test_h };
			SDL_FRect dst{ 0.f, 0.f, dfl_txt_rect_.w, dfl_txt_rect_.h };

			dfl_txt_texture_ = CreateSharedTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
				SDL_TEXTUREACCESS_TARGET, static_cast<int>(dfl_txt_rect_.w), static_cast<int>(dfl_txt_rect_.h));
			CacheRenderTarget crt_(renderer);
			SDL_SetRenderTarget(renderer, dfl_txt_texture_.get());
			SDL_SetTextureBlendMode(dfl_txt_texture_.get(), SDL_BLENDMODE_BLEND);
			RenderClear(renderer, 0, 0, 0, 0);
			RenderTexture(renderer, textTex.value().get(), &src, &dst);
			crt_.release(renderer);
		}

		// =========================================================================
		// State
		// =========================================================================

		Volt::EditCursorModel model_;
		uint32_t max_text_size_ = 100;

		// Layout cache -- rebuilt only when text changes (layout_dirty_).
		std::vector<float> cp_advances_;
		std::vector<float> cp_x_offsets_; // size == codepointCount()+1, includes end sentinel
		bool layout_dirty_ = true;
		float scroll_offset_px_ = 0.f;

		// IME preedit (basic support -- see top-of-file note).
		std::string composition_text_;
		size_t ime_display_byte_offset_ = std::string::npos;

		// Selection drag tracking.
		bool is_dragging_ = false;
		size_t drag_anchor_cp_ = 0;

		IView* on_focus_view_ = nullptr;
		CharStore char_store_{};
		float corner_radius_ = 0.f, line_height_ = 0.f;
		SharedTexture txt_texture_;
		float txt_texture_last_w_ = -1.f, txt_texture_last_h_ = -1.f;
		SharedTexture dfl_txt_texture_;
		std::function<void(EditBox&)> on_text_input_callback_;
		std::function<void(EditBox&)> on_enter_callback_;
		std::function<void(EditBox&, std::string&)> on_text_input_filter_callback_;
		TextAttributes text_attributes_;
		FontAttributes font_attributes_;
		SDL_Color selection_color_{};
		EditBoxAttributes attr_;

		SDL_FRect dfl_txt_rect_{};
		SDL_FRect text_rect_{};
		SDL_FRect final_text_rect_{};
		std::string placeholder_text_;
		RectOutline outline_rect_;
		bool has_focus_ = false;
		Cursor cursor_;
		AdaptiveVsyncHandler adaptive_vsync_handler_;
		bool highlight_on_hover_ = false;
		SDL_Color on_hover_outline_color_{ 0,0,0,0 };
		SDL_Color on_hover_bg_color_{ 0,0,0,0 };
		SDL_Color outline_color_{ 0,0,0,0 };
	};
//} // namespace
