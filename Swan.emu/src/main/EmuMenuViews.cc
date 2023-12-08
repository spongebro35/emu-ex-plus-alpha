/*  This file is part of Swan.emu.

	Swan.emu is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Swan.emu is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Swan.emu.  If not, see <http://www.gnu.org/licenses/> */

#include <emuframework/SystemOptionView.hh>
#include <emuframework/SystemActionsView.hh>
#include <mednafen-emuex/MDFNUtils.hh>
#include "MainApp.hh"
#include <wswan/wswan.h>

namespace EmuEx
{

using namespace MDFN_IEN_WSWAN;

template <class T>
using MainAppHelper = EmuAppHelper<T, MainApp>;

class CustomSystemOptionView : public SystemOptionView, public MainAppHelper<CustomSystemOptionView>
{
	using MainAppHelper<CustomSystemOptionView>::system;
	using MainAppHelper<CustomSystemOptionView>::app;

	TextHeadingMenuItem userProfile{"WonderSwan User Profile", &defaultBoldFace()};

	BoolMenuItem language
	{
		"Language", &defaultFace(),
		(bool)system().userProfile.languageIsEnglish,
		"Japanese", "English",
		[this](BoolMenuItem &item)
		{
			system().userProfile.languageIsEnglish = item.flipBoolValue(*this);
		}
	};

	DualTextMenuItem name
	{
		"Name", system().userName, &defaultFace(),
		[this](const Input::Event &e)
		{
			app().pushAndShowNewCollectValueInputView<const char*, ScanValueMode::ALLOW_BLANK>(attachParams(), e,
				"Input name", system().userName,
				[this](EmuApp &app, auto str_)
				{
					std::string_view str{str_};
					if(str.size() > system().userName.max_size())
					{
						app.postErrorMessage("Name is too long");
						return false;
					}
					system().userName = str;
					name.set2ndName(str);
					return true;
				});
		}
	};

	DualTextMenuItem birthYear
	{
		"Birth Year", std::to_string(system().userProfile.birthYear), &defaultFace(),
		[this](const Input::Event &e)
		{
			app().pushAndShowNewCollectValueRangeInputView<int, 1, 9999>(attachParams(), e,
				"Input 1 to 9999", std::to_string(system().userProfile.birthYear),
				[this](EmuApp &app, auto val)
				{
					system().userProfile.birthYear = val;
					birthYear.set2ndName(std::to_string(val));
					return true;
				});
		}
	};

	DualTextMenuItem birthMonth
	{
		"Birth Month", std::to_string(system().userProfile.birthMonth), &defaultFace(),
		[this](const Input::Event &e)
		{
			app().pushAndShowNewCollectValueRangeInputView<int, 1, 12>(attachParams(), e,
				"Input 1 to 12", std::to_string(system().userProfile.birthMonth),
				[this](EmuApp &app, auto val)
				{
					system().userProfile.birthMonth = val;
					birthMonth.set2ndName(std::to_string(val));
					return true;
				});
		}
	};

	DualTextMenuItem birthDay
	{
		"Birth Day", std::to_string(system().userProfile.birthDay), &defaultFace(),
		[this](const Input::Event &e)
		{
			app().pushAndShowNewCollectValueRangeInputView<int, 1, 31>(attachParams(), e,
				"Input 1 to 31", std::to_string(system().userProfile.birthDay),
				[this](EmuApp &app, auto val)
				{
					system().userProfile.birthDay = val;
					birthDay.set2ndName(std::to_string(val));
					return true;
				});
		}
	};

	TextMenuItem::SelectDelegate setSexDel()
	{
		return [this](TextMenuItem &item) { system().userProfile.sex = item.id(); };
	}

	TextMenuItem sexItem[3]
	{
		{"M", &defaultFace(), setSexDel(), WSWAN_SEX_MALE},
		{"F", &defaultFace(), setSexDel(), WSWAN_SEX_FEMALE},
		{"?", &defaultFace(), setSexDel(), 3},
	};

	MultiChoiceMenuItem sex
	{
		"Sex", &defaultFace(),
		(MenuItem::Id)system().userProfile.sex,
		sexItem
	};

