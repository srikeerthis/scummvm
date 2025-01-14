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

#include "ags/events.h"
#include "common/system.h"
#include "ags/globals.h"
#include "ags/engine/ac/keycode.h"

namespace AGS {

EventsManager *g_events;

EventsManager::EventsManager() {
	g_events = this;
	_keys.resize(AGS3::__allegro_KEY_MAX);
	Common::fill(&_joystickAxis[0], &_joystickAxis[32], 0);
	Common::fill(&_joystickButton[0], &_joystickButton[32], 0);
}

EventsManager::~EventsManager() {
	g_events = nullptr;
}

void EventsManager::pollEvents() {
	Common::Event e;

	while (g_system->getEventManager()->pollEvent(e)) {
		switch (e.type) {
		case Common::EVENT_QUIT:
		case Common::EVENT_RETURN_TO_LAUNCHER:
			_G(want_exit) = true;
			_G(abort_engine) = true;
			_G(check_dynamic_sprites_at_exit) = false;
			break;

		case Common::EVENT_JOYAXIS_MOTION:
			assert(e.joystick.axis < 32);
			_joystickAxis[e.joystick.axis] = e.joystick.position;
			break;

		case Common::EVENT_JOYBUTTON_DOWN:
			assert(e.joystick.button < 32);
			_joystickButton[e.joystick.button] = true;
			break;

		case Common::EVENT_JOYBUTTON_UP:
			assert(e.joystick.button < 32);
			_joystickButton[e.joystick.button] = false;
			break;

		case Common::EVENT_KEYDOWN:
			updateKeys(e.kbd, true);

			if (!isModifierKey(e.kbd.keycode)) {
				// Add keypresses to the pending key list
				_pendingKeys.push(e.kbd);
			}
			break;

		case Common::EVENT_KEYUP:
			updateKeys(e.kbd, false);
			break;

		default:
			// Add other event types to the pending events queue. If the event is a
			// mouse move and the prior one was also, then discard the prior one.
			// This'll help prevent too many mouse move events accumulating
			if (e.type == Common::EVENT_MOUSEMOVE && !_pendingEvents.empty() &&
					_pendingEvents.back().type == Common::EVENT_MOUSEMOVE)
				_pendingEvents.back() = e;
			else
				_pendingEvents.push(e);
			break;
		}
	}
}

bool EventsManager::keypressed() {
	pollEvents();
	return !_pendingKeys.empty();
}

int EventsManager::readKey() {
	pollEvents();
	if (_pendingKeys.empty())
		return 0;

	Common::KeyState keyState = _pendingKeys.pop();

	int scancode = getScancode(keyState.keycode);
	int code = scancode << 8;

	if (isExtendedKey(keyState.keycode))
		code |= EXTENDED_KEY_CODE;
	else if ((keyState.flags & (Common::KBD_CTRL | Common::KBD_ALT))  == 0)
		code |= keyState.ascii;
	else
		code |= scancode;

	return code;
}

Common::Event EventsManager::readEvent() {
	pollEvents();
	return _pendingEvents.empty() ? Common::Event() : _pendingEvents.pop();
}

void EventsManager::warpMouse(const Common::Point &newPos) {
	g_system->warpMouse(newPos.x, newPos.y);
}

bool EventsManager::isModifierKey(const Common::KeyCode &keycode) const {
	return keycode == Common::KEYCODE_LCTRL || keycode == Common::KEYCODE_LALT
		|| keycode == Common::KEYCODE_RCTRL || keycode == Common::KEYCODE_RALT
		|| keycode == Common::KEYCODE_LSHIFT || keycode == Common::KEYCODE_RSHIFT
		|| keycode == Common::KEYCODE_LSUPER || keycode == Common::KEYCODE_RSUPER
		|| keycode == Common::KEYCODE_CAPSLOCK || keycode == Common::KEYCODE_NUMLOCK
		|| keycode == Common::KEYCODE_SCROLLOCK;
}

bool EventsManager::isExtendedKey(const Common::KeyCode &keycode) const {
	const Common::KeyCode EXTENDED_KEYS[] = {
		Common::KEYCODE_F1, Common::KEYCODE_F2, Common::KEYCODE_F3,
		Common::KEYCODE_F4, Common::KEYCODE_F5, Common::KEYCODE_F6,
		Common::KEYCODE_F7, Common::KEYCODE_F8, Common::KEYCODE_F9,
		Common::KEYCODE_F10, Common::KEYCODE_F11, Common::KEYCODE_F12,
		Common::KEYCODE_KP0, Common::KEYCODE_KP1, Common::KEYCODE_KP2,
		Common::KEYCODE_KP3, Common::KEYCODE_KP4, Common::KEYCODE_KP5,
		Common::KEYCODE_KP6, Common::KEYCODE_KP7, Common::KEYCODE_KP8,
		Common::KEYCODE_KP9, Common::KEYCODE_KP_PERIOD,
		Common::KEYCODE_INSERT, Common::KEYCODE_DELETE,
		Common::KEYCODE_HOME, Common::KEYCODE_END,
		Common::KEYCODE_PAGEUP, Common::KEYCODE_PAGEDOWN,
		Common::KEYCODE_LEFT, Common::KEYCODE_RIGHT,
		Common::KEYCODE_UP, Common::KEYCODE_DOWN,
		Common::KEYCODE_INVALID
	};

	for (const Common::KeyCode *kc = EXTENDED_KEYS;
			*kc != Common::KEYCODE_INVALID; ++kc) {
		if (keycode == *kc)
			return true;
	}

	return false;
}

int EventsManager::getScancode(Common::KeyCode keycode) const {
	if (keycode >= Common::KEYCODE_a && keycode <= Common::KEYCODE_z)
		return (int)keycode - Common::KEYCODE_a + AGS3::__allegro_KEY_A;
	if (keycode >= Common::KEYCODE_0 && keycode <= Common::KEYCODE_9)
		return (int)keycode - Common::KEYCODE_0 + AGS3::__allegro_KEY_0;
	if (keycode >= Common::KEYCODE_KP0 && keycode <= Common::KEYCODE_KP9)
		return (int)keycode - Common::KEYCODE_KP0 + AGS3::__allegro_KEY_0_PAD;
	if (keycode >= Common::KEYCODE_F1 && keycode <= Common::KEYCODE_F12)
		return (int)keycode - Common::KEYCODE_F1 + AGS3::__allegro_KEY_F1;

	switch (keycode) {
	case Common::KEYCODE_ESCAPE: return AGS3::__allegro_KEY_ESC;
	case Common::KEYCODE_TILDE: return AGS3::__allegro_KEY_TILDE;
	case Common::KEYCODE_MINUS: return AGS3::__allegro_KEY_MINUS;
	case Common::KEYCODE_EQUALS: return AGS3::__allegro_KEY_EQUALS;
	case Common::KEYCODE_BACKSPACE: return AGS3::__allegro_KEY_BACKSPACE;
	case Common::KEYCODE_TAB: return AGS3::__allegro_KEY_TAB;
	case Common::KEYCODE_LEFTBRACKET: return AGS3::__allegro_KEY_OPENBRACE;
	case Common::KEYCODE_RIGHTBRACKET: return AGS3::__allegro_KEY_CLOSEBRACE;
	case Common::KEYCODE_RETURN: return AGS3::__allegro_KEY_ENTER;
	case Common::KEYCODE_COLON: return AGS3::__allegro_KEY_COLON;
	case Common::KEYCODE_QUOTE: return AGS3::__allegro_KEY_QUOTE;
	case Common::KEYCODE_BACKSLASH: return AGS3::__allegro_KEY_BACKSLASH;
	case Common::KEYCODE_COMMA: return AGS3::__allegro_KEY_COMMA;
	case Common::KEYCODE_SLASH: return AGS3::__allegro_KEY_SLASH;
	case Common::KEYCODE_SPACE: return AGS3::__allegro_KEY_SPACE;
	case Common::KEYCODE_INSERT: return AGS3::__allegro_KEY_INSERT;
	case Common::KEYCODE_DELETE: return AGS3::__allegro_KEY_DEL;
	case Common::KEYCODE_HOME: return AGS3::__allegro_KEY_HOME;
	case Common::KEYCODE_END: return AGS3::__allegro_KEY_END;
	case Common::KEYCODE_PAGEUP: return AGS3::__allegro_KEY_PGUP;
	case Common::KEYCODE_PAGEDOWN: return AGS3::__allegro_KEY_PGDN;
	case Common::KEYCODE_LEFT: return AGS3::__allegro_KEY_LEFT;
	case Common::KEYCODE_RIGHT: return AGS3::__allegro_KEY_RIGHT;
	case Common::KEYCODE_UP: return AGS3::__allegro_KEY_UP;
	case Common::KEYCODE_DOWN: return AGS3::__allegro_KEY_DOWN;
	case Common::KEYCODE_KP_DIVIDE: return AGS3::__allegro_KEY_SLASH_PAD;
	case Common::KEYCODE_ASTERISK: return AGS3::__allegro_KEY_ASTERISK;
	case Common::KEYCODE_KP_MINUS: return AGS3::__allegro_KEY_MINUS_PAD;
	case Common::KEYCODE_KP_PLUS: return AGS3::__allegro_KEY_PLUS_PAD;
	case Common::KEYCODE_KP_PERIOD: return AGS3::__allegro_KEY_DEL_PAD;
	case Common::KEYCODE_KP_ENTER: return AGS3::__allegro_KEY_ENTER_PAD;
	case Common::KEYCODE_PRINT: return AGS3::__allegro_KEY_PRTSCR;
	case Common::KEYCODE_PAUSE: return AGS3::__allegro_KEY_PAUSE;
	case Common::KEYCODE_SEMICOLON: return AGS3::__allegro_KEY_SEMICOLON;

	case Common::KEYCODE_LSHIFT: return AGS3::__allegro_KEY_LSHIFT;
	case Common::KEYCODE_RSHIFT: return AGS3::__allegro_KEY_RSHIFT;
	case Common::KEYCODE_LCTRL: return AGS3::__allegro_KEY_LCONTROL;
	case Common::KEYCODE_RCTRL: return AGS3::__allegro_KEY_RCONTROL;
	case Common::KEYCODE_LALT: return AGS3::__allegro_KEY_ALT;
	case Common::KEYCODE_RALT: return AGS3::__allegro_KEY_ALT;
	case Common::KEYCODE_SCROLLOCK: return AGS3::__allegro_KEY_SCRLOCK;
	case Common::KEYCODE_NUMLOCK: return AGS3::__allegro_KEY_NUMLOCK;
	case Common::KEYCODE_CAPSLOCK: return AGS3::__allegro_KEY_CAPSLOCK;
	default: return 0;
	}
}

void EventsManager::updateKeys(const Common::KeyState &keyState, bool isDown) {
	// Update the keys array with whether the key is pressed
	int scancode = getScancode(keyState.keycode);
	if (scancode != 0)
		_keys[scancode] = isDown;
}

uint EventsManager::getModifierFlags() const {
	if (_pendingKeys.empty())
		return 0;

	byte flags = _pendingKeys.front().flags;
	uint keyFlags = 0;
	if (flags & Common::KBD_SHIFT)
		keyFlags |= AGS3::__allegro_KB_SHIFT_FLAG;
	if (flags & Common::KBD_CTRL)
		keyFlags |= AGS3::__allegro_KB_CTRL_FLAG;
	if (flags & Common::KBD_ALT)
		keyFlags |= AGS3::__allegro_KB_ALT_FLAG;
	if (flags & Common::KBD_META)
		keyFlags |= AGS3::__allegro_KB_COMMAND_FLAG;
	if (flags & Common::KBD_SCRL)
		keyFlags |= AGS3::__allegro_KB_SCROLOCK_FLAG;
	if (flags & Common::KBD_NUM)
		keyFlags |= AGS3::__allegro_KB_NUMLOCK_FLAG;
	if (flags & Common::KBD_CAPS)
		keyFlags |= AGS3::__allegro_KB_CAPSLOCK_FLAG;

	return keyFlags;
}

bool EventsManager::isKeyPressed(AGS3::AllegroKbdKeycode keycode) const {
	return _keys[keycode];
}

} // namespace AGS
