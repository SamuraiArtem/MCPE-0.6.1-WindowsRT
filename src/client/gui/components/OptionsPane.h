#ifndef ITEMPANE_H__
#define ITEMPANE_H__

#include <string>
#include <vector>
#include "GuiElementContainer.h"
#include "../../../world/item/ItemInstance.h"
#include "../../../client/Options.h"
class Font; 
class Textures;
class NinePatchLayer;
class ItemPane;
class OptionButton;
class Button;
class OptionsGroup;
class Slider;
class Minecraft;
class OptionsPane: public GuiElementContainer
{
	typedef GuiElementContainer super;
public:
	OptionsPane();
	OptionsGroup& createOptionsGroup( std::string label );
	void createToggle(Minecraft* minecraft, OptionsGroup& group, std::string label, const Options::Option* option );
	void createProgressSlider(Minecraft* minecraft, OptionsGroup& group, std::string label, const Options::Option* option, float progressMin=0.0f, float progressMax=0.0f );
	void createStepSlider(Minecraft* minecraft, OptionsGroup& group, std::string label, const Options::Option* option, const std::vector<int>& stepVec );
	void setupPositions();
	virtual void render(Minecraft* minecraft, int xm, int ym);

	// The Graphics pane is taller than the screen on a 1366x768 Surface RT,
	// so the rows below the fold have to be reachable.
	void setViewHeight(int h) { viewHeight = h; clampScroll(); }
	int  getMaxScroll() const { return contentHeight > viewHeight ? contentHeight - viewHeight : 0; }
	int  getScrollOffset() const { return scrollOffset; }
	void scrollBy(int dy);
	void clampScroll();
	// Is the point on a control (so a drag must not scroll the pane)?
	bool hitTest(int px, int py);
	bool isPointInView(int px, int py) const;

	int scrollOffset;
	int viewHeight;
	int contentHeight;
};

#endif /*ITEMPANE_H__*/
