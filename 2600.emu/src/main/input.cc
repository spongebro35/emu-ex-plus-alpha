/*  This file is part of 2600.emu.

	2600.emu is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	2600.emu is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with 2600.emu.  If not, see <http://www.gnu.org/licenses/> */

#include <EventHandler.hxx>
#include <OSystem.hxx>
// TODO: Some Stella types collide with MacTypes.h
#define Debugger DebuggerMac
#include <emuframework/EmuApp.hh>
#include <emuframework/EmuInput.hh>
#undef Debugger
#include "MainSystem.hh"
#include <imagine/util/math/space.hh>

namespace EmuEx
{

enum
{
	vcsKeyIdxUp = Controls::systemKeyMapStart,
	vcsKeyIdxRight,
	vcsKeyIdxDown,
	vcsKeyIdxLeft,
	vcsKeyIdxLeftUp,
	vcsKeyIdxRightUp,
	vcsKeyIdxRightDown,
	vcsKeyIdxLeftDown,
	vcsKeyIdxJSBtn,
	vcsKeyIdxJSBtnTurbo,
	vcsKeyIdxJSBtnAlt,
	vcsKeyIdxJSBtnAltTurbo,

	vcsKeyIdxUp2,
	vcsKeyIdxRight2,
	vcsKeyIdxDown2,
	vcsKeyIdxLeft2,
	vcsKeyIdxLeftUp2,
	vcsKeyIdxRightUp2,
	vcsKeyIdxRightDown2,
	vcsKeyIdxLeftDown2,
	vcsKeyIdxJSBtn2,
	vcsKeyIdxJSBtnTurbo2,
	vcsKeyIdxJSBtnAlt2,
	vcsKeyIdxJSBtnAltTurbo2,

