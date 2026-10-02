#include "OptionsScreen.h"

#include "StartMenuScreen.h"
#include "DialogDefinitions.h"
#include "../../Minecraft.h"
#include "../../../AppPlatform.h"

#include "../components/OptionsPane.h"
#include "../components/ImageButton.h"
#include "../components/OptionsGroup.h"
#include "../components/TextBox.h"
#include "../../../platform/input/Mouse.h"
#include "../../../platform/input/Multitouch.h"
#include "../../../platform/input/Keyboard.h"
OptionsScreen::OptionsScreen()
: btnClose(NULL),
  bHeader(NULL),
  currentOptionPane(NULL),
  selectedCategory(0),
  nicknameBox(NULL),
  dragScrolling(false),
  dragStartY(0),
  dragPointerId(-1),
  dragLastY(0),
  dragMoved(false) {
}

OptionsScreen::~OptionsScreen() {
	if(btnClose != NULL) {
		delete btnClose;
		btnClose = NULL;
	}
	if(bHeader != NULL) {
		delete bHeader,
		bHeader = NULL;
	}
	for(std::vector<Touch::TButton*>::iterator it = categoryButtons.begin(); it != categoryButtons.end(); ++it) {
		if(*it != NULL) {
			delete *it;
			*it = NULL;
		}
	}
	for(std::vector<OptionsPane*>::iterator it = optionPanes.begin(); it != optionPanes.end(); ++it) {
		if(*it != NULL) {
			delete *it;
			*it = NULL;
		}
	}
	categoryButtons.clear();
	// nicknameBox is owned by Screen::textBoxes' lifetime here: it is a plain
	// screen element, not part of an options pane, so delete it with us.
	if(nicknameBox != NULL) {
		delete nicknameBox;
		nicknameBox = NULL;
	}
}

void OptionsScreen::init() {
	bHeader = new Touch::THeader(0, "Options");
	btnClose = new ImageButton(1, "");
	ImageDef def;
	def.name = "gui/touchgui.png";
	def.width = 34;
	def.height = 26;

	def.setSrc(IntRectangle(150, 0, (int)def.width, (int)def.height));
	btnClose->setImageDef(def, true);

	categoryButtons.push_back(new Touch::TButton(2, "Login"));
	categoryButtons.push_back(new Touch::TButton(3, "Game"));
	categoryButtons.push_back(new Touch::TButton(4, "Controls"));
	categoryButtons.push_back(new Touch::TButton(5, "Graphics"));
	buttons.push_back(bHeader);
	buttons.push_back(btnClose);
	for(std::vector<Touch::TButton*>::iterator it = categoryButtons.begin(); it != categoryButtons.end(); ++it) {
		buttons.push_back(*it);
		tabButtons.push_back(*it);
	}
	// Nickname entry. The Login tab used to show the "Mojang" group with a
	// duplicate sensitivity slider and nothing else.
	nicknameBox = new TextBox(100, minecraft->options.username);
	nicknameBox->x = 0;
	nicknameBox->y = 0;
	nicknameBox->w = 100;
	nicknameBox->h = 20;
	textBoxes.push_back(nicknameBox);

	generateOptionScreens();

}
void OptionsScreen::setupPositions() {
	int buttonHeight = btnClose->height;
	btnClose->x = width - btnClose->width;
	btnClose->y = 0;
	int offsetNum = 1;
	for(std::vector<Touch::TButton*>::iterator it = categoryButtons.begin(); it != categoryButtons.end(); ++it) {
		(*it)->x = 0;
		(*it)->y = offsetNum * buttonHeight;
		(*it)->selected = false;
		offsetNum++;
	}
	bHeader->x = 0;
	bHeader->y = 0;
	bHeader->width = width - btnClose->width;
	bHeader->height = btnClose->height;
	for(std::vector<OptionsPane*>::iterator it = optionPanes.begin(); it != optionPanes.end(); ++it) {
		if(categoryButtons.size() > 0 && categoryButtons[0] != NULL) {
			(*it)->x = categoryButtons[0]->width;
			(*it)->y = bHeader->height;
			(*it)->width = width - categoryButtons[0]->width;
			// Tell the pane how much of it is actually visible, otherwise it
			// cannot work out whether there is anything to scroll.
			(*it)->setViewHeight(height - (*it)->y);
			(*it)->setupPositions();
			(*it)->clampScroll();
		}
	}
	if(!optionPanes.empty() && nicknameBox != NULL) {
		OptionsPane* loginPane = optionPanes[0];
		nicknameBox->x = loginPane->x + 10;
		nicknameBox->y = loginPane->y + 14;
		nicknameBox->w = loginPane->width - 25;
		nicknameBox->h = 20;
	}
	selectCategory(0);
}

