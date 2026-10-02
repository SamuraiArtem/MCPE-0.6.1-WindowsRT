#ifndef NET_MINECRAFT_CLIENT_GUI_SCREENS__OptionsScreen_H__
#define NET_MINECRAFT_CLIENT_GUI_SCREENS__OptionsScreen_H__

#include "../Screen.h"
#include "../components/Button.h"

class ImageButton;
class OptionsPane;
class TextBox;

class OptionsScreen: public Screen
{
	typedef Screen super;
	void init();

	void generateOptionScreens();

public:
	OptionsScreen();
	~OptionsScreen();
	void setupPositions();
	void buttonClicked( Button* button );
	void render(int xm, int ym, float a);
	void removed();
	void selectCategory(int index);

	virtual void mouseClicked( int x, int y, int buttonNum );
	virtual void mouseReleased( int x, int y, int buttonNum );
	virtual bool mouseScrolled(int delta);
	virtual void keyPressed(int eventKey);
	virtual void keyboardNewChar(char inputChar);
	virtual void tick();
private:
	void commitNickname();
	Touch::THeader* bHeader;
	ImageButton* btnClose;
	std::vector<Touch::TButton*> categoryButtons;
	std::vector<OptionsPane*> optionPanes;
	OptionsPane* currentOptionPane;
	int selectedCategory;
	// Nickname entry, shown instead of the old "options.group.mojang" group
	TextBox* nicknameBox;
	// Drag-to-scroll state for the options panes
	bool dragScrolling;
	int dragStartY;
	int dragPointerId;
	int dragLastY;
	// A tap that never moved counts as a tap, not as a scroll.
	bool dragMoved;
};

#endif /*NET_MINECRAFT_CLIENT_GUI_SCREENS__OptionsScreen_H__*/
