#ifndef NET_MINECRAFT_CLIENT_GUI_COMPONENTS__TextBox_H__
#define NET_MINECRAFT_CLIENT_GUI_COMPONENTS__TextBox_H__

//package net.minecraft.client.gui;

#include <string>
#include "../GuiComponent.h"
#include "../../Options.h"

class Font;
class Minecraft;

class TextBox: public GuiComponent
{
public:
	TextBox(int id, const std::string& msg);
    TextBox(int id, int x, int y, const std::string& msg);
    TextBox(int id, int x, int y, int w, int h, const std::string& msg);

	virtual void setFocus(Minecraft* minecraft);
	virtual bool loseFocus(Minecraft* minecraft);

    virtual void render(Minecraft* minecraft, int xm, int ym);
	bool isInside(int px, int py) const {
		return px >= x && px < x + w && py >= y && py < y + h;
	}
	static const int MAX_LENGTH = 16;
	
public:
	int w, h;
	int x, y;

	std::string text;
	int id;
	bool focused;
};

#endif /*NET_MINECRAFT_CLIENT_GUI_COMPONENTS__TextBox_H__*/