void OptionsScreen::render( int xm, int ym, float a ) {
	renderBackground();
	super::render(xm, ym, a);
	int xmm = xm * width / minecraft->width;
	int ymm = ym * height / minecraft->height - 1;
	if(currentOptionPane != NULL)
		currentOptionPane->render(minecraft, xmm, ymm);
	// The Login tab is a plain nickname field instead of an options group
	if(nicknameBox != NULL && currentOptionPane != NULL && !optionPanes.empty()
	   && currentOptionPane == optionPanes[0]) {
		minecraft->font->draw("Nickname", (float)nicknameBox->x, (float)(nicknameBox->y - 11), 0xffffffff, false);
		nicknameBox->render(minecraft, xmm, ymm);
	}
}

void OptionsScreen::removed()
{
	commitNickname();
}
void OptionsScreen::buttonClicked( Button* button ) {
	if(button == btnClose) {
		minecraft->reloadOptions();
		minecraft->screenChooser.setScreen(SCREEN_STARTMENU);
	} else if(button->id > 1 && button->id < 7) {
		// This is a category button
		int categoryButton = button->id - categoryButtons[0]->id;
		selectCategory(categoryButton);
	}
}

void OptionsScreen::selectCategory( int index ) {
	int currentIndex = 0;
	for(std::vector<Touch::TButton*>::iterator it = categoryButtons.begin(); it != categoryButtons.end(); ++it) {
		if(index == currentIndex) {
			(*it)->selected = true;
		} else {
			(*it)->selected = false;
		}
		currentIndex++;
	}
	if(index < (int)optionPanes.size())
		currentOptionPane = optionPanes[index];
}

