#pragma once
// EditCore.hpp
// UTF-8 aware text buffer + cursor/selection model.

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Volt {
	// Utf8TextBuffer -- a std::string plus a codepoint->byte-offset index.
	class Utf8TextBuffer {
	public:
		Utf8TextBuffer() { rebuild(); }
		explicit Utf8TextBuffer(std::string initial) : text_(std::move(initial)) { rebuild(); }

		[[nodiscard]] const std::string& text() const { return text_; }
		[[nodiscard]] size_t codepointCount() const {
			return offsets_.empty() ? 0 : offsets_.size() - 1;
		}

		// Byte offset where codepoint `cp_index` begins. cp_index may equal
		// codepointCount() (the "end" position, i.e. text_.size()).
		[[nodiscard]] size_t byteOffsetOf(size_t cp_index) const {
			size_t clamped = std::min(cp_index, codepointCount());
			return offsets_[clamped];
		}

		// Inserts utf8_text at codepoint index `at` (0..codepointCount()).
		// utf8_text may itself contain multiple codepoints (e.g. a paste, or
		// an IME composition commit) -- every one of them gets its own offset
		// entry. Returns how many codepoints were inserted, so callers can
		// advance the cursor by the right amount.
		size_t insert(size_t at, const std::string& utf8_text) {
			if (utf8_text.empty()) return 0;
			size_t byte_at = byteOffsetOf(at);
			text_.insert(byte_at, utf8_text);
			rebuild();
			size_t count = 0, pos = 0;
			while (pos < utf8_text.size()) {
				pos += codepointByteLength(utf8_text, pos);
				++count;
			}
			return count;
		}

		// Erases codepoints in [from, to). Indices are clamped and swapped
		// if given in the wrong order, so callers never need to pre-sort a
		// selection range themselves.
		void eraseRange(size_t from, size_t to) {
			size_t cc = codepointCount();
			from = std::min(from, cc);
			to = std::min(to, cc);
			if (from > to) std::swap(from, to);
			if (from == to) return;
			size_t byte_from = byteOffsetOf(from);
			size_t byte_to = byteOffsetOf(to);
			text_.erase(byte_from, byte_to - byte_from);
			rebuild();
		}

		void clear() { text_.clear(); rebuild(); }
		void set(std::string new_text) { text_ = std::move(new_text); rebuild(); }

		// Byte length of the UTF-8 codepoint starting at text[byte_offset].
		// Defensive: an invalid lead byte (stray continuation byte, or a
		// byte that claims a multi-byte sequence that would run past the end
		// of the string -- e.g. a paste truncated mid-codepoint) is treated
		// as a 1-byte codepoint. This means malformed input can never desync
		// the offset table, throw, or read out of bounds -- worst case it
		// renders that one byte oddly, which is recoverable by further
		// editing, instead of corrupting the whole buffer or crashing.
		static size_t codepointByteLength(const std::string& text, size_t byte_offset) {
			if (byte_offset >= text.size()) return 0;
			unsigned char c = static_cast<unsigned char>(text[byte_offset]);
			size_t len;
			if      ((c & 0x80) == 0x00) len = 1; // 0xxxxxxx
			else if ((c & 0xE0) == 0xC0) len = 2; // 110xxxxx
			else if ((c & 0xF0) == 0xE0) len = 3; // 1110xxxx
			else if ((c & 0xF8) == 0xF0) len = 4; // 11110xxx
			else len = 1;                          // invalid lead byte
			if (byte_offset + len > text.size()) len = text.size() - byte_offset;
			return len;
		}

	private:
		void rebuild() {
			offsets_.clear();
			size_t pos = 0;
			while (pos < text_.size()) {
				offsets_.push_back(pos);
				pos += codepointByteLength(text_, pos);
			}
			offsets_.push_back(text_.size());
		}

		std::string text_;
		std::vector<size_t> offsets_{0};
	};

	// =========================================================================
	// EditCursorModel -- cursor position + selection + edit operations.
	// =========================================================================
	// Everything a text field needs to do in response to keyboard/mouse input,
	// expressed purely in terms of codepoint indices. No pixels, no SDL, no
	// rendering. The EditBox class translates SDL events into calls here, and
	// translates codepoint positions back into pixel positions for drawing --
	// this class never needs to know about either.
	class EditCursorModel {
	public:
		[[nodiscard]] const Utf8TextBuffer& buffer() const { return buffer_; }
		[[nodiscard]] const std::string& text() const { return buffer_.text(); }
		[[nodiscard]] size_t cursor() const { return cursor_cp_; }

		// Normalized selection range [lo, hi), or nullopt if nothing is selected.
		[[nodiscard]] std::optional<std::pair<size_t, size_t>> selection() const {
			if (!anchor_cp_.has_value() || *anchor_cp_ == cursor_cp_) return std::nullopt;
			size_t lo = std::min(*anchor_cp_, cursor_cp_);
			size_t hi = std::max(*anchor_cp_, cursor_cp_);
			return std::make_pair(lo, hi);
		}
		[[nodiscard]] bool hasSelection() const { return selection().has_value(); }

		[[nodiscard]] std::string selectedText() const {
			auto sel = selection();
			if (!sel) return {};
			size_t byte_lo = buffer_.byteOffsetOf(sel->first);
			size_t byte_hi = buffer_.byteOffsetOf(sel->second);
			return buffer_.text().substr(byte_lo, byte_hi - byte_lo);
		}

		void setText(std::string new_text) {
			buffer_.set(std::move(new_text));
			cursor_cp_ = buffer_.codepointCount();
			anchor_cp_.reset();
		}

		void clear() {
			buffer_.clear();
			cursor_cp_ = 0;
			anchor_cp_.reset();
		}

		// Types `utf8_text` at the cursor. If a selection is active, it is
		// replaced (standard "typing over a selection" behavior). Returns
		// the number of codepoints that ended up inserted (useful if a
		// caller wants to know how much the visible text grew).
		size_t insertAtCursor(const std::string& utf8_text) {
			if (hasSelection()) deleteSelection();
			size_t inserted = buffer_.insert(cursor_cp_, utf8_text);
			cursor_cp_ += inserted;
			anchor_cp_.reset();
			return inserted;
		}

		// Backspace: deletes the selection if present, else the one codepoint
		// before the cursor. Returns true if anything was actually deleted
		// (callers use this to decide whether to fire a "text changed" event).
		bool backspace() {
			if (hasSelection()) { deleteSelection(); return true; }
			if (cursor_cp_ == 0) return false;
			buffer_.eraseRange(cursor_cp_ - 1, cursor_cp_);
			--cursor_cp_;
			return true;
		}

		// Forward-delete (the Delete key): deletes the selection if present,
		// else the one codepoint AFTER the cursor, cursor does not move.
		// This is new behavior -- the original EditBox had no forward-delete
		// at all, only backspace-from-the-end.
		bool deleteForward() {
			if (hasSelection()) { deleteSelection(); return true; }
			if (cursor_cp_ >= buffer_.codepointCount()) return false;
			buffer_.eraseRange(cursor_cp_, cursor_cp_ + 1);
			return true;
		}

		void deleteSelection() {
			auto sel = selection();
			if (!sel) return;
			buffer_.eraseRange(sel->first, sel->second);
			cursor_cp_ = sel->first;
			anchor_cp_.reset();
		}

		// Cursor movement. `extend_selection` corresponds to the Shift key:
		// true grows/shrinks the selection anchor-to-cursor range; false
		// collapses any existing selection and moves the bare cursor.
		//
		// Convention matched to virtually every desktop text field: when a
		// selection exists and you press an unshifted arrow key, the cursor
		// collapses to the near edge of the selection in the direction of
		// travel (Left -> selection start, Right -> selection end) rather
		// than moving one codepoint from wherever the "cursor" happened to
		// be internally. This was simply absent before (no arrow key
		// handling existed at all).
		void moveLeft(bool extend_selection) {
			if (extend_selection) {
				ensureAnchor();
				if (cursor_cp_ > 0) --cursor_cp_;
			} else {
				if (auto sel = selection()) cursor_cp_ = sel->first;
				else if (cursor_cp_ > 0) --cursor_cp_;
				anchor_cp_.reset();
			}
		}

		void moveRight(bool extend_selection) {
			size_t cc = buffer_.codepointCount();
			if (extend_selection) {
				ensureAnchor();
				if (cursor_cp_ < cc) ++cursor_cp_;
			} else {
				if (auto sel = selection()) cursor_cp_ = sel->second;
				else if (cursor_cp_ < cc) ++cursor_cp_;
				anchor_cp_.reset();
			}
		}

		void moveHome(bool extend_selection) {
			if (extend_selection) ensureAnchor();
			else anchor_cp_.reset();
			cursor_cp_ = 0;
		}

		void moveEnd(bool extend_selection) {
			if (extend_selection) ensureAnchor();
			else anchor_cp_.reset();
			cursor_cp_ = buffer_.codepointCount();
		}

		// Moves the bare cursor to an arbitrary codepoint index (used for
		// click-to-position). Clears any selection, matching standard
		// "click somewhere in the text" behavior.
		void moveTo(size_t cp_index) {
			cursor_cp_ = std::min(cp_index, buffer_.codepointCount());
			anchor_cp_.reset();
		}

		// Click-and-drag equivalent: moves the cursor to cp_index while
		// keeping (or starting) a selection anchored at the drag's start.
		void extendSelectionTo(size_t cp_index) {
			ensureAnchor();
			cursor_cp_ = std::min(cp_index, buffer_.codepointCount());
		}

		void selectAll() {
			if (buffer_.codepointCount() == 0) { anchor_cp_.reset(); return; }
			anchor_cp_ = 0;
			cursor_cp_ = buffer_.codepointCount();
		}

	private:
		void ensureAnchor() {
			if (!anchor_cp_.has_value()) anchor_cp_ = cursor_cp_;
		}

		Utf8TextBuffer buffer_;
		size_t cursor_cp_ = 0;
		std::optional<size_t> anchor_cp_; // nullopt == no selection
	};

} // namespace Volt
