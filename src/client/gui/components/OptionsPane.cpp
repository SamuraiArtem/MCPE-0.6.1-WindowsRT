#include "OptionsPane.h"
#include "OptionsGroup.h"
#include "OptionsItem.h"
#include "ImageButton.h"
#include "Slider.h"
#include "../../Minecraft.h"
OptionsPane::OptionsPane()
: scrollOffset(0),
  viewHeight(0),
  contentHeight(0) {

}

void OptionsPane::setupPositions() {
	// Layout from the top of the (possibly scrolled) view, then remember how
	// tall the content really is so getMaxScroll() can clamp the offset.
	//
	// Each child is laid out (setupPositions) before its height is read. The
	// old code asked for the height first and laid out afterwards, so the
	// first pass always used stale heights and the rows drifted into each
	// other as soon as the pane was scrolled.
	int currentHeight = y + 1 - scrollOffset;
	for(std::vector<GuiElement*>::iterator it = children.begin(); it != children.end(); ++it ) {
		(*it)->width = width;
		(*it)->x = x;
		(*it)->y = currentHeight;
		(*it)->setupPositions();
		currentHeight += (*it)->height + 6;
	}
	contentHeight = currentHeight - (y + 1 - scrollOffset);
	height = contentHeight;
	super::setupPositions();
}

void OptionsPane::render( Minecraft* minecraft, int xm, int ym ) {
	// Clip to the visible strip. Without this the rows that are scrolled past
	// the top of the pane keep drawing, so their text ran out over the grey
	// header bar above the list.
	bool clipped = viewHeight > 0 && contentHeight > viewHeight;
	if(clipped && minecraft != NULL) {
		glEnable2(GL_SCISSOR_TEST);
		minecraft->gui.setScissorRect(IntRectangle(x, y, width, viewHeight));
	}

	super::render(minecraft, xm, ym);

	// Thin scrollbar, otherwise there is no hint that more settings are below
	int maxScroll = getMaxScroll();
	if(maxScroll > 0 && viewHeight > 0) {
		int barX = x + width - 2;
		int bottom = y + viewHeight;
		fill(barX, y, barX + 2, bottom, 0x30000000);
		int thumbHeight = viewHeight * viewHeight / (contentHeight > 0 ? contentHeight : 1);
		if(thumbHeight < 8) thumbHeight = 8;
		if(thumbHeight > viewHeight) thumbHeight = viewHeight;
		int thumbY = y + (viewHeight - thumbHeight) * scrollOffset / maxScroll;
		fill(barX, thumbY, barX + 2, thumbY + thumbHeight, 0xffa0a0a0);
	}

	if(clipped)
		glDisable2(GL_SCISSOR_TEST);
}

void OptionsPane::scrollBy(int dy) {
	int wanted = scrollOffset + dy;
	if(wanted < 0) wanted = 0;
	if(wanted > getMaxScroll()) wanted = getMaxScroll();
	if(wanted == scrollOffset)
		return;
	scrollOffset = wanted;
	setupPositions();
}

void OptionsPane::clampScroll() {
	int maxScroll = getMaxScroll();
	int clamped = scrollOffset;
	if(clamped < 0) clamped = 0;
	if(clamped > maxScroll) clamped = maxScroll;
	if(clamped != scrollOffset) {
		scrollOffset = clamped;
		setupPositions();
	}
}

bool OptionsPane::hitTest(int px, int py) {
	for(std::vector<GuiElement*>::iterator it = children.begin(); it != children.end(); ++it) {
		if((*it)->contains(px, py))
			return true;
	}
	return false;
}

bool OptionsPane::isPointInView(int px, int py) const {
	return px >= x && px < x + width && py >= y && py < y + viewHeight;
}

OptionsGroup& OptionsPane::createOptionsGroup( std::string label ) {
	OptionsGroup* newGroup = new OptionsGroup(label);
	children.push_back(newGroup);
	// create and return a new group index
	return *newGroup;
}

void OptionsPane::createToggle( Minecraft* minecraft, OptionsGroup& group, std::string label, const Options::Option* option ) {
	ImageDef def;
	def.setSrc(IntRectangle(160, 206, 39, 20));
	def.name = "gui/touchgui.png";
	def.width = 39 * 0.7f;
	def.height = 20 * 0.7f;
	OptionButton* element = new OptionButton(option);
	element->setImageDef(def, true);
	// Show the current state right away instead of an uninitialised image
	if (minecraft != NULL)
		element->updateImage(&minecraft->options);
	OptionsItem* item = new OptionsItem(label, element, option);
	group.addChild(item);
	setupPositions();
}

void OptionsPane::createProgressSlider( Minecraft* minecraft, OptionsGroup& group, std::string label, const Options::Option* option, float progressMin/*=0.0f*/, float progressMax/*=0.0f */ ) {
	if(progressMax <= progressMin && minecraft != NULL && option != NULL) {
		progressMin = minecraft->options.getProgrssMin(option);
		progressMax = minecraft->options.getProgrssMax(option);
	}
	Slider* element = new Slider(minecraft, option, progressMin, progressMax);
	element->width = 100;
	element->height = 20;
	OptionsItem* item = new OptionsItem(label, element, option);
	group.addChild(item);
	setupPositions();
}

void OptionsPane::createStepSlider( Minecraft* minecraft, OptionsGroup& group, std::string label, const Options::Option* option, const std::vector<int>& stepVec ) {
	if(stepVec.size() < 2) return;
	Slider* element = new Slider(minecraft, option, stepVec);
	element->width = 100;
	element->height = 20;
	OptionsItem* item = new OptionsItem(label, element, option);
	group.addChild(item);
	setupPositions();
}