void OptionsScreen::generateOptionScreens() {
	optionPanes.push_back(new OptionsPane());
	optionPanes.push_back(new OptionsPane());
	optionPanes.push_back(new OptionsPane());
	optionPanes.push_back(new OptionsPane());
	// Login pane: the nickname text box is placed by setupPositions(), so no
	// group is needed here.

	static const int diffArr[] = { 0, 1, 2, 3 };
	std::vector<int> diffVec(diffArr, diffArr + 4);
	static const int guiScaleArr[] = { 1, 2, 3, 4 };
	std::vector<int> guiScaleVec(guiScaleArr, guiScaleArr + 4);
	// Render distance is in chunks now (2..16)
	static const int renderDistArr[] = { 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 16 };
	std::vector<int> renderDistVec(renderDistArr, renderDistArr + 11);

	// Game Pane
	OptionsGroup& gameGroup = optionPanes[1]->createOptionsGroup("Game");
	optionPanes[1]->createStepSlider(minecraft, gameGroup, "Difficulty", &Options::Option::DIFFICULTY, diffVec);
	optionPanes[1]->createToggle(minecraft, gameGroup, "Third person camera", &Options::Option::THIRD_PERSON);
	optionPanes[1]->createToggle(minecraft, gameGroup, "Server visible", &Options::Option::SERVER_VISIBLE);

	OptionsGroup& soundGroup = optionPanes[1]->createOptionsGroup("Sound");
	optionPanes[1]->createProgressSlider(minecraft, soundGroup, "Music", &Options::Option::MUSIC);
	optionPanes[1]->createProgressSlider(minecraft, soundGroup, "Sound", &Options::Option::SOUND);

	// Input Pane
	OptionsGroup& controlsGroup = optionPanes[2]->createOptionsGroup("Controls");
	optionPanes[2]->createProgressSlider(minecraft, controlsGroup, "Mouse sensitivity", &Options::Option::SENSITIVITY);
	optionPanes[2]->createProgressSlider(minecraft, controlsGroup, "Touch sensitivity", &Options::Option::TOUCH_SENSITIVITY);
	optionPanes[2]->createToggle(minecraft, controlsGroup, "Invert Y axis", &Options::Option::INVERT_MOUSE);
	optionPanes[2]->createToggle(minecraft, controlsGroup, "Lefty", &Options::Option::LEFT_HANDED);
	optionPanes[2]->createToggle(minecraft, controlsGroup, "Use touch screen", &Options::Option::USE_TOUCHSCREEN);
	optionPanes[2]->createToggle(minecraft, controlsGroup, "Split touch controls", &Options::Option::USE_TOUCH_JOYPAD);
	optionPanes[2]->createToggle(minecraft, controlsGroup, "Touch buttons", &Options::Option::TOUCH_BUTTONS);
	static const int touchSizeArr[] = { 0, 1, 2, 3 };
	std::vector<int> touchSizeVec(touchSizeArr, touchSizeArr + 4);
	optionPanes[2]->createStepSlider(minecraft, controlsGroup, "Touch button size", &Options::Option::TOUCH_BUTTON_SIZE, touchSizeVec);

	OptionsGroup& feedBackGroup = optionPanes[2]->createOptionsGroup("Feedback");
	optionPanes[2]->createToggle(minecraft, feedBackGroup, "Vibrate on destroy", &Options::Option::DESTROY_VIBRATION);

	// Graphics Pane
	OptionsGroup& graphicsGroup = optionPanes[3]->createOptionsGroup("Graphics");
	optionPanes[3]->createStepSlider(minecraft, graphicsGroup, "Gui Scale", &Options::Option::GUI_SCALE, guiScaleVec);
	optionPanes[3]->createStepSlider(minecraft, graphicsGroup, "Render distance", &Options::Option::RENDER_DISTANCE, renderDistVec);
	optionPanes[3]->createToggle(minecraft, graphicsGroup, "Fog", &Options::Option::FOG);
	optionPanes[3]->createToggle(minecraft, graphicsGroup, "Fancy Graphics", &Options::Option::GRAPHICS);
	optionPanes[3]->createToggle(minecraft, graphicsGroup, "Ambient occlusion", &Options::Option::AMBIENT_OCCLUSION);
	optionPanes[3]->createToggle(minecraft, graphicsGroup, "View bobbing", &Options::Option::VIEW_BOBBING);
	optionPanes[3]->createToggle(minecraft, graphicsGroup, "Hide GUI", &Options::Option::HIDE_GUI);
	optionPanes[3]->createToggle(minecraft, graphicsGroup, "3D anaglyph", &Options::Option::ANAGLYPH);
	optionPanes[3]->createToggle(minecraft, graphicsGroup, "Limit framerate", &Options::Option::LIMIT_FRAMERATE);
	optionPanes[3]->createToggle(minecraft, graphicsGroup, "Show FPS", &Options::Option::SHOW_FPS);
}

void OptionsScreen::mouseClicked( int x, int y, int buttonNum ) {
	// Nickname field first: it lives in the pane area, so the pane would
	// otherwise swallow the press as a background drag.
	if(nicknameBox != NULL && nicknameBox->focused && buttonNum == MouseAction::ACTION_LEFT
	   && nicknameBox->isInside(x, y)) {
		super::mouseClicked(x, y, buttonNum);
		return;
	}
	if(nicknameBox != NULL && buttonNum == MouseAction::ACTION_LEFT && nicknameBox->isInside(x, y)) {
		commitNickname();
		nicknameBox->setFocus(minecraft);
		dragScrolling = false;
		super::mouseClicked(x, y, buttonNum);
		return;
	}
	if(nicknameBox != NULL && nicknameBox->focused) {
		nicknameBox->loseFocus(minecraft);
		commitNickname();
	}
	// A press on empty space starts a drag-scroll instead of hitting a widget.
	// Note: setupPositions() already subtracts scrollOffset from every child's
	// y, so all child bounding boxes are already in screen coordinates: do not
	// add getScrollOffset() here or the hit test will miss by 2x scroll offset.
	if(currentOptionPane != NULL && currentOptionPane->isPointInView(x, y)
	   && !currentOptionPane->hitTest(x, y)) {
		dragScrolling = true;
		dragStartY = y;
		dragLastY = y;
		dragMoved = false;
		dragPointerId = Multitouch::getFirstActivePointerIdEx();
		super::mouseClicked(x, y, buttonNum);
		return;
	}
	if(currentOptionPane != NULL)
		currentOptionPane->mouseClicked(minecraft, x, y, buttonNum);
	super::mouseClicked(x, y, buttonNum);
}

