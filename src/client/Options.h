#ifndef NET_MINECRAFT_CLIENT__Options_H__
#define NET_MINECRAFT_CLIENT__Options_H__

//package net.minecraft.client;

//#include "locale/Language.h"

#include <string>
#include <cstdio>
#include "KeyMapping.h"
#include "../platform/input/Keyboard.h"
#include "../util/StringUtils.h"
#include "OptionsFile.h"

class Minecraft;
typedef std::vector<std::string> StringVector;

class Options
{
public:
    class Option
	{
        const bool _isProgress;
        const bool _isBoolean;
        const std::string _captionId;
		const int _ordinal;

	public:
		static const Option MUSIC;
		static const Option SOUND;
		static const Option INVERT_MOUSE;
		static const Option SENSITIVITY;
		static const Option RENDER_DISTANCE;
		static const Option VIEW_BOBBING;
		static const Option ANAGLYPH;
		static const Option LIMIT_FRAMERATE;
		static const Option DIFFICULTY;
		static const Option GRAPHICS;
	static const Option FOG;
		static const Option AMBIENT_OCCLUSION;
		static const Option GUI_SCALE;
        
		static const Option THIRD_PERSON;
		static const Option HIDE_GUI;
		static const Option SERVER_VISIBLE;
		static const Option LEFT_HANDED;
		static const Option USE_TOUCHSCREEN;
		static const Option USE_TOUCH_JOYPAD;
		static const Option DESTROY_VIBRATION;

		static const Option PIXELS_PER_MILLIMETER;

		// On-screen D-pad: can be turned off completely and its size can be
		// changed in steps, because 14mm was unusable on the Surface RT
		// while 42mm covered too much of the screen.
		static const Option TOUCH_BUTTONS;
		static const Option TOUCH_BUTTON_SIZE;
		static const Option TOUCH_SENSITIVITY;
		static const Option SHOW_FPS;

		/*
        static Option* getItem(int id) {
            for (Option item : Option.values()) {
                if (item.getId() == id) {
                    return item;
                }
            }
            return NULL;
        }
		*/

        Option(int ordinal, const std::string& captionId, bool hasProgress, bool isBoolean)
		:	_captionId(captionId),
			_isProgress(hasProgress),
			_isBoolean(isBoolean),
			_ordinal(ordinal)
		{}

        bool isProgress() const {
            return _isProgress;
        }

        bool isBoolean() const {
            return _isBoolean;
        }

		bool isInt() const {
			return (!_isBoolean && !_isProgress);
		}

        int getId() {
            return _ordinal;
        }

        std::string getCaptionId() const {
            return _captionId;
        }
    };

private:
	static const float SOUND_MIN_VALUE;
	static const float SOUND_MAX_VALUE;
	static const float MUSIC_MIN_VALUE;
	static const float MUSIC_MAX_VALUE;
	static const float SENSITIVITY_MIN_VALUE;
	static const float SENSITIVITY_MAX_VALUE;
	static const float TOUCH_SENSITIVITY_MIN_VALUE;
	static const float TOUCH_SENSITIVITY_MAX_VALUE;
	static const float PIXELS_PER_MILLIMETER_MIN_VALUE;
	static const float PIXELS_PER_MILLIMETER_MAX_VALUE;
	// Render distance is counted in chunks (2..16) instead of the old
	// 0..3 index that the renderer turned into 256 >> index blocks.
public:
	static const int RENDER_DISTANCE_MIN;
	static const int RENDER_DISTANCE_MAX;
	static const int RENDER_DISTANCE_DEFAULT;
	// Touch button size steps, and the scale each step means relative to the
	// big (42mm) buttons. Step 0 is half of that, i.e. the old 14mm default.
	static const int TOUCH_BUTTON_SIZE_STEPS;
	static const float TOUCH_BUTTON_SIZE_SCALE[];
	static float getTouchButtonSizeScale(int step);
	// Graphics quality ladder, cycled with L (down) and K (up). Rung 1 and 2
	// leave the render distance alone, rung 3 switches the fog off for good,
	// rung 4 drops the smooth lighting, and rungs 5..8 each take off two
	// chunks of render distance. It never goes below four chunks, because at
	// two the held item vanishes and the sky starts flickering.
	static const int QUALITY_LEVEL_MIN;
	static const int QUALITY_LEVEL_MAX;
	static const int QUALITY_LEVEL_DEFAULT;
	static const int QUALITY_MIN_DISTANCE;
	int graphicsLevel;
	int graphicsBaseDistance;
private:
    static const char* RENDER_DISTANCE_NAMES[];
    static const char* DIFFICULTY_NAMES[];
    static const char* GUI_SCALE[];
	static const int DIFFICULY_LEVELS[];
public:
	static bool debugGl;

