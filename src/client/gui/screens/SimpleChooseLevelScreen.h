#ifndef NET_MINECRAFT_CLIENT_GUI_SCREENS__DemoChooseLevelScreen_H__
#define NET_MINECRAFT_CLIENT_GUI_SCREENS__DemoChooseLevelScreen_H__

#include "ChooseLevelScreen.h"
#include "../components/TextBox.h"
class Button;

class SimpleChooseLevelScreen: public ChooseLevelScreen
{
public:
	SimpleChooseLevelScreen(const std::string& levelName);

	virtual ~SimpleChooseLevelScreen();

	void init();

	void setupPositions();

	void render(int xm, int ym, float a);

	void mouseClicked(int x, int y, int buttonNum);
	void keyPressed(int key);

	void buttonClicked(Button* button);
	bool handleBackEvent(bool isDown);

private:
	Button* bCreative;
	Button* bSurvival;
	Button* bBack;
	TextBox* nameBox;
	TextBox* seedBox;
	bool hasChosen;

	std::string levelName;
};

#endif /*NET_MINECRAFT_CLIENT_GUI_SCREENS__DemoChooseLevelScreen_H__*/