	vcsKeyIdxSelect,
	vcsKeyIdxReset,
	vcsKeyIdxP1Diff,
	vcsKeyIdxP2Diff,
	vcsKeyIdxColorBW,
	vcsKeyIdxKeyboard1Base,
	vcsKeyIdxKeyboard2Base = vcsKeyIdxKeyboard1Base + 12,
};

constexpr std::array<unsigned, 4> dpadButtonCodes
{
	vcsKeyIdxUp,
	vcsKeyIdxRight,
	vcsKeyIdxDown,
	vcsKeyIdxLeft,
};

constexpr unsigned consoleButtonCodes[]
{
	vcsKeyIdxSelect,
	vcsKeyIdxReset,
};

constexpr unsigned jsButtonCodes[]
{
	vcsKeyIdxJSBtn,
	vcsKeyIdxJSBtnAlt,
};

constexpr std::array jsComponents
{
	InputComponentDesc{"D-Pad", dpadButtonCodes, InputComponent::dPad, LB2DO},
	InputComponentDesc{"Joystick Buttons", jsButtonCodes, InputComponent::button, RB2DO},
	InputComponentDesc{"Console Buttons", consoleButtonCodes, InputComponent::button, RB2DO, InputComponentFlagsMask::rowSize1},
};

constexpr SystemInputDeviceDesc jsDesc{"Joystick", jsComponents};

const int EmuSystem::inputFaceBtns = 4;
bool EmuSystem::inputHasShortBtnTexture = true;
const int EmuSystem::maxPlayers = 2;

void A2600System::clearInputBuffers(EmuInputView &)
{
	Event &ev = osystem.eventHandler().event();
	ev.clear();

	ev.set(Event::ConsoleLeftDiffB, p1DiffB);
	ev.set(Event::ConsoleLeftDiffA, !p1DiffB);
	ev.set(Event::ConsoleRightDiffB, p2DiffB);
	ev.set(Event::ConsoleRightDiffA, !p2DiffB);
	ev.set(Event::ConsoleColor, vcsColor);
	ev.set(Event::ConsoleBlackWhite, !vcsColor);
}

void A2600System::updateJoytickMapping(EmuApp &app, Controller::Type type)
{
	if(type == Controller::Type::Paddles)
	{
		jsFireMap = {Event::LeftPaddleAFire, Event::LeftPaddleBFire};
		jsLeftMap = {Event::LeftPaddleAIncrease, Event::LeftPaddleBIncrease};
		jsRightMap = {Event::LeftPaddleADecrease, Event::LeftPaddleBDecrease};
	}
	else
	{
		jsFireMap = {Event::LeftJoystickFire, Event::RightJoystickFire};
		jsLeftMap = {Event::LeftJoystickLeft, Event::RightJoystickLeft};
		jsRightMap = {Event::LeftJoystickRight, Event::RightJoystickRight};
	}
}

static bool isJoystickButton(unsigned input)
{
	switch(input)
	{
		case vcsKeyIdxJSBtnTurbo:
		case vcsKeyIdxJSBtn:
		case vcsKeyIdxJSBtnAltTurbo:
		case vcsKeyIdxJSBtnAlt:
		case vcsKeyIdxJSBtnTurbo2:
		case vcsKeyIdxJSBtn2:
		case vcsKeyIdxJSBtnAltTurbo2:
		case vcsKeyIdxJSBtnAlt2:
			return true;
		default: return false;
	}
}

InputAction A2600System::translateInputAction(InputAction action)
{
	if(!isJoystickButton(action.key))
		action.setTurboFlag(false);
	action.key = [&] -> unsigned
	{
		switch(action.key)
		{
			case vcsKeyIdxUp: return Event::LeftJoystickUp;
			case vcsKeyIdxRight: return jsRightMap[0];
			case vcsKeyIdxDown: return Event::LeftJoystickDown;
			case vcsKeyIdxLeft: return jsLeftMap[0];
			case vcsKeyIdxLeftUp: return Event::LeftJoystickLeft | (Event::LeftJoystickUp << 8);
			case vcsKeyIdxRightUp: return Event::LeftJoystickRight | (Event::LeftJoystickUp << 8);
			case vcsKeyIdxRightDown: return Event::LeftJoystickRight | (Event::LeftJoystickDown << 8);
			case vcsKeyIdxLeftDown: return Event::LeftJoystickLeft | (Event::LeftJoystickDown << 8);
			case vcsKeyIdxJSBtnTurbo: action.setTurboFlag(true); [[fallthrough]];
			case vcsKeyIdxJSBtn: return jsFireMap[0];
			case vcsKeyIdxJSBtnAltTurbo: action.setTurboFlag(true); [[fallthrough]];
			case vcsKeyIdxJSBtnAlt: return Event::LeftJoystickFire5;

			case vcsKeyIdxUp2: return Event::RightJoystickUp;
			case vcsKeyIdxRight2: return jsRightMap[1];
			case vcsKeyIdxDown2: return Event::RightJoystickDown;
			case vcsKeyIdxLeft2: return jsLeftMap[1];
			case vcsKeyIdxLeftUp2: return Event::RightJoystickLeft | (Event::RightJoystickUp << 8);
			case vcsKeyIdxRightUp2: return Event::RightJoystickRight | (Event::RightJoystickUp << 8);
			case vcsKeyIdxRightDown2: return Event::RightJoystickRight | (Event::RightJoystickDown << 8);
			case vcsKeyIdxLeftDown2: return Event::RightJoystickLeft | (Event::RightJoystickDown << 8);
			case vcsKeyIdxJSBtnTurbo2: action.setTurboFlag(true); [[fallthrough]];
			case vcsKeyIdxJSBtn2: return jsFireMap[1];
			case vcsKeyIdxJSBtnAltTurbo2: action.setTurboFlag(true); [[fallthrough]];
			case vcsKeyIdxJSBtnAlt2: return Event::RightJoystickFire5;

			case vcsKeyIdxSelect: return Event::ConsoleSelect;
			case vcsKeyIdxP1Diff: return Event::Combo1; // toggle P1 diff
			case vcsKeyIdxP2Diff: return Event::Combo2; // toggle P2 diff
			case vcsKeyIdxColorBW: return Event::Combo3; // toggle Color/BW
			case vcsKeyIdxReset: return Event::ConsoleReset;
			case vcsKeyIdxKeyboard1Base ... vcsKeyIdxKeyboard1Base + 11:
				return Event::LeftKeyboard1 + (action.key - vcsKeyIdxKeyboard1Base);
			case vcsKeyIdxKeyboard2Base ... vcsKeyIdxKeyboard2Base + 11:
				return Event::RightKeyboard1 + (action.key - vcsKeyIdxKeyboard2Base);
		}
		bug_unreachable("invalid key");
	}();
	return action;
}

void A2600System::handleInputAction(EmuApp *app, InputAction a)
{
	auto &ev = osystem.eventHandler().event();
	auto event1 = a.key & 0xFF;
	bool isPushed = a.state == Input::Action::PUSHED;

	//logMsg("got key %d", emuKey);

	switch(event1)
	{
		case Event::Combo1:
			if(!isPushed)
				break;
			p1DiffB ^= true;
			if(app)
			{
				app->postMessage(1, false, p1DiffB ? "P1 Difficulty -> B" : "P1 Difficulty -> A");
			}
			ev.set(Event::ConsoleLeftDiffB, p1DiffB);
			ev.set(Event::ConsoleLeftDiffA, !p1DiffB);
			break;
		case Event::Combo2:
			if(!isPushed)
				break;
			p2DiffB ^= true;
			if(app)
			{
				app->postMessage(1, false, p2DiffB ? "P2 Difficulty -> B" : "P2 Difficulty -> A");
			}
			ev.set(Event::ConsoleRightDiffB, p2DiffB);
			ev.set(Event::ConsoleRightDiffA, !p2DiffB);
			break;
		case Event::Combo3:
			if(!isPushed)
				break;
			vcsColor ^= true;
			if(app)
			{
				app->postMessage(1, false, vcsColor ? "Color Switch -> Color" : "Color Switch -> B&W");
			}
			ev.set(Event::ConsoleColor, vcsColor);
			ev.set(Event::ConsoleBlackWhite, !vcsColor);
			break;
		case Event::LeftKeyboard1 ... Event::RightKeyboardPound:
			ev.set(Event::Type(event1), isPushed);
			break;
		default:
			ev.set(Event::Type(event1), isPushed);
			auto event2 = a.key >> 8;
			if(event2) // extra event for diagonals
			{
				ev.set(Event::Type(event2), isPushed);
			}
	}
}

static void updateDPadForPaddles(EmuApp &app, Console &console, PaddleRegionMode mode)
{
	if(console.leftController().type() == Controller::Type::Paddles)
	{
		app.defaultVController().setGamepadDPadIsEnabled(mode == PaddleRegionMode::OFF);
	}
	else
	{
		app.defaultVController().setGamepadDPadIsEnabled(true);
	}
}

void A2600System::updatePaddlesRegionMode(EmuApp &app, PaddleRegionMode mode)
{
	optionPaddleAnalogRegion = (uint8_t)mode;
	updateDPadForPaddles(app, osystem.console(), mode);
}

void A2600System::setControllerType(EmuApp &app, Console &console, Controller::Type type)
{
	if(type == Controller::Type::Unknown)
		type = autoDetectedInput1;
	if(type == Controller::Type::Genesis)
	{
		app.unsetDisabledInputKeys();
	}
	else
	{
		static constexpr std::array<unsigned, 2> disableExtraBtn{vcsKeyIdxJSBtnAlt, vcsKeyIdxJSBtnAltTurbo};
		app.setDisabledInputKeys(disableExtraBtn);
	}
	updateDPadForPaddles(app, console, (PaddleRegionMode)optionPaddleAnalogRegion.val);
	updateJoytickMapping(app, type);
	Controller &currentController = console.leftController();
	if(currentController.type() == type)
	{
		logMsg("using controller type:%s", controllerTypeStr(type));
		return;
	}
	auto props = console.properties();
	props.set(PropType::Controller_Left, Controller::getPropName(type));
	props.set(PropType::Controller_Right, Controller::getPropName(type));
	const string& md5 = props.get(PropType::Cart_MD5);
	console.setProperties(props);
	console.setControllers(md5);
	if(Config::DEBUG_BUILD)
	{
		logMsg("current controller name in console object:%s", console.leftController().name().c_str());
	}
	logMsg("set controller to type:%s", controllerTypeStr(type));
}

Controller::Type limitToSupportedControllerTypes(Controller::Type type)
{
	switch(type)
	{
		case Controller::Type::Joystick:
		case Controller::Type::Genesis:
		case Controller::Type::Keyboard:
		case Controller::Type::Paddles:
			return type;
		default:
			return Controller::Type::Joystick;
	}
}

const char *controllerTypeStr(Controller::Type type)
{
	switch(type)
	{
		case Controller::Type::Joystick: return "Joystick";
		case Controller::Type::Genesis: return "Genesis Gamepad";
		case Controller::Type::Keyboard: return "Keyboard";
		case Controller::Type::Paddles: return "Paddles";
		default: return "Auto";
	}
}

bool A2600System::updatePaddle(Input::DragTrackerState dragState)
{
	auto regionMode = (PaddleRegionMode)optionPaddleAnalogRegion.val;
	if(regionMode == PaddleRegionMode::OFF)
		return false;
	auto &app = osystem.app();
	int regionXStart = 0;
	int regionXEnd = app.viewController().inputView().viewRect().size().x;
	if(regionMode == PaddleRegionMode::LEFT)
	{
		regionXEnd /= 2;
	}
	else if(regionMode == PaddleRegionMode::RIGHT)
	{
		regionXStart = regionXEnd / 2;
	}
	auto pos = IG::remap(dragState.pos().x, regionXStart, regionXEnd, -32768 / 2, 32767 / 2);
	pos = std::clamp(pos, -32768, 32767);
	auto evType = app.defaultVController().inputPlayer() == 0 ? Event::LeftPaddleAAnalog : Event::LeftPaddleBAnalog;
	osystem.eventHandler().event().set(evType, pos);
	//logMsg("set paddle position:%d", pos);
	return true;
}

bool A2600System::onPointerInputStart(const Input::MotionEvent &, Input::DragTrackerState dragState, IG::WindowRect)
{
	switch(osystem.console().leftController().type())
	{
		case Controller::Type::Paddles:
		{
			return updatePaddle(dragState);
		}
		default:
			return false;
	}
}

bool A2600System::onPointerInputUpdate(const Input::MotionEvent &, Input::DragTrackerState dragState,
	Input::DragTrackerState, IG::WindowRect)
{
	switch(osystem.console().leftController().type())
	{
		case Controller::Type::Paddles:
		{
			return updatePaddle(dragState);
		}
		default:
			return false;
	}
}

VControllerImageIndex A2600System::mapVControllerButton(unsigned key) const
{
	using enum VControllerImageIndex;
	switch(key)
	{
		case vcsKeyIdxSelect: return auxButton1;
		case vcsKeyIdxReset: return auxButton2;
		case vcsKeyIdxJSBtn:
		case vcsKeyIdxJSBtnAlt: return button1;
		case vcsKeyIdxJSBtnTurbo:
		case vcsKeyIdxJSBtnAltTurbo: return button2;
		default: return button1;
	}
}

SystemInputDeviceDesc A2600System::inputDeviceDesc(int idx) const
{
	return jsDesc;
}

}