	float music;
    float sound;
    //note: sensitivity is transformed in Options::update
    float sensitivity;
    // Multiplier on top of the mouse sensitivity for touch look only, so the
    // touch camera can be tuned without changing the mouse.
    float touchSensitivity;
    bool invertYMouse;
    int viewDistance; // in chunks (2..16), converted to blocks by the renderer
    bool bobView;
    bool anaglyph3d;
    bool limitFramerate;
    bool fancyGraphics;
    bool fog;
    bool ambientOcclusion;
    bool showFps;
	bool useMouseForDigging;
	bool isLeftHanded;
	bool destroyVibration;
    //std::string skin;

    KeyMapping keyUp;
    KeyMapping keyLeft;
    KeyMapping keyDown;
    KeyMapping keyRight;
    KeyMapping keyJump;
    KeyMapping keyBuild;
    KeyMapping keyDrop;
    KeyMapping keyChat;
    KeyMapping keyFog;
    KeyMapping keySneak;
	KeyMapping keyCraft;
	KeyMapping keyDestroy;
	KeyMapping keyUse;

	KeyMapping keyMenuNext;
	KeyMapping keyMenuPrevious;
	KeyMapping keyMenuOk;
	KeyMapping keyMenuCancel;

    KeyMapping* keyMappings[16];

	/*protected*/ Minecraft* minecraft;
    ///*private*/ File optionsFile;

    int difficulty;
    bool hideGui;
    bool thirdPersonView;
    bool renderDebug;

    bool isFlying;
    // F while flying: four times the climb rate. Not saved, it is a
    // momentary modifier that is switched off when the flight ends.
    bool fastFlight;
    bool smoothCamera;
    bool fixedCamera;
    float flySpeed;
    float cameraSpeed;
    int guiScale;
	std::string username;

	bool serverVisible;
	bool isJoyTouchArea;
	bool useTouchScreen;
	bool touchButtons;
	int touchButtonSize;
	float pixelsPerMillimeter;
    Options(Minecraft* minecraft, const std::string& workingDirectory)
	:	minecraft(minecraft)
	{
        //optionsFile = /*new*/ File(workingDirectory, "options.txt");
        initDefaultValues();

		load();
    }

	Options()
	:	minecraft(NULL)
	{
		
	}

	void initDefaultValues();

    std::string getKeyDescription(int i) {
        //Language language = Language.getInstance();
        //return language.getElement(keyMappings[i].name);
		return "Options::getKeyDescription not implemented";
    }

    std::string getKeyMessage(int i) {
        //return Keyboard.getKeyName(keyMappings[i].key);
		return "Options::getKeyMessage not implemented";
    }

    void setKey(int i, int key) {
        keyMappings[i]->key = key;
        save();
    }

