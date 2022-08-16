/*  This file is part of EmuFramework.

	Imagine is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Imagine is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with EmuFramework.  If not, see <http://www.gnu.org/licenses/> */

#define LOGTAG "VControllerKeyboard"
#include <emuframework/VController.hh>
#include <emuframework/EmuApp.hh>
#include <imagine/util/math/space.hh>
#include <imagine/gfx/RendererCommands.hh>
#include <imagine/logger/logger.h>

namespace EmuEx
{

void VControllerKeyboard::updateImg(Gfx::Renderer &r)
{
	if(mode_ == VControllerKbMode::LAYOUT_2)
		spr.setUVBounds({{0., .5}, {texXEnd, 1.}});
	else
		spr.setUVBounds({{}, {texXEnd, .5}});
}

void VControllerKeyboard::setImg(Gfx::Renderer &r, Gfx::TextureSpan img)
{
	spr = {{{-.5, -.5}, {.5, .5}}, img};
	hasMipmaps = img.texture()->levels() > 1;
	texXEnd = img.uvBounds().x2;
	updateImg(r);
}

void VControllerKeyboard::place(float btnSize, float yOffset, Gfx::ProjectionPlane projP)
{
	float xSize, ySize;
	IG::setSizesWithRatioX(xSize, ySize, 3./2., std::min(btnSize*10, projP.width()));
	float vArea = projP.height() - yOffset*2;
	if(ySize > vArea)
	{
		IG::setSizesWithRatioY(xSize, ySize, 3./2., vArea);
	}
	Gfx::GCRect boundGC {{}, {xSize, ySize}};
	boundGC.setPos({0., projP.bounds().y + yOffset}, CB2DO);
	spr.setPos(boundGC);
	bound = projP.projectRect(boundGC);
	keyXSize = std::max(bound.xSize() / VKEY_COLS, 1);
	keyYSize = std::max(bound.ySize() / KEY_ROWS, 1);
	logMsg("key size %dx%d", keyXSize, keyYSize);
}

void VControllerKeyboard::draw(Gfx::RendererCommands &cmds, Gfx::ProjectionPlane projP) const
{
	if(hasMipmaps)
		cmds.set(View::imageCommonTextureSampler);
	else
		cmds.set(Gfx::CommonTextureSampler::NO_MIP_CLAMP);
	auto &basicEffect = cmds.basicEffect();
	spr.draw(cmds, basicEffect);
	if(selected.x != -1)
	{
		cmds.setColor(.2, .71, .9, 1./3.);
		basicEffect.disableTexture(cmds);
		IG::WindowRect rect{};
		rect.x = bound.x + (selected.x * keyXSize);
		rect.x2 = bound.x + ((selected.x2 + 1) * keyXSize);
		rect.y = bound.y + (selected.y * keyYSize);
		rect.y2 = rect.y + keyYSize;
		Gfx::GeomRect::draw(cmds, rect, projP);
	}
	if(shiftIsActive() && mode_ == VControllerKbMode::LAYOUT_1)
	{
		cmds.setColor(.2, .71, .9, 1./2.);
		basicEffect.disableTexture(cmds);
		IG::WindowRect rect{};
		rect.x = bound.x + (shiftRect.x * keyXSize);
		rect.x2 = bound.x + ((shiftRect.x2 + 1) * keyXSize);
		rect.y = bound.y + (shiftRect.y * keyYSize);
		rect.y2 = rect.y + keyYSize;
		Gfx::GeomRect::draw(cmds, rect, projP);
	}
}

int VControllerKeyboard::getInput(IG::WP c) const
{
	if(!bound.overlaps(c))
		return -1;
	int relX = c.x - bound.x, relY = c.y - bound.y;
	int row = std::min(relY/keyYSize, 3);
	int col = std::min(relX/keyXSize, 19);
	int idx = col + (row * VKEY_COLS);
	//logMsg("pointer %d,%d key @ %d,%d, idx %d", relX, relY, row, col, idx);
	return idx;
}

unsigned VControllerKeyboard::translateInput(int idx) const
{
	assumeExpr(idx < VKEY_COLS * KEY_ROWS);
	return table[0][idx];
}

bool VControllerKeyboard::keyInput(VController &v, Gfx::Renderer &r, const Input::KeyEvent &e)
{
	if(selected.x == -1)
	{
		if(e.pushed(Input::DefaultKey::CONFIRM) || e.pushed(Input::DefaultKey::DIRECTION))
		{
			selected = selectKey(0, 3);
			return true;
		}
		else
		{
			return false;
		}
	}
	else if(e.isDefaultConfirmButton())
	{
		if(currentKey() == VController::TOGGLE_KEYBOARD)
		{
			if(!e.pushed() || e.repeated())
				return false;
			logMsg("dismiss kb");
			unselectKey();
			v.toggleKeyboard();
		}
		else if(currentKey() == VController::CHANGE_KEYBOARD_MODE)
		{
			if(!e.pushed() || e.repeated())
				return false;
			logMsg("switch kb mode");
			cycleMode(v.system(), r);
			v.resetInput();
		}
		else if(e.pushed())
		{
			v.system().handleInputAction(&v.app(), {currentKey(), Input::Action::PUSHED});
		}
		else
		{
			v.system().handleInputAction(&v.app(), {currentKey(), Input::Action::RELEASED});
		}
		return true;
	}
	else if(!e.pushed())
	{
		return false;
	}
	else if(e.isDefaultLeftButton())
	{
		selectKeyRel(-1, 0);
		return true;
	}
	else if(e.isDefaultRightButton())
	{
		selectKeyRel(1, 0);
		return true;
	}
	else if(e.isDefaultUpButton())
	{
		selectKeyRel(0, -1);
		return true;
	}
	else if(e.isDefaultDownButton())
	{
		selectKeyRel(0, 1);
		return true;
	}
	return false;
}

IG::WindowRect VControllerKeyboard::selectKey(unsigned x, unsigned y)
{
	if(x >= VKEY_COLS || y >= KEY_ROWS)
	{
		logErr("selected key:%dx%d out of range", x, y);
		return {{-1, -1}, {-1, -1}};
	}
	return extendKeySelection({{(int)x, (int)y}, {(int)x, (int)y}});
}

void VControllerKeyboard::selectKeyRel(int x, int y)
{
	if(x > 0)
	{
		selected.x2 = IG::wrapMinMax(selected.x2 + x, 0, (int)VKEY_COLS);
		selected.x = selected.x2;
	}
	else if(x < 0)
	{
		selected.x = IG::wrapMinMax(selected.x + x, 0, (int)VKEY_COLS);
		selected.x2 = selected.x;
	}
	if(y != 0)
	{
		selected.y = selected.y2 = IG::wrapMinMax(selected.y2 + y, 0, (int)KEY_ROWS);
		selected.x2 = selected.x;
	}
	selected = extendKeySelection(selected);
	if(!currentKey(selected.x, selected.y))
	{
		logMsg("skipping blank key index");
		selectKeyRel(x, y);
	}
}

void VControllerKeyboard::unselectKey()
{
	selected = {{-1, -1}, {-1, -1}};
}

IG::WindowRect VControllerKeyboard::extendKeySelection(IG::WindowRect selected)
{
	auto key = currentKey(selected.x, selected.y);
	for(auto i : iotaCount(selected.x))
	{
		if(table[selected.y][selected.x - 1] == key)
			selected.x--;
		else
			break;
	}
	for(auto i : iotaCount((VKEY_COLS - 1) - selected.x2))
	{
		if(table[selected.y][selected.x2 + 1] == key)
			selected.x2++;
		else
			break;
	}
	logMsg("extended selection to:%d:%d", selected.x, selected.x2);
	return selected;
}

unsigned VControllerKeyboard::currentKey(int x, int y) const
{
	return table[y][x];
}

unsigned VControllerKeyboard::currentKey() const
{
	return currentKey(selected.x, selected.y);
}

void VControllerKeyboard::setMode(EmuSystem &sys, Gfx::Renderer &r, VControllerKbMode mode)
{
	mode_ = mode;
	updateImg(r);
	updateKeyboardMapping(sys);
}

void VControllerKeyboard::cycleMode(EmuSystem &sys, Gfx::Renderer &r)
{
	setMode(sys, r,
		mode() == VControllerKbMode::LAYOUT_1 ? VControllerKbMode::LAYOUT_2
		: VControllerKbMode::LAYOUT_1);
}

void VControllerKeyboard::applyMap(KbMap map)
{
	table = {};
	// 1st row
	auto *__restrict tablePtr = &table[0][0];
	auto *__restrict mapPtr = &map[0];
	for(auto i : iotaCount(10))
	{
		tablePtr[0] = *mapPtr;
		tablePtr[1] = *mapPtr;
		tablePtr += 2;
		mapPtr++;
	}
	// 2nd row
	mapPtr = &map[10];
	if(mode_ == VControllerKbMode::LAYOUT_1)
	{
		tablePtr = &table[1][1];
		for(auto i : iotaCount(9))
		{
			tablePtr[0] = *mapPtr;
			tablePtr[1] = *mapPtr;
			tablePtr += 2;
			mapPtr++;
		}
	}
	else
	{
		tablePtr = &table[1][0];
		for(auto i : iotaCount(10))
		{
			tablePtr[0] = *mapPtr;
			tablePtr[1] = *mapPtr;
			tablePtr += 2;
			mapPtr++;
		}
	}
	// 3rd row
	mapPtr = &map[20];
	table[2][0] = table[2][1] = table[2][2] = *mapPtr;
	mapPtr++;
	tablePtr = &table[2][3];
	for(auto i : iotaCount(7))
	{
		tablePtr[0] = *mapPtr;
		tablePtr[1] = *mapPtr;
		tablePtr += 2;
		mapPtr++;
	}
	table[2][17] = table[2][18] = table[2][19] = *mapPtr;
	// 4th row
	table[3][0] = table[3][1] = table[3][2] = VController::TOGGLE_KEYBOARD;
	table[3][3] = table[3][4] = table[3][5] = VController::CHANGE_KEYBOARD_MODE;
	tablePtr = &table[3][6];
	mapPtr = &map[33];
	for(auto i : iotaCount(8))
	{
		*tablePtr++ = *mapPtr;
	}
	mapPtr += 4;
	table[3][14] = table[3][15] = table[3][16] = *mapPtr;
	mapPtr += 2;
	table[3][17] = table[3][18] = table[3][19] = *mapPtr;

	/*iterateTimes(table.size(), i)
	{
		logMsg("row:%d", i);
		iterateTimes(table[0].size(), j)
		{
			logMsg("col:%d = %d", j, table[i][j]);
		}
	}*/
}

void VControllerKeyboard::updateKeyboardMapping(EmuSystem &sys)
{
	auto map = sys.vControllerKeyboardMap(mode());
	applyMap(map);
}

void VControllerKeyboard::setShiftActive(bool on)
{
	if(on)
	{
		shiftRect = selectKey(0, 2);
	}
	else
	{
		shiftRect = {{-1, -1}, {-1, -1}};
	}
}

bool VControllerKeyboard::toggleShiftActive()
{
	setShiftActive(shiftIsActive() ^ 1);
	return shiftIsActive();
}

bool VControllerKeyboard::shiftIsActive() const
{
	return shiftRect.x != -1;
}

}
