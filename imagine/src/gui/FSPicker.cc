/*  This file is part of Imagine.

	Imagine is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Imagine is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Imagine.  If not, see <http://www.gnu.org/licenses/> */

#define LOGTAG "FSPicker"

#include <imagine/gui/FSPicker.hh>
#include <imagine/gui/TextTableView.hh>
#include <imagine/gui/TextEntry.hh>
#include <imagine/gui/NavView.hh>
#include <imagine/fs/FS.hh>
#include <imagine/base/ApplicationContext.hh>
#include <imagine/gfx/RendererCommands.hh>
#include <imagine/gfx/BasicEffect.hh>
#include <imagine/logger/logger.h>
#include <imagine/util/math/int.hh>
#include <imagine/util/format.hh>
#include <imagine/util/string.h>
#include <string>
#include <system_error>

namespace IG
{

FSPicker::FSPicker(ViewAttachParams attach, Gfx::TextureSpan backRes, Gfx::TextureSpan closeRes,
	FilterFunc filter, Mode mode, Gfx::GlyphTextureSet *face_):
	View{attach},
	filter{filter},
	msgText{face_ ? face_ : &defaultFace()},
	mode_{mode}
{
	auto nav = makeView<BasicNavView>
		(
			&face(),
			isSingleDirectoryMode() ? nullptr : backRes,
			closeRes
		);
	const Gfx::LGradientStopDesc fsNavViewGrad[]
	{
		{ .0, Gfx::VertexColorPixelFormat.build(1. * .4, 1. * .4, 1. * .4, 1.) },
		{ .3, Gfx::VertexColorPixelFormat.build(1. * .4, 1. * .4, 1. * .4, 1.) },
		{ .97, Gfx::VertexColorPixelFormat.build(.35 * .4, .35 * .4, .35 * .4, 1.) },
		{ 1., nav->separatorColor() },
	};
	nav->setBackgroundGradient(fsNavViewGrad);
	nav->setCenterTitle(false);
	nav->setOnPushLeftBtn(
		[this](const Input::Event &e)
		{
			onLeftNavBtn(e);
		});
	nav->setOnPushRightBtn(
		[this](const Input::Event &e)
		{
			onRightNavBtn(e);
		});
	nav->setOnPushMiddleBtn(
		[this](const Input::Event &e)
		{
			pushFileLocationsView(e);
		});
	controller.setNavView(std::move(nav));
	controller.push(makeView<TableView>([](const TableView &) { return 0; },
		[&d = dir](const TableView &, size_t idx) -> MenuItem& { return d[idx].text; }));
	controller.navView()->showLeftBtn(true);
	dir.reserve(16); // start with some initial capacity to avoid small reallocations
}

void FSPicker::place()
{
	controller.place(viewRect(), displayRect(), projP);
	if(dirListThread.isWorking())
		return;
	msgText.compile(renderer(), projP);
}

void FSPicker::changeDirByInput(IG::CStringView path, FS::RootPathInfo rootInfo, const Input::Event &e)
{
	setPath(path, std::move(rootInfo), e);
	place();
	postDraw();
}

void FSPicker::setOnChangePath(OnChangePathDelegate del)
{
	onChangePath_ = del;
}

void FSPicker::setOnSelectPath(OnSelectPathDelegate del)
{
	onSelectPath_ = del;
}

void FSPicker::onLeftNavBtn(const Input::Event &e)
{
	if(!isAtRoot())
	{
		goUpDirectory(e);
	}
	else
	{
		pushFileLocationsView(e);
	}
}

void FSPicker::onRightNavBtn(const Input::Event &e)
{
	if(mode_ == Mode::DIR)
		onSelectPath_.callCopy(*this, root.path, appContext().fileUriDisplayName(root.path), e);
	else
		dismiss();
}

bool FSPicker::inputEvent(const Input::Event &e)
{
	if(e.keyEvent())
	{
		auto &keyEv = e.asKeyEvent();
		if(keyEv.pushed(Input::DefaultKey::CANCEL))
		{
			dismiss();
			return true;
		}
		else if(controller.viewHasFocus() && keyEv.pushed(Input::DefaultKey::LEFT))
		{
			controller.moveFocusToNextView(e, CT2DO);
			controller.top().setFocus(false);
			return true;
		}
		else if(keyEv.pushedKey(Input::Keycode::GAME_B) || keyEv.pushedKey(Input::Keycode::F1))
		{
			pushFileLocationsView(e);
			return true;
		}
	}
	return controller.inputEvent(e);
}

void FSPicker::prepareDraw()
{
	controller.navView()->prepareDraw();
	controller.top().prepareDraw();
	if(dirListThread.isWorking())
		return;
	msgText.makeGlyphs(renderer());
}

void FSPicker::draw(Gfx::RendererCommands &cmds)
{
	if(!dirListThread.isWorking())
	{
		if(dir.size())
		{
			controller.top().draw(cmds);
		}
		else
		{
			using namespace IG::Gfx;
			cmds.set(ColorName::WHITE);
			cmds.basicEffect().enableAlphaTexture(cmds);
			auto textRect = controller.top().viewRect();
			if(IG::isOdd(textRect.ySize()))
				textRect.y2--;
			msgText.draw(cmds, projP.unProjectRect(textRect).pos(C2DO), C2DO, projP);
		}
	}
	controller.navView()->draw(cmds);
}

void FSPicker::onAddedToController(ViewController *, const Input::Event &e)
{
	controller.top().onAddedToController(&controller, e);
}

void FSPicker::setEmptyPath()
{
	logMsg("setting empty path");
	dirListThread.stop();
	dirListEvent.cancel();
	root = {};
	dir.clear();
	msgText.setString("No folder is set");
	if(mode_ == Mode::FILE_IN_DIR)
	{
		fileTableView().setName({});
	}
	else
	{
		fileTableView().setName("Select File Location");
	}
}

void FSPicker::setPath(IG::CStringView path, FS::RootPathInfo rootInfo, const Input::Event &e)
{
	if(!strlen(path))
	{
		setEmptyPath();
		return;
	}
	highlightFirstDirEntry = e.keyEvent();
	startDirectoryListThread(path);
	root.path = path;
	auto pathLen = path.size();
	// verify root info
	if(rootInfo.length && rootInfo.length > pathLen)
	{
		logWarn("invalid root length:%zu with path length:%zu", rootInfo.length, pathLen);
		rootInfo.length = 0;
	}
	// if the path is a URI and no root info is provided, root at the URI itself
	bool isUri = IG::isUri(path);
	if(!rootInfo.length && isUri)
	{
		rootInfo = {appContext().fileUriDisplayName(path), path.size()};
	}
	FS::PathString rootedPath{};
	if(rootInfo.length)
	{
		logMsg("root info:%d:%s", (int)rootInfo.length, rootInfo.name.data());
		root.info = rootInfo;
		if(pathLen > rootInfo.length)
			rootedPath = IG::format<FS::PathString>("{}{}", rootInfo.name, &path[rootInfo.length]);
		else
			rootedPath = rootInfo.name;
	}
	else
	{
		logMsg("no root info");
		root.info = {};
		rootedPath = root.path;
	}
	if(isUri)
	{
		rootedPath = IG::decodeUri<FS::PathString>(rootedPath);
	}
	fileTableView().setName(rootedPath);
	onChangePath_.callSafe(*this, e);
}

void FSPicker::setPath(IG::CStringView path, FS::RootPathInfo rootInfo)
{
	return setPath(path, std::move(rootInfo), appContext().defaultInputEvent());
}

void FSPicker::setPath(IG::CStringView path, const Input::Event &e)
{
	return setPath(path, appContext().rootPathInfo(path), e);
}

void FSPicker::setPath(IG::CStringView path)
{
	return setPath(path, appContext().rootPathInfo(path));
}

FS::PathString FSPicker::path() const
{
	return root.path;
}

FS::RootedPath FSPicker::rootedPath() const
{
	return root;
}

void FSPicker::clearSelection()
{
	controller.top().clearSelection();
}

bool FSPicker::isSingleDirectoryMode() const
{
	return mode_ == Mode::FILE_IN_DIR;
}

void FSPicker::goUpDirectory(const Input::Event &e)
{
	clearSelection();
	changeDirByInput(FS::dirnameUri(root.path), root.info, e);
}

bool FSPicker::isAtRoot() const
{
	if(root.info.length)
	{
		return root.pathIsRoot();
	}
	else
	{
		return root.path.empty() || root.path == "/";
	}
}

void FSPicker::pushFileLocationsView(const Input::Event &e)
{
	if(isSingleDirectoryMode())
		return;
	class FileLocationsTextTableView : public TextTableView
	{
	public:
		FileLocationsTextTableView(ViewAttachParams attach,
			std::vector<FS::PathLocation> locations, size_t customItems):
				TextTableView{"File Locations", attach, locations.size() + customItems},
				locations_{std::move(locations)} {}
		const std::vector<FS::PathLocation> &locations() const { return locations_; }

	protected:
		std::vector<FS::PathLocation> locations_;
	};

	int customItems = 1 + Config::envIsLinux + appContext().hasSystemPathPicker() + appContext().hasSystemDocumentPicker();
	auto view = makeView<FileLocationsTextTableView>(appContext().rootFileLocations(), customItems);
	if(appContext().hasSystemPathPicker())
	{
		view->appendItem("Browse For Folder",
			[this](View &view, const Input::Event &e)
			{
				appContext().showSystemPathPicker(
					[this, &view](IG::CStringView uri, IG::CStringView displayName)
					{
						view.dismiss();
						if(mode_ == Mode::DIR)
							onSelectPath_.callCopy(*this, uri, displayName, appContext().defaultInputEvent());
						else
							changeDirByInput(uri, appContext().rootPathInfo(uri), appContext().defaultInputEvent());
					});
			});
	}
	if(mode_ != Mode::DIR && appContext().hasSystemDocumentPicker())
	{
		view->appendItem("Browse For File",
			[this](View &view, const Input::Event &e)
			{
				appContext().showSystemDocumentPicker(
					[this, &view](IG::CStringView uri, IG::CStringView displayName)
					{
						onSelectPath_.callCopy(*this, uri, displayName, appContext().defaultInputEvent());
					});
			});
	}
	for(auto &loc : view->locations())
	{
		view->appendItem(loc.description,
			[this, &loc](View &view, const Input::Event &e)
			{
				auto ctx = appContext();
				if(ctx.usesPermission(Permission::WRITE_EXT_STORAGE))
				{
					if(!ctx.requestPermission(Permission::WRITE_EXT_STORAGE))
						return;
				}
				changeDirByInput(loc.root.path, loc.root.info, e);
				view.dismiss();
			});
	}
	if(Config::envIsLinux)
	{
		view->appendItem("Root Filesystem",
			[this](View &view, const Input::Event &e)
			{
				changeDirByInput("/", {}, e);
				view.dismiss();
			});
	}
	view->appendItem("Custom Path",
		[this](const Input::Event &e)
		{
			auto textInputView = makeView<CollectTextInputView>(
				"Input a directory path", root.path, nullptr,
				[this](CollectTextInputView &view, const char *str)
				{
					if(!str || !strlen(str))
					{
						view.dismiss();
						return false;
					}
					changeDirByInput(str, appContext().rootPathInfo(str), appContext().defaultInputEvent());
					dismissPrevious();
					view.dismiss();
					return false;
				});
			pushAndShow(std::move(textInputView), e);
		});
	pushAndShow(std::move(view), e);
}

Gfx::GlyphTextureSet &FSPicker::face()
{
	return *msgText.face();
}

TableView &FSPicker::fileTableView()
{
	return static_cast<TableView&>(controller.top());
}

void FSPicker::setShowHiddenFiles(bool on)
{
	showHiddenFiles_ = on;
}

void FSPicker::startDirectoryListThread(CStringView path)
{
	if(dirListThread.isWorking())
	{
		logMsg("deferring listing directory until worker thread stops");
		dirListThread.requestStop();
		dirListEvent.setCallback([this]()
		{
			startDirectoryListThread(root.path);
		});
		return;
	}
	dir.clear();
	fileTableView().setItemsDelegate();
	dirListEvent.setCallback([this]()
	{
		fileTableView().setItemsDelegate([&d = dir](const TableView &) { return d.size(); });
		if(highlightFirstDirEntry)
			fileTableView().highlightCell(0);
		else
			fileTableView().resetScroll();
		place();
		postDraw();
	});
	dirListEvent.cancel();
	dirListThread.reset([this](WorkThread::Context ctx, const std::string &path)
	{
		listDirectory(path, ctx.stop);
		if(ctx.stop.isQuitting()) [[unlikely]]
			return;
		ctx.finishedWork();
		dirListEvent.notify();
	}, std::string{path});
}

void FSPicker::listDirectory(IG::CStringView path, ThreadStop &stop)
{
	try
	{
		appContext().forEachInDirectoryUri(path,
			[this, &stop](auto &entry)
			{
				//logMsg("entry:%s", entry.path().data());
				if(stop) [[unlikely]]
				{
					logMsg("interrupted listing directory");
					return false;
				}
				bool isDir = entry.type() == FS::file_type::directory;
				if(mode_ == Mode::DIR) // filter non-directories
				{
					if(!isDir)
						return true;
				}
				else if(mode_ == Mode::FILE_IN_DIR) // filter directories
				{
					if(isDir)
						return true;
				}
				if(!showHiddenFiles_ && entry.name().starts_with('.'))
				{
					return true;
				}
				if(filter && !filter(entry))
				{
					return true;
				}
				auto &item = dir.emplace_back(FileEntry{std::string{entry.path()}, {entry.name(), &face(), nullptr}});
				if(isDir)
					item.text.setFlags(item.text.flags() | FileEntry::IS_DIR_FLAG);
				return true;
			});
		std::sort(dir.begin(), dir.end(),
			[](const FileEntry &e1, const FileEntry &e2)
			{
				if(e1.isDir() && !e2.isDir())
					return true;
				else if(!e1.isDir() && e2.isDir())
					return false;
				else
					return IG::stringNoCaseLexCompare(e1.path, e2.path);
			});
		if(dir.size())
		{
			for(auto &d : dir)
			{
				if(d.isDir())
				{
					d.text.setOnSelect(
						[this, &dirPath = d.path](const Input::Event &e)
						{
							assert(!isSingleDirectoryMode());
							auto path = std::move(dirPath);
							logMsg("entering dir:%s", path.data());
							changeDirByInput(path, root.info, e);
						});
				}
				else
				{
					d.text.setOnSelect(
						[this, &dirPath = d.path](const Input::Event &e)
						{
							onSelectPath_.callCopy(*this, dirPath, appContext().fileUriDisplayName(dirPath), e);
						});
				}
			}
			msgText.setString({});
		}
		else // no entries, show a message instead
		{
			msgText.setString("Empty Directory");
		}
	}
	catch(std::system_error &err)
	{
		logErr("can't open %s", path.data());
		auto ec = err.code();
		std::string_view extraMsg = mode_ == Mode::FILE_IN_DIR ? "" : "\nPick a path from the top bar";
		msgText.setString(fmt::format("Can't open directory:\n{}{}", ec.message(), extraMsg));
	}
}

}