    void set(const Option* item, float value) {
        if (item == &Option::MUSIC) {
            music = value;
            //minecraft.soundEngine.updateOptions();
        } else if (item == &Option::SOUND) {
            sound = value;
            //minecraft.soundEngine.updateOptions();
        } else if (item == &Option::SENSITIVITY) {
            sensitivity = value;
        } else if (item == &Option::PIXELS_PER_MILLIMETER) {
			 pixelsPerMillimeter = value;
		} else if (item == &Option::TOUCH_SENSITIVITY) {
			// This one was missing, so the slider threw the value away and the
			// caption stayed at 100% forever.
			touchSensitivity = value < TOUCH_SENSITIVITY_MIN_VALUE ? TOUCH_SENSITIVITY_MIN_VALUE
				: (value > TOUCH_SENSITIVITY_MAX_VALUE ? TOUCH_SENSITIVITY_MAX_VALUE : value);
		}
		notifyOptionUpdate(item, value);
		save();
    }
	void set(const Option* item, int value) {
		// Only DIFFICULTY was handled here, so every other step slider
		// (render distance, gui scale) silently threw its value away.
		if(item == &Option::DIFFICULTY) difficulty = value;
		if(item == &Option::RENDER_DISTANCE) {
			viewDistance = value < RENDER_DISTANCE_MIN ? RENDER_DISTANCE_MIN
				: (value > RENDER_DISTANCE_MAX ? RENDER_DISTANCE_MAX : value);
			// The quality ladder counts down from this, so the slider has to
			// move the ladder's base with it, otherwise L has nothing to cut.
			graphicsBaseDistance = viewDistance;
		}
		if(item == &Option::GUI_SCALE) {
			guiScale = value < 1 ? 1 : (value > 4 ? 4 : value);
		}
		if(item == &Option::TOUCH_BUTTON_SIZE) {
			touchButtonSize = value < 0 ? 0
				: (value >= TOUCH_BUTTON_SIZE_STEPS ? TOUCH_BUTTON_SIZE_STEPS - 1 : value);
		}
		notifyOptionUpdate(item, value);
		save();
	}

    void toggle(const Option* option, int dir) {
        if (option == &Option::INVERT_MOUSE)	invertYMouse = !invertYMouse;
        if (option == &Option::RENDER_DISTANCE) {
			int v = viewDistance + dir;
			viewDistance = v < RENDER_DISTANCE_MIN ? RENDER_DISTANCE_MIN
				: (v > RENDER_DISTANCE_MAX ? RENDER_DISTANCE_MAX : v);
		}
        if (option == &Option::FOG)				fog = !fog;
        if (option == &Option::GUI_SCALE) {
			int v = guiScale + dir;
			guiScale = v < 1 ? 1 : (v > 4 ? 4 : v);
		}
        if (option == &Option::VIEW_BOBBING)	bobView = !bobView;
		if (option == &Option::THIRD_PERSON)	thirdPersonView = !thirdPersonView;
		if (option == &Option::HIDE_GUI)		hideGui = !hideGui;
		if (option == &Option::SERVER_VISIBLE)		serverVisible = !serverVisible;
		if (option == &Option::LEFT_HANDED) isLeftHanded = !isLeftHanded;
		if (option == &Option::USE_TOUCHSCREEN) useTouchScreen = !useTouchScreen;
		if (option == &Option::USE_TOUCH_JOYPAD) isJoyTouchArea = !isJoyTouchArea;
		if (option == &Option::TOUCH_BUTTONS) touchButtons = !touchButtons;
		if (option == &Option::SHOW_FPS) showFps = !showFps;
		if (option == &Option::DESTROY_VIBRATION) destroyVibration = !destroyVibration;
		if (option == &Option::ANAGLYPH) {
            anaglyph3d = !anaglyph3d;
            //minecraft->textures.reloadAll();
        }
        if (option == &Option::LIMIT_FRAMERATE) limitFramerate = !limitFramerate;
        if (option == &Option::DIFFICULTY) difficulty = (difficulty + dir) & 3;
        if (option == &Option::GRAPHICS) {
            fancyGraphics = !fancyGraphics;
            //minecraft->levelRenderer.allChanged();
        }
        if (option == &Option::AMBIENT_OCCLUSION) {
            ambientOcclusion = !ambientOcclusion;
            //minecraft->levelRenderer.allChanged();
        }
		notifyOptionUpdate(option, getBooleanValue(option));
        save();
    }

	int getIntValue(const Option* item) {
		if(item == &Option::DIFFICULTY) return difficulty;
		if(item == &Option::RENDER_DISTANCE) return viewDistance;
		if(item == &Option::GUI_SCALE) return guiScale < 1 ? 1 : guiScale;
		if(item == &Option::TOUCH_BUTTON_SIZE) return touchButtonSize;
		return 0;
	}

