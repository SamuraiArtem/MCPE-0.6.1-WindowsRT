#include "SimpleChooseLevelScreen.h"
#include "ProgressScreen.h"
#include "ScreenChooser.h"
#include "../components/Button.h"
#include "../../Minecraft.h"
#include "../../../world/level/LevelSettings.h"
#include "../../../platform/time.h"
#include "../../../util/StringUtils.h"

SimpleChooseLevelScreen::SimpleChooseLevelScreen(const std::string& levelName)
:	bCreative(0),
	bSurvival(0),
	bBack(0),
	nameBox(NULL),
	seedBox(NULL),
	levelName(levelName),
	hasChosen(false)
{
}

SimpleChooseLevelScreen::~SimpleChooseLevelScreen()
{
	delete bCreative;
	delete bSurvival;
	delete bBack;
	if (nameBox) { delete nameBox; nameBox = NULL; }
	if (seedBox) { delete seedBox; seedBox = NULL; }
}

void SimpleChooseLevelScreen::init()
{
	if (minecraft->useTouchscreen()) {
		bCreative = new Touch::TButton(1, "Creative");
		bSurvival = new Touch::TButton(2, "Survival");
		bBack	  = new Touch::TButton(3, "Back");
	} else {
		bCreative = new Button(1, "Creative");
		bSurvival = new Button(2, "Survival");
		bBack	  = new Button(3, "Back");
	}
	buttons.push_back(bCreative);
	buttons.push_back(bSurvival);
	buttons.push_back(bBack);

	tabButtons.push_back(bCreative);
	tabButtons.push_back(bSurvival);
	tabButtons.push_back(bBack);

	nameBox = new TextBox(101, "My World");
	nameBox->w = 160;
	nameBox->h = 20;
	textBoxes.push_back(nameBox);

	seedBox = new TextBox(102, "");
	seedBox->w = 160;
	seedBox->h = 20;
	textBoxes.push_back(seedBox);
}

void SimpleChooseLevelScreen::setupPositions()
{
	int cx = (width - 160) / 2;
	if (cx < 10) cx = 10;

	nameBox->x = cx;
	nameBox->y = 26;
	nameBox->w = 160;

	seedBox->x = cx;
	seedBox->y = 66;
	seedBox->w = 160;

	bCreative->width = 76;
	bCreative->x = cx;
	bCreative->y = 102;

	bSurvival->width = 76;
	bSurvival->x = cx + 84;
	bSurvival->y = 102;

	bBack->width = 160;
	bBack->x = cx;
	bBack->y = height - 32;
}

void SimpleChooseLevelScreen::render( int xm, int ym, float a )
{
	renderDirtBackground(0);
    glEnable2(GL_BLEND);

	int cx = (width - 160) / 2;
	if (cx < 10) cx = 10;

	minecraft->font->draw("World Name:", (float)cx, (float)(nameBox->y - 11), 0xffffff, false);
	minecraft->font->draw("Seed (optional):", (float)cx, (float)(seedBox->y - 11), 0xffffff, false);

	nameBox->render(minecraft, xm, ym);
	seedBox->render(minecraft, xm, ym);

	Screen::render(xm, ym, a);
    glDisable2(GL_BLEND);
}

void SimpleChooseLevelScreen::mouseClicked(int x, int y, int buttonNum)
{
	if (nameBox && nameBox->isInside(x, y)) {
		nameBox->setFocus(minecraft);
		if (seedBox) seedBox->loseFocus(minecraft);
	} else if (seedBox && seedBox->isInside(x, y)) {
		seedBox->setFocus(minecraft);
		if (nameBox) nameBox->loseFocus(minecraft);
	} else {
		if (nameBox && nameBox->focused) nameBox->loseFocus(minecraft);
		if (seedBox && seedBox->focused) seedBox->loseFocus(minecraft);
	}
	Screen::mouseClicked(x, y, buttonNum);
}

void SimpleChooseLevelScreen::keyPressed(int key)
{
	Screen::keyPressed(key);
}

static char ILLEGAL_CHARS[] = {
	'/', '\n', '\r', '\t', '\0', '\f', '`', '?', '*', '\\', '<', '>', '|', '\"', ':'
};

void SimpleChooseLevelScreen::buttonClicked( Button* button )
{
	if (button == bBack) {
		minecraft->screenChooser.setScreen(SCREEN_STARTMENU);
		return;
	}
	if (hasChosen)
		return;

	int gameType = GameType::Survival;
	if (button == bCreative)
		gameType = GameType::Creative;
	if (button == bSurvival)
		gameType = GameType::Survival;

	std::string customName = nameBox ? Util::stringTrim(nameBox->text) : "";
	if (customName.empty())
		customName = "World";

	std::string cleanId = customName;
	for (unsigned int i = 0; i < sizeof(ILLEGAL_CHARS); ++i)
		cleanId = Util::stringReplace(cleanId, std::string(1, ILLEGAL_CHARS[i]), "");
	if (cleanId.empty())
		cleanId = "world";

	std::string levelId = getUniqueLevelName(cleanId);

	int seed = getEpochTimeS();
	if (seedBox) {
		std::string seedStr = Util::stringTrim(seedBox->text);
		if (!seedStr.empty()) {
			int tmpSeed;
			if (sscanf(seedStr.c_str(), "%d", &tmpSeed) > 0)
				seed = tmpSeed;
			else
				seed = Util::hashCode(seedStr);
		}
	}

	LevelSettings settings(seed, gameType);
	minecraft->selectLevel(levelId, customName, settings);
	minecraft->hostMultiplayer();
	minecraft->setScreen(new ProgressScreen());
	hasChosen = true;
}

bool SimpleChooseLevelScreen::handleBackEvent(bool isDown) {
	if (!isDown)
		minecraft->screenChooser.setScreen(SCREEN_STARTMENU);
	return true;
}
