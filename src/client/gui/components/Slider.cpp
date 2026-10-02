#include "Slider.h"
#include "../../Minecraft.h"
#include "../../renderer/Textures.h"
#include "../Screen.h"
#include "../../../util/Mth.h"
#include <algorithm>
#include <assert.h>
#include "../../../util/Mth.h"
Slider::Slider(Minecraft* minecraft, const Options::Option* option,  float progressMin, float progressMax)
: sliderType(SliderProgress), mouseDownOnElement(false), option(option), numSteps(0), progressMin(progressMin), progressMax(progressMax) {
	if(option != NULL) {
		percentage = (minecraft->options.getProgressValue(option) - progressMin) / (progressMax - progressMin);
	}
}

Slider::Slider(Minecraft* minecraft, const Options::Option* option, const std::vector<int>& stepVec )
: sliderType(SliderStep),
  curStepValue(0),
  curStep(0),
  sliderSteps(stepVec),
  mouseDownOnElement(false),
  option(option),
  percentage(0),
  progressMin(0.0f),
  progressMax(1.0) {
	assert(stepVec.size() > 1);
	numSteps = sliderSteps.size();
	if(option != NULL) {
		curStepValue = minecraft->options.getIntValue(option);
		std::vector<int>::iterator currentItem = std::find(sliderSteps.begin(), sliderSteps.end(), curStepValue);
		if(currentItem != sliderSteps.end()) {
			// Was a local "int curStep;" shadowing the member, so the handle
			// always stayed at the far left and the slider looked broken.
			curStep = currentItem - sliderSteps.begin();
		} else {
			// Value is not one of the steps (or the option has no value yet):
			// pick the closest one instead of leaving it at 0.
			float bestDist = -1;
			for (unsigned int a = 0; a < sliderSteps.size(); ++a) {
				float dist = (float)Mth::abs(sliderSteps[a] - curStepValue);
				if (bestDist < 0 || dist < bestDist) {
					bestDist = dist;
					curStep = (int)a;
				}
			}
			curStepValue = sliderSteps[curStep];
		}
		percentage = numSteps > 1 ? float(curStep) / float(numSteps - 1) : 0.0f;
	}
}

void Slider::render( Minecraft* minecraft, int xm, int ym ) {
	int xSliderStart = x + 5;
	int xSliderEnd = x + width - 5;
	int ySliderStart = y + 6;
	int ySliderEnd = y + 9;
	int handleSizeX = 9;
	int handleSizeY = 15;
	int barWidth = xSliderEnd - xSliderStart;
	//fill(x, y + 8, x + (int)(width * percentage), y + height, 0xffff00ff);
	fill(xSliderStart, ySliderStart, xSliderEnd, ySliderEnd, 0xff606060);
	if(sliderType == SliderStep) {
		int stepDistance = barWidth / (numSteps -1);
		for(int a = 0; a <= numSteps - 1; ++a) {
			int renderSliderStepPosX = xSliderStart + a * stepDistance + 1;
			fill(renderSliderStepPosX - 1, ySliderStart - 2, renderSliderStepPosX + 1, ySliderEnd + 2, 0xff606060);
		}
	}
	minecraft->textures->loadAndBindTexture("gui/touchgui.png");
	blit(xSliderStart + (int)(percentage * barWidth) - handleSizeX / 2, y, 226, 126, handleSizeX, handleSizeY, handleSizeX, handleSizeY);
}

void Slider::mouseClicked( Minecraft* minecraft, int x, int y, int buttonNum ) {
	if(pointInside(x, y)) {
		mouseDownOnElement = true;
		if (sliderType == SliderStep) {
			// Jump straight to the clicked step, otherwise the first tap
			// did nothing at all.
			int xSliderStart = x + 5;
			int barWidth = (x + width - 5) - xSliderStart;
			float p = barWidth > 0 ? float(x - xSliderStart) / float(barWidth) : 0.0f;
			percentage = Mth::clamp(p, 0.0f, 1.0f);
			curStep = Mth::floor(percentage * (numSteps - 1) + 0.5f);
			if (curStep < 0) curStep = 0;
			if (curStep > numSteps - 1) curStep = numSteps - 1;
			curStepValue = sliderSteps[curStep];
			setOption(minecraft);
		}
	}
}

void Slider::mouseReleased( Minecraft* minecraft, int x, int y, int buttonNum ) {
	mouseDownOnElement = false;
	if(sliderType == SliderStep) {
		curStep = Mth::floor((percentage * (numSteps-1) + 0.5f));
		curStepValue = sliderSteps[Mth::Min(curStep, numSteps-1)];
		percentage = float(curStep) / (numSteps - 1);
		setOption(minecraft);
	}
}

void Slider::tick(Minecraft* minecraft) {
	if(minecraft->screen != NULL) {
		int xm = Mouse::getX();
		int ym = Mouse::getY();
		minecraft->screen->toGUICoordinate(xm, ym);
		if(mouseDownOnElement) {
			int xSliderStart = x + 5;
			int barWidth = (x + width - 5) - xSliderStart;
			float p = barWidth > 0 ? float(xm - xSliderStart) / float(barWidth) : 0.0f;
			percentage = Mth::clamp(p, 0.0f, 1.0f);
			if (sliderType == SliderStep && numSteps > 1) {
				curStep = Mth::floor(percentage * (numSteps - 1) + 0.5f);
				if (curStep < 0) curStep = 0;
				if (curStep > numSteps - 1) curStep = numSteps - 1;
				curStepValue = sliderSteps[curStep];
			}
			setOption(minecraft);
		}
	}
}

void Slider::setOption( Minecraft* minecraft ) {
	if(option != NULL) {
		if(sliderType == SliderStep) {
			if(minecraft->options.getIntValue(option) != curStepValue) {
				minecraft->options.set(option, curStepValue);
			}
		} else {
			if(minecraft->options.getProgressValue(option) != percentage * (progressMax - progressMin) + progressMin) {
				minecraft->options.set(option, percentage *  (progressMax - progressMin) + progressMin);
			}
		}
	}
}