    float getProgressValue(const Option* item) {
        if (item == &Option::MUSIC) return music;
        if (item == &Option::SOUND) return sound;
        if (item == &Option::SENSITIVITY) return sensitivity;
		if (item == &Option::TOUCH_SENSITIVITY) return touchSensitivity;
		if (item == &Option::PIXELS_PER_MILLIMETER) return pixelsPerMillimeter;
        return 0;
    }

    bool getBooleanValue(const Option* item) {
        if (item == &Option::INVERT_MOUSE)
            return invertYMouse;
        if (item == &Option::VIEW_BOBBING)
            return bobView;
        if (item == &Option::ANAGLYPH)
            return anaglyph3d;
        if (item == &Option::LIMIT_FRAMERATE)
            return limitFramerate;
        if (item == &Option::AMBIENT_OCCLUSION)
            return ambientOcclusion;
        if (item == &Option::THIRD_PERSON)
            return thirdPersonView;
        if (item == &Option::HIDE_GUI)
            return hideGui;
		if (item == &Option::THIRD_PERSON)
			return thirdPersonView;
		if (item == &Option::SERVER_VISIBLE)
			return serverVisible;
		if (item == &Option::LEFT_HANDED)
			return isLeftHanded;
		if (item == &Option::USE_TOUCHSCREEN)
			return useTouchScreen;
		if (item == &Option::USE_TOUCH_JOYPAD)
			return isJoyTouchArea;
		if (item == &Option::TOUCH_BUTTONS)
			return touchButtons;
		if (item == &Option::DESTROY_VIBRATION)
			return destroyVibration;
		if (item == &Option::GRAPHICS)
			return fancyGraphics;
		if (item == &Option::FOG)
			return fog;
		if (item == &Option::SHOW_FPS)
			return showFps;
		return false;
	}

	float getProgrssMin(const Option* item) {
		if (item == &Option::MUSIC) return MUSIC_MIN_VALUE;
		if (item == &Option::SOUND) return SOUND_MIN_VALUE;
		if (item == &Option::SENSITIVITY) return SENSITIVITY_MIN_VALUE;
		if (item == &Option::TOUCH_SENSITIVITY) return TOUCH_SENSITIVITY_MIN_VALUE;
		if (item == &Option::PIXELS_PER_MILLIMETER) return PIXELS_PER_MILLIMETER_MIN_VALUE;
		return 0;
	}

	float getProgrssMax(const Option* item) {
		if (item == &Option::MUSIC) return MUSIC_MAX_VALUE;
		if (item == &Option::SOUND) return SOUND_MAX_VALUE;
		if (item == &Option::SENSITIVITY) return SENSITIVITY_MAX_VALUE;
		if (item == &Option::TOUCH_SENSITIVITY) return TOUCH_SENSITIVITY_MAX_VALUE;
		if (item == &Option::PIXELS_PER_MILLIMETER) return PIXELS_PER_MILLIMETER_MAX_VALUE;
		return 1.0f;
	} 

	std::string getMessage(const Option* item);
	// Human readable current value, shown next to a control. Sliders and
	// toggles used to give no clue about what they were set to.
	std::string getValueText(const Option* item);

	void update();
    void load();
    void save();
	void addOptionToSaveOutput(StringVector& stringVector, std::string name, bool boolValue);
	void addOptionToSaveOutput(StringVector& stringVector, std::string name, float floatValue);
	void addOptionToSaveOutput(StringVector& stringVector, std::string name, int intValue);
	void notifyOptionUpdate(const Option* option, bool value);
	void notifyOptionUpdate(const Option* option, float value);
	void notifyOptionUpdate(const Option* option, int value);
private:
    static bool readFloat(const std::string& string, float& value);
    static bool readInt(const std::string& string, int& value);
	static bool readBool(const std::string& string, bool& value);

private:
	OptionsFile optionsFile;
	
};

#endif /*NET_MINECRAFT_CLIENT__Options_H__*/
