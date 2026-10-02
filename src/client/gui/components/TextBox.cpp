#include "TextBox.h"
#include "../../Minecraft.h"
#include "../../../AppPlatform.h"
TextBox::TextBox( int id, const std::string& msg )
 : id(0), w(0), h(0), x(0), y(0), text(msg), focused(false) {

}

TextBox::TextBox( int id, int x, int y, const std::string& msg ) 
 : id(id), w(0), h(0), x(x), y(y), text(msg), focused(false) {

}

TextBox::TextBox( int id, int x, int y, int w, int h, const std::string& msg )
 : id(id), w(w), h(h), x(x), y(y), text(msg) {

}

void TextBox::setFocus(Minecraft* minecraft) {
	if(!focused) {
		minecraft->platform()->showKeyboard();
		focused = true;
	}
}

bool TextBox::loseFocus(Minecraft* minecraft) {
	if(focused) {
		minecraft->platform()->hideKeyboard();
		focused = false;
		return true;
	}
	return false;
}

void TextBox::render( Minecraft* minecraft, int xm, int ym ) {
	// This used to be empty, so the field was invisible even when focused.
	if(w <= 0 || h <= 0)
		return;
	int border = focused ? 0xffe0e0e0 : 0xff707070;
	fill(x, y, x + w, y + h, 0xff101010);
	fill(x, y, x + w, y + 1, border);
	fill(x, y + h - 1, x + w, y + h, border);
	fill(x, y, x + 1, y + h, border);
	fill(x + w - 1, y, x + w, y + h, border);
	int textY = y + (h - 8) / 2;
	if(!text.empty())
		minecraft->font->drawShadow(text, (float)(x + 4), (float)textY, 0xe0e0e0);
	else
		minecraft->font->drawShadow("...", (float)(x + 4), (float)textY, 0x606060);
	if(focused) {
		int caretX = x + 4 + (text.empty() ? 0 : minecraft->font->width(text));
		fill(caretX, y + 3, caretX + 1, y + h - 3, 0xffe0e0e0);
	}
}
