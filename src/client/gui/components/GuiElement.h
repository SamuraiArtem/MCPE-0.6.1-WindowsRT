#ifndef NET_MINECRAFT_CLIENT_GUI__GuiElement_H__
#define NET_MINECRAFT_CLIENT_GUI__GuiElement_H__
#include "../GuiComponent.h"

class Tesselator;
class Minecraft;

class GuiElement : public GuiComponent {
public:
	GuiElement(bool active=false, bool visible=true, int x = 0, int y = 0, int width=24, int height=24);
    virtual ~GuiElement() {}
    virtual void tick(Minecraft* minecraft) {}
    virtual void render(Minecraft* minecraft, int xm, int ym) { }
	virtual void setupPositions() {}
	virtual void mouseClicked(Minecraft* minecraft, int x, int y, int buttonNum) {}
	virtual void mouseReleased(Minecraft* minecraft, int x, int y, int buttonNum) {}
	virtual bool pointInside(int x, int y);
	// Does this element (or, for containers, any of its children) cover the
	// point? Used to tell a drag on empty space from a press on a control.
	virtual bool contains(int px, int py) {
		return px >= x && px < x + width && py >= y && py < y + height;
	}
	void setVisible(bool visible);
	bool active;
	bool visible;
	int x;
	int y;
	int width;
	int height;
};

#endif /*NET_MINECRAFT_CLIENT_GUI__GuiElement_H__*/
