/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "gui/widgets/editable.h"
#include "common/rect.h"
#include "common/system.h"
#include "graphics/font.h"
#include "gui/gui-manager.h"

namespace GUI {

EditableWidget::EditableWidget(GuiObject *boss, int x, int y, int w, int h, const U32String &tooltip, uint32 cmd)
	: Widget(boss, x, y, w, h, tooltip), CommandSender(boss), _cmd(cmd) {
	init();
}

EditableWidget::EditableWidget(GuiObject *boss, const String &name, const U32String &tooltip, uint32 cmd)
	: Widget(boss, name, tooltip), CommandSender(boss), _cmd(cmd) {
	init();
}

void EditableWidget::init() {
	_caretVisible = false;
	_caretTime = 0;
	_caretPos = 0;

	_caretInverse = false;

	_editScrollOffset = 0;

	_highlightVisible = false;
	_highlightPos = 0;
	_highlightSize = 0;
	_highlightCharacterCount = 0;

	_align = g_gui.useRTL() ? Graphics::kTextAlignRight : Graphics::kTextAlignLeft;
	_drawAlign = _align;

	_font = ThemeEngine::kFontStyleBold;
	_inversion = ThemeEngine::kTextInversionNone;
}

EditableWidget::~EditableWidget() {
}

void EditableWidget::reflowLayout() {
	Widget::reflowLayout();

	_editScrollOffset = g_gui.getStringWidth(_editString, _font) - getEditRect().width();
	if (_editScrollOffset < 0) {
		_editScrollOffset = 0;
		_drawAlign = _align;
	} else {
		_drawAlign = Graphics::kTextAlignLeft;
	}
}

void EditableWidget::setEditString(const U32String &str) {
	// TODO: We probably should filter the input string here,
	// e.g. using tryInsertChar.
	_editString = str;
	_caretPos = 0;
}

bool EditableWidget::tryInsertChar(byte c, int pos) {
	if ((c >= 32 && c <= 127) || c >= 160) {
		_editString.insertChar(c, pos);
		return true;
	}
	return false;
}

void EditableWidget::handleTickle() {
	uint32 time = g_system->getMillis();
	// prevent blinking, if text is highlighted
	if (_highlightVisible && isEnabled()) {
		_caretTime = 0;
		drawCaret(_caretVisible);
	} else if (_caretTime < time && isEnabled()) {
		_caretTime = time + kCaretBlinkTime;
		drawCaret(_caretVisible);
	}
}

bool EditableWidget::handleKeyDown(Common::KeyState state) {
	bool handled = true;
	bool dirty = false;
	bool forcecaret = false;

	if (!isEnabled())
		return false;

	// First remove caret
	if (_caretVisible)
		drawCaret(true);

	// Remap numeric keypad if NUM lock is *not* active.
	// This code relies on the fact that the various KEYCODE_KP* values are
	// consecutive.
	if (0 == (state.flags & Common::KBD_NUM)
		&& Common::KEYCODE_KP0 <= state.keycode
		&& state.keycode <= Common::KEYCODE_KP_PERIOD) {
		const Common::KeyCode remap[11] = {
			Common::KEYCODE_INSERT, 	// KEYCODE_KP0
			Common::KEYCODE_END,	 	// KEYCODE_KP1
			Common::KEYCODE_DOWN, 		// KEYCODE_KP2
			Common::KEYCODE_PAGEDOWN, 	// KEYCODE_KP3
			Common::KEYCODE_LEFT, 		// KEYCODE_KP4
			Common::KEYCODE_INVALID, 	// KEYCODE_KP5
			Common::KEYCODE_RIGHT,	 	// KEYCODE_KP6
			Common::KEYCODE_HOME,	 	// KEYCODE_KP7
			Common::KEYCODE_UP, 		// KEYCODE_KP8
			Common::KEYCODE_PAGEUP, 	// KEYCODE_KP9
			Common::KEYCODE_DELETE,	 	// KEYCODE_KP_PERIOD
		};
		state.keycode = remap[state.keycode - Common::KEYCODE_KP0];
	}

	switch (state.keycode) {
	case Common::KEYCODE_RETURN:
	case Common::KEYCODE_KP_ENTER:
		initHighlight();
		// confirm edit and exit editmode
		endEditMode();
		dirty = true;
		break;

	case Common::KEYCODE_ESCAPE:
		initHighlight();
		abortEditMode();
		dirty = true;
		break;

	case Common::KEYCODE_BACKSPACE:
		if (_highlightVisible) {
			// remove highlighted characters
			for (uint32 i = _highlightPos + _highlightCharacterCount; i > (uint32)_highlightPos; i--) {
				_editString.deleteChar(i - 1);
				_highlightString.deleteChar(i - 1);
			}
			// set position of caret to end of string
			setCaretPos(_highlightPos);
			initHighlight();
			sendCommand(_cmd, 0);
			dirty = true;
		} else if (_caretPos > 0) {
			_caretPos--;
			_editString.deleteChar(_caretPos);
			dirty = true;
			initHighlight();
			sendCommand(_cmd, 0);
		}
		forcecaret = true;
		break;

	case Common::KEYCODE_DELETE:
		if (_highlightVisible) {
			// remove highlighted characters
			for (uint32 i = _highlightPos + _highlightCharacterCount; i > (uint32)_highlightPos; i--) {
				_editString.deleteChar(i - 1);
				_highlightString.deleteChar(i - 1);
			}
			setCaretPos(_highlightPos);
			initHighlight();
			sendCommand(_cmd, 0);
			dirty = true;
		} else if (_caretPos < (int)_editString.size()) {
			_editString.deleteChar(_caretPos);
			dirty = true;
			initHighlight();
			sendCommand(_cmd, 0);
		}
		forcecaret = true;
		break;

	case Common::KEYCODE_DOWN:
	case Common::KEYCODE_END:
		initHighlight();
		// Move caret to end
		setCaretPos(_editString.size());
		forcecaret = true;
		dirty = true;
		break;

	case Common::KEYCODE_LEFT:
		// select text using shift+Left key
		if (state.flags & Common::KBD_SHIFT) {
			_highlightVisible = true;
			if (_highlightPos > 0) {
				--_highlightPos;
				_highlightCharacterCount++;
				// increase size of highlight
				_highlightSize += g_gui.getCharWidth(_editString[_highlightPos], _font);
				// temporary string that stores selected characters at the same index of editstring
				_highlightString.setChar(_editString[_highlightPos], _highlightPos);
			}
		} else if (_caretPos > 0) {
			// Move caret one left (if possible)
			dirty = setCaretPos(_caretPos - 1);
			initHighlight();
			sendCommand(_cmd, 0);
		}
		forcecaret = true;
		dirty = true;
		break;

	case Common::KEYCODE_RIGHT:
		// Move caret one right (if possible)
		if (_caretPos < (int)_editString.size()) {
			dirty = setCaretPos(_caretPos + 1);
			initHighlight();
			sendCommand(_cmd, 0);
		}
		initHighlight();
		sendCommand(_cmd, 0);
		forcecaret = true;
		dirty = true;
		break;

	case Common::KEYCODE_UP:
	case Common::KEYCODE_HOME:
		initHighlight();
		// Move caret to start
		setCaretPos(0);
		sendCommand(_cmd, 0);
		forcecaret = true;
		dirty = true;
		break;

	case Common::KEYCODE_v:
		if (state.flags & Common::KBD_CTRL) {
			if (g_system->hasTextInClipboard()) {
				initHighlight();
				U32String text = g_system->getTextFromClipboard();
				text.trim();
				for (uint32 i = 0; i < text.size(); ++i) {
					if (tryInsertChar(text[i], _caretPos))
						++_caretPos;
				}
				dirty = true;
			}
		} else {
			defaultKeyDownHandler(state, dirty, forcecaret, handled);
		}
		break;

	case Common::KEYCODE_c:
		if (state.flags & Common::KBD_CTRL) {
			if (!getEditString().empty())
				g_system->setTextInClipboard(getEditString());
		} else {
			defaultKeyDownHandler(state, dirty, forcecaret, handled);
		}
		break;

#ifdef MACOSX
	// Let ctrl-a / ctrl-e move the caret to the start / end of the line.
	//
	// These shortcuts go back a long time for command line programs. As
	// for edit fields in GUIs, they are supported natively on Mac OS X,
	// which is why I enabled these shortcuts there.
	// On other systems (Windows, Gnome), Ctrl-A by default means
	// "select all", which is why I didn't enable the shortcuts there
	// for now, to avoid potential confusion.
	//
	// But since we don't support text selection, and since at least Gnome
	// can be configured to also support ctrl-a and ctrl-e, we may want
	// to extend this code to other targets, maybe even all. I'll leave
	// this to other porters to decide, though.
	case Common::KEYCODE_a:
	case Common::KEYCODE_e:
		if (state.flags & Common::KBD_CTRL) {
			if (state.keycode == Common::KEYCODE_a) {
				// Move caret to start
				dirty = setCaretPos(0);
				forcecaret = true;
			} else if (state.keycode == Common::KEYCODE_e) {
				// Move caret to end
				dirty = setCaretPos(_editString.size());
				forcecaret = true;
			}
			break;
		}
#endif

	default:
		defaultKeyDownHandler(state, dirty, forcecaret, handled);
	}

	if (dirty)
		markAsDirty();

	if (forcecaret)
		makeCaretVisible();

	return handled;
}

void EditableWidget::defaultKeyDownHandler(Common::KeyState &state, bool &dirty, bool &forcecaret, bool &handled) {
	//  if highlighted, replace selected text with inserted character
	if (_highlightVisible) {
		if (state.ascii < 256 && tryInsertChar((byte)state.ascii, _caretPos)) {
			for (uint32 i = _highlightPos + _highlightCharacterCount; i > (uint32)_highlightPos; i--) {
				_editString.deleteChar(i - 1);
				_highlightString.deleteChar(i - 1);
			}
			setCaretPos(_highlightPos + 1);

			initHighlight();
			dirty = true;
			forcecaret = true;

			sendCommand(_cmd, 0);
		}
	} else if (state.ascii < 256 && tryInsertChar((byte)state.ascii, _caretPos)) {
		// insert character at end of string
		_caretPos++;
		initHighlight();
		dirty = true;
		forcecaret = true;

		sendCommand(_cmd, 0);
	} else {
		handled = false;
	}
}

int EditableWidget::getCaretOffset() const {
	Common::U32String substr(_editString.begin(), _editString.begin() + _caretPos);
	return g_gui.getStringWidth(substr, _font) - _editScrollOffset;
}

int EditableWidget::getHighlightOffset() const {
	Common::U32String substr(_editString.begin(), _editString.begin() + _highlightPos);
	return g_gui.getStringWidth(substr, _font) - _editScrollOffset;
}

void EditableWidget::drawCaret(bool erase) {
	// Only draw if item is visible
	if (!isVisible() || !_boss->isVisible())
		return;

	Common::Rect editRect = getEditRect();

	int x = editRect.left;
	int y = editRect.top;

	if (_align == Graphics::kTextAlignRight) {
		int strVisibleWidth = g_gui.getStringWidth(_editString, _font) - _editScrollOffset;
		if (strVisibleWidth > editRect.width()) {
			_drawAlign = Graphics::kTextAlignLeft;
			strVisibleWidth = editRect.width();
		} else {
			_drawAlign = _align;
		}
		x = editRect.right - strVisibleWidth;
	}

	const int caretOffset = getCaretOffset();
	x += caretOffset;

	if (y < 0 || y + editRect.height() > _h)
		return;

	if (g_gui.useRTL())
		x += g_system->getOverlayWidth() - _w - getAbsX() + g_gui.getOverlayOffset();
	else
		x += getAbsX();
	y += getAbsY();

	if (_highlightVisible) {
		// highlight rectangle
		Common::Rect _highlightRect = Common::Rect(x - _highlightSize, y, x, y + editRect.height());
		_highlightalign = Graphics::kTextAlignRight;
		// if highlight overlaps editArea redraw highlight
		if (_highlightRect.left < _textDrawableArea.left || _highlightRect.right > _textDrawableArea.right) {
			_highlightRect = Common::Rect(x - _highlightSize, y, _textDrawableArea.right, y + editRect.height());
			_highlightRect.clip(_textDrawableArea);
			// number of visible characters in highlight
			_visiblestr = (_textDrawableArea.width() > 200) ? 54 : 24;
			Common::U32String substr(_editString.begin() + _highlightPos, _editString.begin() + _highlightPos + _visiblestr);
			// draw selected character and highlight rectangle
			g_gui.theme()->drawText(_highlightRect, substr, _state, _highlightalign,
									ThemeEngine::kTextInversionFocus, -_editScrollOffset, false, _font,
									ThemeEngine::kFontColorNormal, true, _textDrawableArea);
		} else {
			g_gui.theme()->drawText(_highlightRect, _highlightString, _state, _highlightalign,
									ThemeEngine::kTextInversionFocus, -_editScrollOffset, false, _font,
									ThemeEngine::kFontColorNormal, true, _textDrawableArea);
		}
	} else {
		// if no text selected draw caret
		initHighlight();
		g_gui.theme()->drawCaret(Common::Rect(x, y, x + 1, y + editRect.height()), erase);
	}

	if (erase) {
		GUI::EditableWidget::String character;
		int width;

		if ((uint)_caretPos < _editString.size()) {
			const byte chr = _editString[_caretPos];
			width = g_gui.getCharWidth(chr, _font);
			character = chr;

			const uint last = (_caretPos > 0) ? _editString[_caretPos - 1] : 0;
			x += g_gui.getKerningOffset(last, chr, _font);
		} else {
			// We draw a fake space here to assure that removing the caret
			// does not result in color glitches in case the edit rect is
			// drawn with an inversion.
			width = g_gui.getCharWidth(' ', _font);
			character = " ";
		}

		// TODO: Right now we manually prevent text from being drawn outside
		// the edit area here. We might want to consider to use
		// setTextDrawableArea for this. However, it seems that only
		// EditTextWidget uses that but not ListWidget. Thus, one should check
		// whether we can unify the drawing in the text area first to avoid
		// possible glitches due to different methods used.
		width = MIN(editRect.width() - caretOffset, width);
		if (width > 0) {
			g_gui.theme()->drawText(Common::Rect(x, y, x + width, y + editRect.height()), character,
									_state, _drawAlign, _inversion, 0, false, _font,
									ThemeEngine::kFontColorNormal, true, _textDrawableArea);
		}
	}

	_caretVisible = !erase;
}

bool EditableWidget::setCaretPos(int newPos) {
	assert(newPos >= 0 && newPos <= (int)_editString.size());
	_caretPos = newPos;
	return adjustOffset();
}

bool EditableWidget::adjustOffset() {
	// check if the caret is still within the textbox; if it isn't,
	// adjust _editScrollOffset

	int caretpos = getCaretOffset();
	const int editWidth = getEditRect().width();

	// check if highlight visible and adjust _editScrollOffset
	if (_highlightVisible) {
		int highlightpos = getHighlightOffset();
		if (highlightpos < 1) {
			// scroll left
			_editScrollOffset += highlightpos;
			return true;
		} else if (highlightpos >= editWidth) {
			// scroll right
			_editScrollOffset -= (editWidth - highlightpos);
			return true;
		}
	} else {
		if (caretpos < 0) {
			// scroll left
			_editScrollOffset += caretpos;
			return true;
		} else if (caretpos >= editWidth) {
			// scroll right
			_editScrollOffset -= (editWidth - caretpos);
			return true;
		} else if (_editScrollOffset > 0) {
			const int strWidth = g_gui.getStringWidth(_editString, _font);
			if (strWidth - _editScrollOffset < editWidth) {
				// scroll right
				_editScrollOffset = (strWidth - editWidth);
				if (_editScrollOffset < 0)
					_editScrollOffset = 0;
			}
		}
	}

	return false;
}

void EditableWidget::makeCaretVisible() {
	_caretTime = g_system->getMillis() + kCaretBlinkTime;
	_caretVisible = true;
	drawCaret(false);
}

void EditableWidget::initHighlight() {
	// disable highlight
	_highlightVisible = false;
	// Empty the temporary string of previous values
	_highlightString.clear();
	// insert blank characters
	for (int i = 0; i < (int)_caretPos; i++)
		_highlightString.insertChar(' ', i);
	// set highlight position to caret position
	_highlightPos = _caretPos;
	_highlightSize = 0;
	// count number of characters highlighted
	_highlightCharacterCount = 0;
}

} // End of namespace GUI