void OptionsScreen::mouseReleased( int x, int y, int buttonNum ) {
	if(dragScrolling) {
		// tick() already scrolled the pane while the finger moved, so the
		// release only has to end the drag.
		dragScrolling = false;
		if(!dragMoved && currentOptionPane != NULL) {
			// A tap on empty space: nudge it a little so a plain touch also
			// works, even when the finger barely moved.
			currentOptionPane->scrollBy(12);
		}
		super::mouseReleased(x, y, buttonNum);
		return;
	}
	if(currentOptionPane != NULL)
		currentOptionPane->mouseReleased(minecraft, x, y, buttonNum);
	super::mouseReleased(x, y, buttonNum);
}

bool OptionsScreen::mouseScrolled(int delta) {
	if(delta == 0 || currentOptionPane == NULL)
		return false;
	if(currentOptionPane->getMaxScroll() <= 0)
		return false;
	currentOptionPane->scrollBy(delta > 0 ? -20 : 20);
	return true;
}

void OptionsScreen::commitNickname() {
	if(nicknameBox == NULL)
		return;
	std::string name = Util::stringTrim(nicknameBox->text);
	if(name.empty() || (int)name.size() > TextBox::MAX_LENGTH) {
		// Keep the previous name instead of joining as an empty player.
		nicknameBox->text = minecraft->options.username;
		return;
	}
	if(name != minecraft->options.username) {
		minecraft->options.username = name;
		minecraft->options.save();
	}
}

void OptionsScreen::keyPressed( int eventKey ) {
	if(nicknameBox != NULL && nicknameBox->focused) {
		if(eventKey == Keyboard::KEY_BACKSPACE) {
			if(!nicknameBox->text.empty()) {
				nicknameBox->text.erase(nicknameBox->text.size() - 1);
				commitNickname();
			}
			return;
		}
		if(eventKey == Keyboard::KEY_RETURN) {
			commitNickname();
			nicknameBox->loseFocus(minecraft);
			return;
		}
	}
	super::keyPressed(eventKey);
}

void OptionsScreen::keyboardNewChar( char inputChar ) {
	if(nicknameBox != NULL && nicknameBox->focused) {
		if(inputChar >= 32 && (int)nicknameBox->text.size() < TextBox::MAX_LENGTH) {
			nicknameBox->text += inputChar;
			commitNickname();
		}
		return;
	}
	super::keyboardNewChar(inputChar);
}

void OptionsScreen::tick() {
	// Follow the finger while it is down instead of only jumping once on
	// release, otherwise the list stays put during the drag and a short swipe
	// moves nothing at all.
	if(dragScrolling && currentOptionPane != NULL) {
		const int* ids;
		int count = Multitouch::getActivePointerIds(&ids);
		bool stillDown = false;
		for(int i = 0; i < count; ++i) {
			if(ids[i] == dragPointerId) {
				stillDown = true;
				int y = Multitouch::getY(dragPointerId);
				int dy = dragLastY - y;
				if(dy != 0) {
					currentOptionPane->scrollBy(dy);
					dragLastY = y;
					if(dy > 0 || dy < 0)
						dragMoved = true;
				}
				break;
			}
		}
		if(!stillDown)
			dragScrolling = false;
	}
	if(currentOptionPane != NULL)
		currentOptionPane->tick(minecraft);
	super::tick();
}
