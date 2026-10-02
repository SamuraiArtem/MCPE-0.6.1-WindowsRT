#include "OptionsItem.h"
#include "../../Minecraft.h"
#include "../../../util/Mth.h"
OptionsItem::OptionsItem( std::string label, GuiElement* element, const Options::Option* option )
: GuiElementContainer(false, true, 0, 0, 24, 12),
  label(label),
  _option(option) {
	  addChild(element);
}

void OptionsItem::setupPositions() {
	int currentHeight = 0;
	for(std::vector<GuiElement*>::iterator it = children.begin(); it != children.end(); ++it) {
		(*it)->x = x + width - (*it)->width - 15;
		(*it)->y = y + currentHeight;
		(*it)->setupPositions();
		currentHeight += (*it)->height;
	}
	// The label is drawn centred in this box, so a row with no control in it
	// (or a very short one) still needs a full text line of height.
	if(currentHeight < 10)
		currentHeight = 10;
	height = currentHeight;
	super::setupPositions();
}

void OptionsItem::render( Minecraft* minecraft, int xm, int ym ) {
	int yOffset = (height - 8) / 2;
	int valueX = x + width;
	// Sliders and toggles used to show no value at all, so there was no way
	// to tell what they were set to. Print it just left of the control.
	if(!children.empty() && _option != NULL) {
		std::string value = minecraft->options.getValueText(_option);
		if(!value.empty()) {
			int valueWidth = minecraft->font->width(value);
			valueX = children[0]->x - valueWidth - 4;
			minecraft->font->draw(value, (float)valueX, (float)y + yOffset, 0x707070, false);
		}
	}
	// ... and shorten the label instead of letting the two overlap.
	std::string shown = label;
	int maxLabelWidth = valueX - x - 4;
	if(maxLabelWidth > 8 && minecraft->font->width(shown) > maxLabelWidth) {
		while(!shown.empty() && minecraft->font->width(shown + "...") > maxLabelWidth)
			shown.erase(shown.size() - 1);
		shown += "...";
	}
	minecraft->font->draw(shown, (float)x, (float)y + yOffset, 0x909090, false);
	super::render(minecraft, xm, ym);
}