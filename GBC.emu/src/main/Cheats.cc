#include <Cheats.hh>
#include <MsgPopup.hh>
#include <TextEntry.hh>
#include <EmuApp.hh>
#include "EmuCheatViews.hh"
#include <main/Cheats.hh>
#include <gambatte.h>
extern gambatte::GB gbEmu;
StaticArrayList<GbcCheat, EmuCheats::MAX> cheatList;
bool cheatsModified = 0;

static bool strIsGGCode(const char *str)
{
	return strlen(str) == 11 && str[3] == '-' && str[7] == '-' &&
		string_isHexValue(&str[0], 3) &&
		string_isHexValue(&str[4], 3) &&
		string_isHexValue(&str[8], 3);
}

static bool strIsGSCode(const char *str)
{
	return strlen(str) == 8 && string_isHexValue(str, 8);
}

void applyCheats()
{
	if(EmuSystem::gameIsRunning())
	{
		std::string ggCodeStr, gsCodeStr;
		for(auto &e : cheatList)
		{
			if(!e.isOn())
				continue;
			std::string &codeStr = strstr(e.code, "-") ? ggCodeStr : gsCodeStr;
			if(codeStr.size())
				codeStr += ";";
			codeStr += e.code;
		}
		gbEmu.setGameGenie(ggCodeStr);
		gbEmu.setGameShark(gsCodeStr);
		if(ggCodeStr.size())
			logMsg("set GG codes: %s", ggCodeStr.c_str());
		if(gsCodeStr.size())
			logMsg("set GS codes: %s", gsCodeStr.c_str());
	}
}

void writeCheatFile()
{
	if(!cheatsModified)
		return;

	FsSys::cPath filename;
	sprintf(filename, "%s/%s.gbcht", EmuSystem::savePath(), EmuSystem::gameName());

	if(!cheatList.size())
	{
		logMsg("deleting cheats file %s", filename);
		FsSys::remove(filename);
		cheatsModified = 0;
		return;
	}

	auto file = IoSys::create(filename);
	if(!file)
	{
		logMsg("error creating cheats file %s", filename);
		return;
	}
	logMsg("writing cheats file %s", filename);

	int version = 0;
	file->writeVar((uint8)version);
	file->writeVar((uint16)cheatList.size());
	for(auto &e : cheatList)
	{
		file->writeVar((uint8)e.flags);
		file->writeVar((uint16)strlen(e.name));
		file->fwrite(e.name, strlen(e.name), 1);
		file->writeVar((uint8)strlen(e.code));
		file->fwrite(e.code, strlen(e.code), 1);
	}
	file->close();
	cheatsModified = 0;
}

void readCheatFile()
{
	FsSys::cPath filename;
	sprintf(filename, "%s/%s.gbcht", EmuSystem::savePath(), EmuSystem::gameName());
	auto file = IoSys::open(filename);
	if(!file)
	{
		return;
	}
	logMsg("reading cheats file %s", filename);

	uint8 version = 0;
	file->readVar(version);
	if(version != 0)
	{
		logMsg("skipping due to version code %d", version);
		file->close();
		return;
	}
	uint16 size = 0;
	file->readVar(size);
	iterateTimes(size, i)
	{
		if(cheatList.isFull())
		{
			logMsg("cheat list full while reading from file");
			break;
		}
		GbcCheat cheat;
		uint8 flags = 0;
		file->readVar(flags);
		cheat.flags = flags;
		uint16 nameLen = 0;
		file->readVar(nameLen);
		file->read(cheat.name, std::min(uint16(sizeof(cheat.name)-1), nameLen));
		uint8 codeLen = 0;
		file->readVar(codeLen);
		file->read(cheat.code, std::min(uint8(sizeof(cheat.code)-1), codeLen));
		cheatList.push_back(cheat);
	}
	file->close();
}

void SystemEditCheatView::renamed(const char *str)
{
	string_copy(cheat->name, str);
	cheatsModified = 1;
}

void SystemEditCheatView::removed()
{
	cheatList.remove(*cheat);
	cheatsModified = 1;
	refreshCheatViews();
	applyCheats();
}

void SystemEditCheatView::init(bool highlightFirst, GbcCheat &cheat)
{
	this->cheat = &cheat;

	uint i = 0;
	loadNameItem(cheat.name, item, i);
	ggCode.init(cheat.code); item[i++] = &ggCode;
	loadRemoveItem(item, i);
	assert(i <= sizeofArray(item));
	BaseMenuView::init(item, i, highlightFirst);
}