	TextMenuItem::SelectDelegate setBloodTypeDel()
	{
		return [this](TextMenuItem &item) { system().userProfile.bloodType = item.id(); };
	}

	TextMenuItem bloodTypeItem[5]
	{
		{"A",  &defaultFace(), setBloodTypeDel(), WSWAN_BLOOD_A},
		{"B",  &defaultFace(), setBloodTypeDel(), WSWAN_BLOOD_B},
		{"O",  &defaultFace(), setBloodTypeDel(), WSWAN_BLOOD_O},
		{"AB", &defaultFace(), setBloodTypeDel(), WSWAN_BLOOD_AB},
		{"?",  &defaultFace(), setBloodTypeDel(), 5},
	};

	MultiChoiceMenuItem bloodType
	{
		"Blood Type", &defaultFace(),
		(MenuItem::Id)system().userProfile.bloodType,
		bloodTypeItem
	};

	BoolMenuItem saveFilenameType = saveFilenameTypeMenuItem(*this, system());

public:
	CustomSystemOptionView(ViewAttachParams attach): SystemOptionView{attach, true}
	{
		loadStockItems();
		item.emplace_back(&saveFilenameType);
		item.emplace_back(&userProfile);
		item.emplace_back(&language);
		item.emplace_back(&name);
		item.emplace_back(&birthYear);
		item.emplace_back(&birthMonth);
		item.emplace_back(&birthDay);
		item.emplace_back(&sex);
		item.emplace_back(&bloodType);
	}
};

class ConsoleOptionView : public TableView, public MainAppHelper<ConsoleOptionView>
{
	TextMenuItem::SelectDelegate setRotationDel()
	{
		return [this](TextMenuItem &item) { system().setRotation((WsRotation)item.id()); };
	}

	TextMenuItem rotationItem[3]
	{
		{"Auto",       &defaultFace(), setRotationDel(), to_underlying(WsRotation::Auto)},
		{"Horizontal", &defaultFace(), setRotationDel(), to_underlying(WsRotation::Horizontal)},
		{"Vertical",   &defaultFace(), setRotationDel(), to_underlying(WsRotation::Vertical)},
	};

	MultiChoiceMenuItem rotation
	{
		"Handheld Rotation", &defaultFace(),
		(MenuItem::Id)system().rotation,
		rotationItem
	};

	TextHeadingMenuItem vGamepad{"Virtual Gamepad", &defaultBoldFace()};

	BoolMenuItem showVGamepadButtons
	{
		system().isRotated() ? "Show A/B" : "Show Y1-4", &defaultFace(),
		system().isRotated() ? system().showVGamepadABWhenVertical : system().showVGamepadYWhenHorizonal,
		[this](BoolMenuItem &item)
		{
			if(system().isRotated())
				system().setShowVGamepadABWhenVertical(item.flipBoolValue(*this));
			else
				system().setShowVGamepadYWhenHorizonal(item.flipBoolValue(*this));
		}
	};

	std::array<MenuItem*, 3> menuItem
	{
		&rotation,
		&vGamepad,
		&showVGamepadButtons,
	};

public:
	ConsoleOptionView(ViewAttachParams attach):
		TableView
		{
			"Console Options",
			attach,
			menuItem
		} {}
};

class CustomSystemActionsView : public SystemActionsView
{
private:
	TextMenuItem options
	{
		"Console Options", &defaultFace(),
		[this](Input::Event e) { pushAndShow(makeView<ConsoleOptionView>(), e); }
	};

public:
	CustomSystemActionsView(ViewAttachParams attach): SystemActionsView{attach, true}
	{
		item.emplace_back(&options);
		loadStandardItems();
	}
};

std::unique_ptr<View> EmuApp::makeCustomView(ViewAttachParams attach, ViewID id)
{
	switch(id)
	{
		case ViewID::SYSTEM_ACTIONS: return std::make_unique<CustomSystemActionsView>(attach);
		case ViewID::SYSTEM_OPTIONS: return std::make_unique<CustomSystemOptionView>(attach);
		default: return nullptr;
	}
}

}