SystemEditCheatView::SystemEditCheatView(Base::Window &win):
	EditCheatView("Edit Code", win),
	ggCode
	{
		"Code",
		[this](DualTextMenuItem &item, const Input::Event &e)
		{
			auto &textInputView = *allocModalView<CollectTextInputView>(window());
			textInputView.init("Input xxxxxxxx (GS) or xxx-xxx-xxx (GG) code", cheat->code, getCollectTextCloseAsset());
			textInputView.onText() =
				[this](CollectTextInputView &view, const char *str)
				{
					if(str)
					{
						if(!strIsGGCode(str) && !strIsGSCode(str))
						{
							popup.postError("Invalid format");
							window().postDraw();
							return 1;
						}
						string_copy(cheat->code, str);
						string_toUpper(cheat->code);
						cheatsModified = 1;
						applyCheats();
						ggCode.compile();
						window().postDraw();
					}
					view.dismiss();
					return 0;
				};
			modalViewController.pushAndShow(textInputView);
		}
	}
{}

void EditCheatListView::loadAddCheatItems(MenuItem *item[], uint &items)
{
	addGGGS.init(); item[items++] = &addGGGS;
}

void EditCheatListView::loadCheatItems(MenuItem *item[], uint &items)
{
	int cheats = std::min(cheatList.size(), (uint)sizeofArray(cheat));
	auto it = cheatList.begin();
	iterateTimes(cheats, c)
	{
		auto &thisCheat = *it;
		cheat[c].init(thisCheat.name); item[items++] = &cheat[c];
		cheat[c].onSelect() =
			[this, c](TextMenuItem &, const Input::Event &e)
			{
				auto &editCheatView = *menuAllocator.allocNew<SystemEditCheatView>(window());
				editCheatView.init(!e.isPointer(), cheatList[c]);
				viewStack.pushAndShow(editCheatView, &menuAllocator);
			};
		++it;
	}
}

EditCheatListView::EditCheatListView(Base::Window &win):
	BaseEditCheatListView(win),
	addGGGS
	{
		"Add Game Genie / GameShark Code",
		[this](TextMenuItem &item, const Input::Event &e)
		{
			auto &textInputView = *allocModalView<CollectTextInputView>(window());
			textInputView.init("Input xxxxxxxx (GS) or xxx-xxx-xxx (GG) code", getCollectTextCloseAsset());
			textInputView.onText() =
				[this](CollectTextInputView &view, const char *str)
				{
					if(str)
					{
						if(cheatList.isFull())
						{
							popup.postError("Cheat list is full");
							view.dismiss();
							return 0;
						}
						if(!strIsGGCode(str) && !strIsGSCode(str))
						{
							popup.postError("Invalid format");
							window().postDraw();
							return 1;
						}
						GbcCheat c;
						string_copy(c.code, str);
						string_toUpper(c.code);
						string_copy(c.name, "Unnamed Cheat");
						cheatList.push_back(c);
						logMsg("added new cheat, %d total", cheatList.size());
						cheatsModified = 1;
						applyCheats();
						view.dismiss();
						refreshCheatViews();

						auto &textInputView = *allocModalView<CollectTextInputView>(window());
						textInputView.init("Input description", getCollectTextCloseAsset());
						textInputView.onText() =
							[this](CollectTextInputView &view, const char *str)
							{
								if(str)
								{
									string_copy(cheatList.back().name, str);
									view.dismiss();
									refreshCheatViews();
								}
								else
								{
									view.dismiss();
								}
								return 0;
							};
						modalViewController.pushAndShow(textInputView);
					}
					else
					{
						view.dismiss();
					}
					return 0;
				};
			modalViewController.pushAndShow(textInputView);
		}
	}
{}

void CheatsView::loadCheatItems(MenuItem *item[], uint &i)
{
	int cheats = std::min(cheatList.size(), (uint)sizeofArray(cheat));
	auto it = cheatList.begin();
	iterateTimes(cheats, cIdx)
	{
		auto &thisCheat = *it;
		cheat[cIdx].init(thisCheat.name, thisCheat.isOn()); item[i++] = &cheat[cIdx];
		cheat[cIdx].onSelect() =
			[this, cIdx](BoolMenuItem &item, const Input::Event &e)
			{
				item.toggle(*this);
				auto &c = cheatList[cIdx];
				c.toggleOn();
				cheatsModified = 1;
				applyCheats();
			};
		logMsg("added cheat %s : %s", thisCheat.name, thisCheat.code);
		++it;
	}
}
