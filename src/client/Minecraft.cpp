#include "Minecraft.h"

#if defined(APPLE_DEMO_PROMOTION)
    #define NO_NETWORK
#endif

#if defined(RPI)
	#define CREATORMODE
#endif

#include "../network/RakNetInstance.h"
#include "../network/ClientSideNetworkHandler.h"
#include "../network/ServerSideNetworkHandler.h"
//#include "../network/Packet.h"
#include "../world/entity/player/Inventory.h"
#include "../world/level/chunk/ChunkCache.h"
#include "../world/level/tile/Tile.h"
#include "../world/level/storage/LevelStorageSource.h"
#include "../world/level/storage/LevelStorage.h"
#include "player/input/KeyboardInput.h"
#ifndef STANDALONE_SERVER
#include "player/input/touchscreen/TouchInputHolder.h"
#endif
#include "player/LocalPlayer.h"
#include "gamemode/CreativeMode.h"
#include "gamemode/SurvivalMode.h"
#include "player/LocalPlayer.h"
#ifndef STANDALONE_SERVER
#include "particle/ParticleEngine.h"
#include "gui/Screen.h"
#include "gui/Font.h"
#include "gui/screens/RenameMPLevelScreen.h"
#include "sound/SoundEngine.h"
#endif
#include "../platform/CThread.h"
#include "../platform/input/Mouse.h"
#include "../AppPlatform.h"
#include "../Performance.h"
#include "../LicenseCodes.h"
#include "../util/PerfTimer.h"
#include "../util/PerfRenderer.h"
#include "player/input/MouseBuildInput.h"

#include "../world/Facing.h"

#include "../network/packet/PlaceBlockPacket.h"

#include "player/input/IInputHolder.h"
#ifndef STANDALONE_SERVER
#include "player/input/touchscreen/TouchscreenInput.h"

#include "player/input/ControllerTurnInput.h"
#include "player/input/XperiaPlayInput.h"

#endif

#include "player/input/MouseTurnInput.h"
#include "../world/entity/MobFactory.h"
#include "../world/level/MobSpawner.h"
#include "../util/Mth.h"
#include "../network/packet/InteractPacket.h"
#ifndef STANDALONE_SERVER
#include "gui/screens/PrerenderTilesScreen.h"
#include "renderer/Textures.h"
#include "gui/screens/DeathScreen.h"
#endif

#include "../network/packet/RespawnPacket.h"
#include "IConfigListener.h"
#include "../world/entity/MobCategory.h"
#ifndef STANDALONE_SERVER
#include "gui/screens/FurnaceScreen.h"
#endif
#include "../world/Difficulty.h"
#include "../server/ServerLevel.h"
#ifdef CREATORMODE
#include "../server/CreatorLevel.h"
#endif
#include "../network/packet/AdventureSettingsPacket.h"
#include "../network/packet/SetSpawnPositionPacket.h"
#include "../network/command/CommandServer.h"
#include "gamemode/CreatorMode.h"
#ifndef STANDALONE_SERVER
#include "gui/screens/ArmorScreen.h"
#endif
#include "../world/level/levelgen/synth/ImprovedNoise.h"
#ifndef STANDALONE_SERVER
#include "renderer/tileentity/TileEntityRenderDispatcher.h"
#endif

#ifndef STANDALONE_SERVER
#include "renderer/ptexture/DynamicTexture.h"
#include "renderer/GameRenderer.h"
#include "renderer/ItemInHandRenderer.h"
#include "renderer/LevelRenderer.h"
#include "renderer/entity/EntityRenderDispatcher.h"
#include "gui/Screen.h"
#include "gui/Font.h"
#include "gui/screens/RenameMPLevelScreen.h"
#include "sound/SoundEngine.h"
#endif

static void checkGlError(const char* tag) {
#ifdef GLDEBUG
	while (1) {
        const int errCode = glGetError();
        if (errCode == GL_NO_ERROR) break;

		LOGE("################\nOpenGL-error @ %s : #%d\n", tag, errCode);
	}
#endif /*GLDEBUG*/
}

/*static*/
const char* Minecraft::progressMessages[] = {
	"Locating server",
	"Building terrain",
	"Preparing",
	"Saving chunks"
};

int Minecraft::customDebugId = Minecraft::CDI_NONE;

#if defined(_MSC_VER)
	#pragma warning( disable : 4355 ) // 'this' pointer in initialization list which is perfectly legal
#endif

bool Minecraft::useAmbientOcclusion = false;

Minecraft::Minecraft()
:	user(NULL),
	level(NULL),
	player(NULL),
	cameraTargetPlayer(NULL),
	levelRenderer(NULL),
	gameRenderer(NULL),
#ifndef STANDALONE_SERVER
	particleEngine(NULL),
	_perfRenderer(NULL),
#endif
	_commandServer(NULL),
#ifndef STANDALONE_SERVER
	textures(NULL),
#endif
	lastTickTime(-1),
	lastTime(0),
	ticksSinceLastUpdate(0),
	gameMode(NULL),
	mouseGrabbed(false),
	missTime(0),
	pause(false),
	_running(false),
	timer(20),
#ifndef STANDALONE_SERVER
	gui(this),
#endif
	netCallback(NULL),
#ifndef STANDALONE_SERVER
	screen(NULL),
	font(NULL),
#endif
	screenMutex(false),
#ifndef STANDALONE_SERVER
	scheduledScreen(NULL),
	hasScheduledScreen(false),
	soundEngine(NULL),
#endif
	ticks(0),
	isGeneratingLevel(false),
	_hasSignaledGeneratingLevelFinished(true),
	generateLevelThread(NULL),
	progressStagePercentage(0),
	progressStageStatusId(0),
	isLookingForMultiplayer(false),
	_licenseId(LicenseCodes::WAIT_PLATFORM_NOT_READY),
	inputHolder(0),
	_supportsNonTouchscreen(false),
#ifndef STANDALONE_SERVER
	screenChooser(this),
#endif
	width(1), height(1),
	//_respawnPlayerTicks(-1),
#ifdef __APPLE__
    _isSuperFast(false),
#endif
	_powerVr(false),
	commandPort(4711),
	reserved_d1(0),reserved_d2(0),
	reserved_f1(0),reserved_f2(0)
{
//#ifdef ANDROID

#if defined(NO_NETWORK)
    raknetInstance = new IRakNetInstance();
#else
	raknetInstance = new RakNetInstance();
#endif
#ifndef STANDALONE_SERVER
	soundEngine = new SoundEngine(20.0f);
	soundEngine->init(this, &options);
#endif
	//setupPieces();

	_fps = 0;
	_frameCounter = 0;
	_lastFpsTime = 0;
}

Minecraft::~Minecraft()
{
	delete netCallback;
	delete raknetInstance;
#ifndef STANDALONE_SERVER
	delete levelRenderer;
	delete gameRenderer;
	delete particleEngine;

	delete soundEngine;
#endif
	delete gameMode;
#ifndef STANDALONE_SERVER
	delete font;
	delete textures;

	if (screen != NULL) {
		delete screen;
		screen = NULL;
	}
#endif
	if (level != NULL) {
		level->saveGame();
		if (level->getChunkSource())
			level->getChunkSource()->saveAll(true);
		delete level->getLevelStorage();
		delete level;
		level = NULL;
	}

	//delete player;
	delete user;
	delete inputHolder;

	delete storageSource;
	delete _perfRenderer;
	delete _commandServer;

	MobFactory::clearStaticTestMobs();
#ifndef STANDALONE_SERVER
	EntityRenderDispatcher::destroy();
#endif
}

// Only called by server
void Minecraft::selectLevel( const std::string& levelId, const std::string& levelName, const LevelSettings& settings )
{
#if defined(CREATORMODE)
	level = new CreatorLevel(
#else
	level = new ServerLevel(
#endif
		storageSource->selectLevel(levelId, false),
		levelName,
		settings,
		SharedConstants::GeneratorVersion);

	// note: settings is useless beyond this point, since it's
	//       either copied to LevelData (or LevelData read from file)
	setLevel(level, "Generating level");
	setIsCreativeMode(level->getLevelData()->getGameType() == GameType::Creative);
	_running = true;
}

void Minecraft::setLevel(Level* level, const std::string& message /* ="" */, LocalPlayer* forceInsertPlayer /* = NULL */) {
	cameraTargetPlayer = NULL;
	LOGI("Seed is %ld\n", level->getSeed());

	if (level != NULL) {
		level->raknetInstance = raknetInstance;
        gameMode->initLevel(level);

		if (!player && forceInsertPlayer)
		{
			player = forceInsertPlayer;
			player->resetPos(false);
			//level->addEntity(forceInsertPlayer);
		}
		else if (player != NULL) {
			player->resetPos(false);
			if (level != NULL) {
				level->addEntity(player);
			}
		}
		this->level = level;
		_hasSignaledGeneratingLevelFinished = false;
#ifdef STANDALONE_SERVER
		const bool threadedLevelCreation = false;
#else
		const bool threadedLevelCreation = true;
#endif

		if (threadedLevelCreation) {
			if (level->isClientSide) {
				// A client must not generate terrain locally: the server
				// streams the chunks in. Generating them in a worker thread
				// while the network handler fills the very same chunks on
				// the main thread was a data race and killed the process
				// without any log.
				LOGI("Client level: skipping local generation\n");
				isGeneratingLevel = false;
			} else {
				// Threaded
				// "Lock"
				isGeneratingLevel = true;
				generateLevelThread = new CThread(Minecraft::prepareLevel_tspawn, this);
			}
		} else {
			// Non-threaded
			generateLevel("Currently not used", level);
		}
    } else {
        player = NULL;
    }

    this->lastTickTime = 0;
	this->_running = true;
}

void Minecraft::leaveGame(bool renameLevel /*=false*/)
{
    if (isGeneratingLevel || !_hasSignaledGeneratingLevelFinished)
        return;
    
	isGeneratingLevel = false;
	bool saveLevel = level && (!level->isClientSide || renameLevel);

	raknetInstance->disconnect();
	if (saveLevel) {
		// If server or wanting to save level as client, save all unsaved chunks!
		level->getChunkSource()->saveAll(true);
	}

	LOGI("Clearing levels\n");

	cameraTargetPlayer = NULL;
#ifndef STANDALONE_SERVER
	levelRenderer->setLevel(NULL);
	particleEngine->setLevel(NULL);
#endif
	LOGI("Erasing callback\n");
	delete netCallback;
	netCallback = NULL;

	LOGI("Erasing level\n");
	if (level != NULL) {
		delete level->getLevelStorage();
		delete level;
		level = NULL;
	}
	//delete player;
	player = NULL;
	cameraTargetPlayer = NULL;

	_running = false;
#ifndef STANDALONE_SERVER
	if (renameLevel) {
		setScreen(new RenameMPLevelScreen(LevelStorageSource::TempLevelId));
	}
	else
		screenChooser.setScreen(SCREEN_STARTMENU);
#endif
}

void Minecraft::prepareLevel(const std::string& title) {
	LOGI("status: 1\n");
	progressStageStatusId = 1;

	Stopwatch A, B, C, D;
	A.start();

	Stopwatch L;

	// Dont update lights if we load the level (ok, actually just with leveldata version=1.+(?))
	if (!level->isNew())
		level->setUpdateLights(false);

	int Max = CHUNK_CACHE_WIDTH * CHUNK_CACHE_WIDTH;
	int pp = 0;
	for (int x = 8; x < (CHUNK_CACHE_WIDTH * CHUNK_WIDTH); x += CHUNK_WIDTH) {
        for (int z = 8; z < (CHUNK_CACHE_WIDTH * CHUNK_WIDTH); z += CHUNK_WIDTH) {
            progressStagePercentage = 100 * pp++ / Max;
            //printf("level generation progress %d\n", progressStagePercentage);
			B.start();
            level->getTile(x, 64, z);
			B.stop();
			L.start();
			if (level->isNew())
				while (level->updateLights())
					;
			L.stop();
        }
    }
	A.stop();
	level->setUpdateLights(true);

	C.start();
	for (int x = 0; x < CHUNK_CACHE_WIDTH; x++)
	{
		for (int z = 0; z < CHUNK_CACHE_WIDTH; z++)
		{
			LevelChunk* chunk = level->getChunk(x, z);
			if (chunk && !chunk->createdFromSave)
			{
				chunk->unsaved = false;
				chunk->clearUpdateMap();
			}
		}
	}
	C.stop();

	LOGI("status: 3\n");
	progressStageStatusId = 3;
	if (level->isNew()) {
		level->setInitialSpawn(); // @note: should obviously be called from Level itself
		level->saveLevelData();
		level->getChunkSource()->saveAll(false);
		level->saveGame();
	} else {
		level->saveLevelData();
		level->loadEntities();
	}

	progressStagePercentage = -1;
	progressStageStatusId = 2;
	LOGI("status: 2\n");

	D.start();
	level->prepare();
	D.stop();

	A.print("Generate level: ");
	L.print(" - light: ");
	B.print(" - getTl: ");
	C.print(" - clear: ");
	D.print(" - prepr: ");
	progressStageStatusId = 0;
}

void Minecraft::update() {
	//LOGI("Enter Update\n");

	// Frame counter for the on-screen FPS display
	++_frameCounter;
	{
		int now = getTimeMs();
		if (_lastFpsTime == 0)
			_lastFpsTime = now;
		else if (now - _lastFpsTime >= 1000) {
			_fps = (int)((_frameCounter * 1000) / (now - _lastFpsTime));
			_frameCounter = 0;
			_lastFpsTime = now;
		}
	}

	// Holding F while flying is the fast flight modifier, so it has to be
	// sampled every frame and not only on the key press.
	options.fastFlight = options.isFlying && Keyboard::isKeyDown(Keyboard::KEY_F);
	if (Options::debugGl)
		LOGI(">>>>>>>>>>\n");

	TIMER_PUSH("root");

	//if (level) {
	//	LOGI("numplayers: %d\n", level->players.size());
	//	for (int i = 0; i < level->players.size(); ++i) {
	//		Player* p = level->players[i];
	//		bool inEnt = std::find(level->entities.begin(), level->entities.end(), p) != level->entities.end();
	//		LOGI("  %p, %d, %d - in? %d\n", p, p->entityId, p->owner.ToUint32(p->owner), inEnt);
	//	}
	//}

	if (pause && level != NULL) {
		float lastA = timer.a;
		timer.advanceTime();
		timer.a = lastA;
	} else {
		timer.advanceTime();
	}

	if (raknetInstance) {
		raknetInstance->runEvents(netCallback);
	}

	TIMER_PUSH("tick");
	int toTick = timer.ticks;
	for (int i = 0; i < toTick; ++i, ++ticks)
		tick(i, toTick-1);

	TIMER_POP_PUSH("updatelights");
	if (level && !isGeneratingLevel) {
		level->updateLights();
	}
	TIMER_POP();

	#ifndef STANDALONE_SERVER
		if (gameMode != NULL) gameMode->render(timer.a);
		TIMER_PUSH("sound");
		soundEngine->update(player, timer.a);
		TIMER_POP_PUSH("render");
		gameRenderer->render(timer.a);
		TIMER_POP();
	#else
	CThread::sleep(1);
	#endif
#ifndef STANDALONE_SERVER
	Multitouch::resetThisUpdate();
#endif
	TIMER_POP();
#ifndef STANDALONE_SERVER
	checkGlError("Update finished");

	if (options.renderDebug) {
		if (!PerfTimer::enabled) {
			PerfTimer::reset();
			PerfTimer::enabled = true;
		}

		//TIMER_PUSH("debugfps");
		_perfRenderer->renderFpsMeter(1);
		checkGlError("render debug");
		//TIMER_POP();
	} else {
		PerfTimer::enabled = false;
	}
#endif
	//LOGI("Exit Update\n");
}

void Minecraft::tick(int nTick, int maxTick) {
	if (missTime > 0) missTime--;
#ifndef STANDALONE_SERVER
	if (!screen && player) {
		if (player->health <= 0) {
			setScreen(new DeathScreen());
		}
	}
#endif
	TIMER_PUSH("gameMode");
	if (level && !pause) {
		gameMode->tick();
	}

	TIMER_POP_PUSH("commandServer");
	if (level && _commandServer) {
		_commandServer->tick();
	}

	TIMER_POP_PUSH("input");
	tickInput();
#ifndef STANDALONE_SERVER
	TIMER_POP_PUSH("gui");
	gui.tick();
#endif
	//
	// Ongoing level generation in a (perhaps) different thread. When it's
	// ready, _levelGenerated() is called once and any threads are deleted.
	//
	if (isGeneratingLevel) {
		return;
	} else if (!_hasSignaledGeneratingLevelFinished) {
		if (generateLevelThread) {
			delete generateLevelThread;
			generateLevelThread = NULL;
		}
		_levelGenerated();
	}

	//
	// Normal game loop, run before or efter level generation
	//
	if (level != NULL)
	{
		if (!pause) {
#ifndef STANDALONE_SERVER
			TIMER_POP_PUSH("gameRenderer");
			gameRenderer->tick(nTick, maxTick);

			TIMER_POP_PUSH("levelRenderer");
			levelRenderer->tick();
#endif
			level->difficulty = options.difficulty;
			if (level->isClientSide) level->difficulty = Difficulty::EASY;

			TIMER_POP_PUSH("level");
			level->tickEntities();
			level->tick();
#ifndef STANDALONE_SERVER
			TIMER_POP_PUSH("animateTick");
			if (player) {
				level->animateTick(Mth::floor(player->x), Mth::floor(player->y), Mth::floor(player->z));
			}
#endif
		}
	}
#ifndef STANDALONE_SERVER
	textures->loadAndBindTexture("terrain.png");
	if (!pause && !(screen && !screen->renderGameBehind())) {
		#if !defined(RPI)
			#ifdef __APPLE__
			if (isSuperFast())
			#endif
			{
			if (nTick == maxTick) {
				TIMER_POP_PUSH("textures");
				textures->tick(true);
			}
			}
		#endif
	}
	TIMER_POP_PUSH("particles");
	particleEngine->tick();
	if (screen) {
		screenMutex = true;
		screen->tick();
		screenMutex = false;
	}

	// @note: fix to keep "isPressed" and "isReleased" as long as necessary.
	//      Most likely keyboard and mouse could/should be reset here as well.
	Multitouch::reset();
#endif
	TIMER_POP();
}

class InputRAII {
public:
	~InputRAII() {
#ifndef STANDALONE_SERVER
		Mouse::reset();
		Keyboard::reset();
#endif
	}
};

void Minecraft::tickInput() {
#ifndef STANDALONE_SERVER
	InputRAII raiiInput;

	if (screen && !screen->passEvents) {
		screenMutex = true;
		screen->updateEvents();
		//screen->updateSetScreen();
		screenMutex = false;
		if (hasScheduledScreen) {
			setScreen(scheduledScreen);
			scheduledScreen = NULL;
			hasScheduledScreen = false;
		}
		return;
	}

	if (!player) {
		return;
	}

#ifdef RPI
	bool mouseDiggable = true;
	bool allowGuiClicks = !mouseGrabbed;
#else
	bool mouseDiggable = !gui.isInside(Mouse::getX(), Mouse::getY());
	bool allowGuiClicks = true;
#endif

	TIMER_PUSH("mouse");
	while (Mouse::next()) {
		//if (Mouse::getButtonState(MouseAction::ACTION_LEFT))
		//	LOGI("mouse-down-at: %d, %d\n", Mouse::getX(), Mouse::getY());
        	int passedTime = getTimeMs() - lastTickTime;
        	if (passedTime > 200) continue; // @note: As long Mouse::clear CLEARS the whole buffer, it's safe to break here
		// But since it might be rewritten anyway (and hopefully there aren't a lot of messages, we just continue.

		const MouseAction& e = Mouse::getEvent();

		if (!mouseGrabbed && !useTouchscreen()) {
			if (!screen && e.data == MouseAction::DATA_DOWN) {
				grabMouse();
			}
		}

		if (allowGuiClicks && e.action == MouseAction::ACTION_LEFT && e.data == MouseAction::DATA_DOWN) {
			gui.handleClick(MouseAction::ACTION_LEFT, Mouse::getX(), Mouse::getY());
		}

		if (e.action == MouseAction::ACTION_WHEEL) {
			Inventory* v = player->inventory;
			int numSlots = gui.getNumSlots() - 1;
			int slot = (v->selected - e.dy + numSlots) % numSlots;
			v->selectSlot(slot);
		}
		if (mouseDiggable) {
			if (e.action == MouseAction::ACTION_LEFT && e.data == MouseAction::DATA_DOWN) {
				handleMouseClick(MouseAction::ACTION_LEFT);
			}
			if (e.action == MouseAction::ACTION_RIGHT && e.data == MouseAction::DATA_DOWN) {
				handleMouseClick(MouseAction::ACTION_RIGHT);
			}
		}
	}

	TIMER_POP_PUSH("keyboard");
	while (Keyboard::next()) {
		int key = Keyboard::getEventKey();
		bool isPressed = (Keyboard::getEventKeyState() == KeyboardAction::KEYDOWN);
		player->setKey(key, isPressed);

		if (isPressed) {
			gui.handleKeyPressed(key);

			#if defined(WIN32) || defined(RPI)//|| defined(_DEBUG) || defined(DEBUG)
				if (key >= '0' && key <= '9') {
					int digit = key - '0';
					int slot = digit - 1;

					if (slot >= 0 && slot < gui.getNumSlots()-1)
						player->inventory->selectSlot(slot);

					#if defined(WIN32)
						if (digit >= 1 && GetAsyncKeyState(VK_CONTROL) < 0) {
							// Set adventure settings here!
							AdventureSettingsPacket p(level->adventureSettings);
							p.toggle((AdventureSettingsPacket::Flags)(1 << slot));
							p.fillIn(level->adventureSettings);
							raknetInstance->send(p);
						}
						if (digit == 0) {
							Pos pos((int)player->x, (int)player->y-1, (int)player->z);
							SetSpawnPositionPacket p(pos);
							raknetInstance->send(p);
						}
					#endif
				}
			#endif
			#if defined(RPI)
				if (key == Keyboard::KEY_E) {
					screenChooser.setScreen(SCREEN_BLOCKSELECTION);
				}
				if (!screen && key == Keyboard::KEY_O || key == 250) {
					releaseMouse();
				}
			#endif
			#if defined(WIN32)
				// While flying, E climbs and Q descends, so they must not
				// open the block selection / drop the held item at the same
				// time. Outside of flight they keep their normal meaning.
				if (key == Keyboard::KEY_E && !options.isFlying) {
					screenChooser.setScreen(SCREEN_BLOCKSELECTION);
				}

				if (key == Keyboard::KEY_F) {
					// Left as it was: F toggles flight, holding it is the
					// fast modifier. Only the space double tap is new.
					options.isFlying = !options.isFlying;
					player->noPhysics = options.isFlying;
					if (!options.isFlying)
						options.fastFlight = false;
				}

				if (key == Keyboard::KEY_T) {
					options.thirdPersonView = !options.thirdPersonView;
					/*
					ImprovedNoise noise;
					for (int i = 0; i < 16; ++i)
						printf("%d\t%f\n", i, noise.grad2(i, 3, 8));
					*/
				}

				if (key == Keyboard::KEY_O) {
					useAmbientOcclusion = !useAmbientOcclusion;
					options.ambientOcclusion = useAmbientOcclusion;
					levelRenderer->allChanged();
				}

				// L walks the quality ladder down, K walks it back up.
				if (key == Keyboard::KEY_L)
					stepGraphicsQuality(-1);
				if (key == Keyboard::KEY_K)
					stepGraphicsQuality(+1);

				if (key == Keyboard::KEY_U) {
					onGraphicsReset();
					player->heal(100);
				}

				if (key == Keyboard::KEY_B || key == 108) // Toggle the game mode
					setIsCreativeMode(!isCreativeMode());

				if (key == Keyboard::KEY_P) // Step forward in time
					level->setTime( level->getTime() + 1000);

				if (key == Keyboard::KEY_G) {
					setScreen(new ArmorScreen());
					/*
					std::vector<AABB>& boxs = level->getCubes(NULL, AABB(128.1f, 73, 128.1f, 128.9f, 74.9f, 128.9f));
					LOGI("boxes: %d\n", (int)boxs.size());
					*/
				}

				if (key == Keyboard::KEY_Y) {
					textures->reloadAll();
					player->hurtTo(2);
				}
				if (key == Keyboard::KEY_Z || key == 108) {
					for (int i = 0; i < 1; ++i) {
						Mob* mob = NULL;
						int forceId = 0;//MobTypes::Sheep;

						int types[] = {
							MobTypes::Sheep,
							MobTypes::Pig,
							MobTypes::Chicken,
							MobTypes::Cow,
						};

						int mobType = (forceId > 0)? forceId : types[Mth::random(sizeof(types) / sizeof(int))];
						mob = MobFactory::CreateMob(mobType, level);

						//((Animal*)mob)->setAge(-1000);
						float dx = 4 - 8 * Mth::random() + 4 * Mth::sin(Mth::DEGRAD * player->yRot);
						float dz = 4 - 8 * Mth::random() + 4 * Mth::cos(Mth::DEGRAD * player->yRot);
						if (mob && !MobSpawner::addMob(level, mob, player->x + dx, player->y, player->z + dz, Mth::random()*360, 0, true))
							delete mob;
					}
				}

				if (key == Keyboard::KEY_X) {
					const EntityList& entities = level->getAllEntities();
					for (int i = entities.size()-1; i >= 0; --i) {
						Entity* e = entities[i];
						if (!e->isPlayer())
							level->removeEntity(e);
					}
				}

				if (key == Keyboard::KEY_C /*|| key == 4*/) {
					player->inventory->clearInventoryWithDefault();
					// @todo: Add saving here for benchmarking
				}
				if (key == Keyboard::KEY_H) {
					setScreen( new PrerenderTilesScreen() );
				}

				if (key == Keyboard::KEY_O) {
					for (int i = Inventory::MAX_SELECTION_SIZE; i < player->inventory->getContainerSize(); ++i)
						if (player->inventory->getItem(i))
							player->inventory->dropSlot(i, false);
				}
				if (key == Keyboard::KEY_F3) {
					options.renderDebug = !options.renderDebug;
				}
				if (key == Keyboard::KEY_M) {
					options.difficulty = (options.difficulty == Difficulty::PEACEFUL)?
						Difficulty::NORMAL : Difficulty::PEACEFUL;
					//setIsCreativeMode( !isCreativeMode() );
				}

				if (options.renderDebug) {
					if (key >= '0' && key <= '9') {
						_perfRenderer->debugFpsMeterKeyPress(key - '0');
					}
				}
			#endif

				if (key == Keyboard::KEY_ESCAPE || key == 82)
					pauseGame(false);

			#ifndef OPENGL_ES
				if (key == Keyboard::KEY_P) {
					static bool isWireFrame = false;
					isWireFrame = !isWireFrame;
					glPolygonMode(GL_FRONT, isWireFrame? GL_LINE : GL_FILL);
					//glPolygonMode(GL_BACK, isWireFrame? GL_LINE : GL_FILL);
				}
			#endif
		}
		#ifdef WIN32
			if (key == Keyboard::KEY_M) {
				for (int i = 0; i < 5 * SharedConstants::TicksPerSecond; ++i)
					level->tick();
			}
		#endif


		//if (!isPressed) LOGI("Key released: %d\n", key);

		if (!options.useMouseForDigging) {
			int passedTime = getTimeMs() - lastTickTime;
			if (passedTime > 200) continue;

			// Destroy and attack is on same button
			if (key == options.keyDestroy.key && isPressed) {
				BuildActionIntention bai(BuildActionIntention::BAI_REMOVE | BuildActionIntention::BAI_ATTACK);
				handleBuildAction(&bai);
			}
			else // Build and use/interact is on same button
			if (key == options.keyUse.key && isPressed) {
				BuildActionIntention bai(BuildActionIntention::BAI_BUILD | BuildActionIntention::BAI_INTERACT);
				handleBuildAction(&bai);
			}
		}
	}

	TIMER_POP_PUSH("tickbuild");
	BuildActionIntention bai;
	// @note: This might be a problem. This method is polling based here, but
	//        event based when using mouse (or keys..), and in java. Not quite
	//        sure just yet what way to go.
	bool buildHandled = inputHolder->getBuildInput()->tickBuild(player, &bai);
	if (buildHandled) {
		if (!bai.isRemoveContinue())
			handleBuildAction(&bai);
	}

	const bool isUsingLeft = (Mouse::isButtonDown(MouseAction::ACTION_LEFT) && mouseDiggable)
		|| Keyboard::isKeyDown(options.keyDestroy.key)
		|| (buildHandled && bai.isRemove());

	const bool isUsingRight = (Mouse::isButtonDown(MouseAction::ACTION_RIGHT) && mouseDiggable)
		|| Keyboard::isKeyDown(options.keyUse.key)
		|| (buildHandled && bai.isBuild());

	TIMER_POP_PUSH("handlemouse");
	if (player && player->isUsingItem()) {
		if (!isUsingRight) {
			gameMode->releaseUsingItem(player);
		}
	} else {
		handleMouseDown(MouseAction::ACTION_LEFT, isUsingLeft);
	}

	lastTickTime = getTimeMs();

	TIMER_POP();
#endif
}

void Minecraft::handleMouseDown(int button, bool down) {
#ifndef STANDALONE_SERVER
	if(player->isSleeping()) {
		return;
	}
    if (button == MouseAction::ACTION_LEFT && missTime > 0) return;
	if (down && hitResult.type == TILE && button == MouseAction::ACTION_LEFT && !hitResult.indirectHit) {
        int x = hitResult.x;
        int y = hitResult.y;
        int z = hitResult.z;
        gameMode->continueDestroyBlock(x, y, z, hitResult.f);
        particleEngine->crack(x, y, z, hitResult.f);
		player->swing();
    } else {
        gameMode->stopDestroyBlock();
    }
#endif
}

void Minecraft::handleMouseClick(int button) {
#ifndef STANDALONE_SERVER
	BuildActionIntention bai(
		(button == MouseAction::ACTION_LEFT)?
			(BuildActionIntention::BAI_REMOVE | BuildActionIntention::BAI_ATTACK | BuildActionIntention::BAI_FIRSTREMOVE)
		:	(BuildActionIntention::BAI_BUILD | BuildActionIntention::BAI_INTERACT));

	handleBuildAction(&bai);
#endif
}

void Minecraft::handleBuildAction(BuildActionIntention* action) {
#ifndef STANDALONE_SERVER
	if (action->isRemove() || action->isAttack()) {
		if (missTime > 0) return;
		player->swing();
	}
	if(player->isUsingItem())
		return;
    bool mayUse = true;

	if (!hitResult.isHit()) {
		if (action->isRemove() && !gameMode->isCreativeType()) {
			missTime = 10;
		}
    } else if (hitResult.type == ENTITY) {
        if (action->isAttack() || action->isRemove()) {
			player->swing();
			//LOGI("attacking!\n");
			InteractPacket packet(InteractPacket::Attack, player->entityId, hitResult.entity->entityId);
			raknetInstance->send(packet);
            gameMode->attack(player, hitResult.entity);
		} else if (action->isInteract() || action->isBuild()) {
			if (hitResult.entity->interactPreventDefault())
				mayUse = false;
			//LOGI("interacting!\n");
			InteractPacket packet(InteractPacket::Interact, player->entityId, hitResult.entity->entityId);
			raknetInstance->send(packet);
            gameMode->interact(player, hitResult.entity);
        }
    } else if (hitResult.type == TILE) {
		int x = hitResult.x;
        int y = hitResult.y;
        int z = hitResult.z;
        int face = hitResult.f;

		int oldTileId = level->getTile(x, y, z);
        Tile* oldTile = Tile::tiles[oldTileId];

		//bool tryDestroyBlock = false;

		if (action->isRemove()) {
			if (!oldTile)
				return;

			//LOGI("tile: %s - %d, %d, %d. b: %f - %f\n", oldTile->getDescriptionId().c_str(), x, y, z, oldTile->getBrightness(level, x, y, z), oldTile->getBrightness(level, x, y+1, z));
            level->extinguishFire(x, y, z, hitResult.f);
			gameMode->startDestroyBlock(x, y, z, hitResult.f);
        }
		else {
			ItemInstance* item = player->inventory->getSelected();
 			if (gameMode->useItemOn(player, level, item, x, y, z, face, hitResult.pos)) {
			    mayUse = false;
			    player->swing();
			#ifdef RPI
				} else if (item && item->id == ((Item*)Item::sword_iron)->id) {
					player->swing();
			#endif
			}
			if (item && item->count <= 0) {
				player->inventory->clearSlot(player->inventory->selected);
			}
			//} else if (item && item->count != oldCount) {
			//	  gameRenderer->itemInHandRenderer->itemPlaced();
			//}
		}
	}
	if (mayUse && (action->isInteract() || action->isBuild())) {
		ItemInstance* item = player->inventory->getSelected();
		if (item && !player->isUsingItem()) {
			if (gameMode->useItem(player, level, item)) {
				gameRenderer->itemInHandRenderer->itemUsed();
			}
			if (item && item->count <= 0) {
				player->inventory->clearSlot(player->inventory->selected);
			}
		}
	}
#endif
}

bool Minecraft::isOnlineClient()
{
    return (level != NULL && level->isClientSide);
}

bool Minecraft::isOnline()
{
	return netCallback != NULL;
}

void Minecraft::pauseGame(bool isBackPaused) {
#ifndef STANDALONE_SERVER
	if (screen != NULL) return;
	screenChooser.setScreen(isBackPaused? SCREEN_PAUSEPREV : SCREEN_PAUSE);
#endif
}
void Minecraft::gameLostFocus() {
#ifndef STANDALONE_SERVER
	if(screen != NULL) {
		screen->lostFocus();
	}
#endif
}


void Minecraft::setScreen( Screen* screen )
{
#ifndef	STANDALONE_SERVER
	Mouse::reset();
	Multitouch::reset();
	Multitouch::resetThisUpdate();

	if (screenMutex) {
		hasScheduledScreen = true;
		scheduledScreen = screen;
		return;
	}

	if (screen != NULL && screen->isErrorScreen())
		return;
	if (screen == NULL && level == NULL)
		screen = screenChooser.createScreen(SCREEN_STARTMENU);

	if (this->screen != NULL) {
		this->screen->removed();
		delete this->screen;
	}

	this->screen = screen;
	if (screen != NULL) {
		releaseMouse();
		//ScreenSizeCalculator ssc = new ScreenSizeCalculator(options, width, height);
		int screenWidth = (int)(width * Gui::InvGuiScale); //ssc.getWidth();
		int screenHeight = (int)(height * Gui::InvGuiScale); //ssc.getHeight();
		screen->init(this, screenWidth, screenHeight);

		if (screen->isInGameScreen() && level) {
			level->saveLevelData();
            level->saveGame();
        }

		//noRender = false;
	} else {
		grabMouse();
	}
#endif
}

void Minecraft::grabMouse()
{
#ifndef STANDALONE_SERVER
	if (mouseGrabbed) return;
	mouseGrabbed = true;
	mouseHandler.grab();
	//setScreen(NULL);
#endif
}

void Minecraft::releaseMouse()
{
#ifndef STANDALONE_SERVER
	if (!mouseGrabbed) {
		return;
	}
	if (player) {
		player->releaseAllKeys();
	}
	mouseGrabbed = false;
	mouseHandler.release();
#endif
}

bool Minecraft::useTouchscreen() {
#if defined(WIN32) || defined(_WIN32) || defined(RPI)
	return options.useTouchScreen;
#else
	return options.useTouchScreen || !_supportsNonTouchscreen;
#endif
}
bool Minecraft::supportNonTouchScreen() {
	return _supportsNonTouchscreen;
}
void Minecraft::init()
{
	printf("[dbg] init: enter\n"); fflush(stdout);
	options.minecraft = this;
	options.initDefaultValues();
	printf("[dbg] init: defaults\n"); fflush(stdout);
#ifndef STANDALONE_SERVER
	checkGlError("Init enter");
	printf("[dbg] init: checkGl ok\n"); fflush(stdout);

	_supportsNonTouchscreen = !platform()->supportsTouchscreen();

	LOGI("IS TOUCHSCREEN? %d\n", options.useTouchScreen);
	printf("[dbg] init: touchscreen done\n"); fflush(stdout);

	textures = new Textures(&options, platform());
	printf("[dbg] init: new Textures\n"); fflush(stdout);
	// Dynamic water simulation and per-frame texture uploads to the GPU
	// are disabled to save CPU cycles and eliminate GPU fillrate stalls.
	// textures->addDynamicTexture(new WaterTexture());
	// textures->addDynamicTexture(new WaterSideTexture());
	gui.texturesLoaded(textures);
	printf("[dbg] init: texturesLoaded\n"); fflush(stdout);

	levelRenderer = new LevelRenderer(this);
	printf("[dbg] init: LevelRenderer\n"); fflush(stdout);
	gameRenderer = new GameRenderer(this);
	printf("[dbg] init: GameRenderer\n"); fflush(stdout);
	particleEngine = new ParticleEngine(level, textures);
	printf("[dbg] init: ParticleEngine\n"); fflush(stdout);

	// Platform specific initialization here
	font = new Font(&options, "font/default8.png", textures);
	printf("[dbg] init: Font\n"); fflush(stdout);

	_perfRenderer = new PerfRenderer(this, font);
	printf("[dbg] init: PerfRenderer\n"); fflush(stdout);

	checkGlError("Init complete");
	printf("[dbg] init: gl complete\n"); fflush(stdout);
#endif

	user = new User("TestUser", "");
	printf("[dbg] init: user\n"); fflush(stdout);
	setIsCreativeMode(false); // false means it's Survival Mode
	reloadOptions();
	printf("[dbg] init: reloadOptions\n"); fflush(stdout);

}

void Minecraft::setSize(int w, int h) {
#ifndef STANDALONE_SERVER
    transformResolution(&w, &h);

	width  = w;
	height = h;

	if (width >= 1000) {
        #ifdef __APPLE__
            Gui::GuiScale = (width > 2000)? 8.0f : 4.0f;
        #else
            Gui::GuiScale = 4.0f;
        #endif
    }
	else if (width >= 800) {
#ifdef __APPLE__
        Gui::GuiScale = 4.0f;
#else
		Gui::GuiScale = 3.0f;
#endif
    }
	else if (width >= 400)
		Gui::GuiScale = 2.0f;
	else
		Gui::GuiScale = 1.0f;

#ifndef __APPLE__
	// The "Gui Scale" option used to be ignored completely: the scale was
	// always derived from the window width. Let the option win when it is set.
	if (options.guiScale >= 1 && options.guiScale <= 4)
		Gui::GuiScale = (float)options.guiScale;
#endif

	Gui::InvGuiScale = 1.0f / Gui::GuiScale;
	int screenWidth  = (int)(width  * Gui::InvGuiScale);
	int screenHeight = (int)(height * Gui::InvGuiScale);

	if (platform()) {
		float pixelsPerMillimeter = options.getProgressValue(&Options::Option::PIXELS_PER_MILLIMETER);
		pixelCalc.setPixelsPerMillimeter(pixelsPerMillimeter);
		pixelCalcUi.setPixelsPerMillimeter(pixelsPerMillimeter * Gui::InvGuiScale);
	}

	Config config = createConfig(this);
	gui.onConfigChanged(config);
    
	if (screen)
		screen->setSize(screenWidth, screenHeight);

	if (inputHolder)
		inputHolder->onConfigChanged(config);
	//LOGI("Setting size: %d, %d: %f\n", width, height, Gui::InvGuiScale);

#ifdef WIN32
	char resbuf[128];
	sprintf(resbuf, "            %d x %d @ scale %.2f", width, height, Gui::GuiScale);
	//gui.addMessage(resbuf);
#endif
#endif /* STANDALONE_SERVER */
}

void Minecraft::reloadOptions() {
	// Was options.update(), which is the legacy Apple loader: it only knew a
	// handful of keys, forced viewDistance to 2 and clamped the difficulty to
	// peaceful/normal, then re-saved the file. That is what made the settings
	// "forget" themselves, so use the real loader now.
	options.load();

	bool wasTouchscreen = options.useTouchScreen;
	options.useTouchScreen = useTouchscreen();
	options.save();

	if ((wasTouchscreen != options.useTouchScreen) || (inputHolder == 0))
		_reloadInput();

	user->name = options.username;

	// Put the quality ladder onto the level that was saved, otherwise the
	// fog/lighting/distance it sets would be overwritten by the raw options
	// the next time the file is read.
	applyGraphicsQuality();

	LOGI("Reloading-options (quality rung %d, distance %d, fog %d)\n",
		options.graphicsLevel + 1, options.viewDistance, (int)options.fog);

    // @todo @fix Android and iOS behaves a bit differently when leaving
    //            an options screen (Android recreates OpenGL surface)
    setSize(width, height);
}

void Minecraft::_reloadInput() {
#ifndef STANDALONE_SERVER
	delete inputHolder;

	if (useTouchscreen()) {
		inputHolder = new TouchInputHolder(this, &options);
	} else {
		#if defined(ANDROID) || defined(__APPLE__) 
			inputHolder = new CustomInputHolder(
				new XperiaPlayInput(&options),
				new ControllerTurnInput(2, ControllerTurnInput::MODE_DELTA),
				new IBuildInput());
		#else
			inputHolder = new CustomInputHolder(
				new KeyboardInput(&options),
				new MouseTurnInput(MouseTurnInput::MODE_DELTA, width/2, height/2),
				new MouseBuildInput());
		#endif
	}

	mouseHandler.setTurnInput(inputHolder->getTurnInput());
	if (level && player) {
		player->input = inputHolder->getMoveInput();
	}
#endif
}


//
// Multiplayer
//
void Minecraft::locateMultiplayer() {
	isLookingForMultiplayer = true;

	raknetInstance->pingForHosts(19132);
	netCallback = new ClientSideNetworkHandler(this, raknetInstance);
}

void Minecraft::cancelLocateMultiplayer() {
	isLookingForMultiplayer = false;

	raknetInstance->stopPingForHosts();

	delete netCallback;
	netCallback = NULL;
}

bool Minecraft::joinMultiplayer( const PingedCompatibleServer& server )
{
	LOGI("joinMultiplayer: addr=%s port=%d looking=%d cb=%d serverCount=%d\n",
		server.address.ToString(false), server.address.GetPort(),
		(int)isLookingForMultiplayer, (int)(netCallback != NULL),
		(int)raknetInstance->getServerList().size());

	if (isLookingForMultiplayer && netCallback) {
		isLookingForMultiplayer = false;
		bool ok = raknetInstance->connect(server.address.ToString(false), server.address.GetPort());
		LOGI("joinMultiplayer: connect returned %d\n", (int)ok);
		return ok;
	}
	LOGE("joinMultiplayer: refused (isLookingForMultiplayer=%d, netCallback=%p)\n",
		(int)isLookingForMultiplayer, (void*)netCallback);
	return false;
}

void Minecraft::hostMultiplayer(int port) {
    // Tear down last instance
    raknetInstance->disconnect();
    delete netCallback;
    netCallback = NULL;

#if !defined(NO_NETWORK)
	netCallback = new ServerSideNetworkHandler(this, raknetInstance);
    #ifdef STANDALONE_SERVER
        raknetInstance->host(user->name, port, 16);
    #else
        raknetInstance->host(user->name, port);
    #endif
#endif
}

//
// Level generation
//
/*static*/

	void* Minecraft::prepareLevel_tspawn(void *p_param)
{
	Minecraft* mc = (Minecraft*) p_param;
	mc->generateLevel("Currently not used", mc->level);
	return 0;
}

void Minecraft::generateLevel( const std::string& message, Level* level )
{
	Stopwatch s;
	s.start();
	prepareLevel(message);
	s.stop();
	s.print("Level generated: ");

	// "Unlock"
	isGeneratingLevel = false;
}

void Minecraft::_levelGenerated()
{
#ifndef STANDALONE_SERVER
	if (player == NULL) {
		player = (LocalPlayer*) gameMode->createPlayer(level);
		gameMode->initPlayer(player);
	}

	if (player) {
		player->input = inputHolder->getMoveInput();
	}

	if (levelRenderer != NULL) levelRenderer->setLevel(level);
	if (particleEngine != NULL) particleEngine->setLevel(level);

	gameMode->adjustPlayer(player);
	gui.onLevelGenerated();
#endif

	level->validateSpawn();
	level->loadPlayer(player, true);
	// if we are client side, we trust the server to have given us a correct position
	if (player && !level->isClientSide) {
		player->resetPos(false);
	}

	this->cameraTargetPlayer = player;

	if (raknetInstance->isServer())
		raknetInstance->announceServer(user->name);

	if (netCallback) {
		netCallback->levelGenerated(level);
	}

#if defined(WIN32) || defined(RPI)
	if (_commandServer) {
		delete _commandServer;
	}
	_commandServer = new CommandServer(this);
	_commandServer->init(commandPort);
#endif

	// Hack to (hopefully) get the players to show (note: in LevelListener
	// instead, since adding yourself always generates a entityAdded)
	//EntityRenderDispatcher::getInstance()->onGraphicsReset();
	_hasSignaledGeneratingLevelFinished = true;
}

Player* Minecraft::respawnPlayer(int playerId) {
	for (unsigned int i = 0; i < level->players.size(); ++i) {
		if (level->players[i]->entityId == playerId) {
			resetPlayer(level->players[i]);
			return level->players[i];
		}
	}
	return NULL;
}

void Minecraft::resetPlayer(Player* player) {
	level->validateSpawn();
	player->reset();

	Pos p;
	if(player->hasRespawnPosition()) {
		p = player->getRespawnPosition();
	}
	else {
		p = level->getSharedSpawnPos();
	}
	player->setPos((float)p.x + 0.5f, (float)p.y + 1.0f, (float)p.z + 0.5f);
	player->resetPos(true);

	if (isCreativeMode())
		player->inventory->clearInventoryWithDefault();
}

void Minecraft::respawnPlayer() {
	// RESET THE FRACKING PLAYER HERE
	//bool slowCheck = false;
	//for (int i = 0; i < level->entities.size(); ++i)
	//	if (level->entities[i] == player) slowCheck = true;
	//
	//LOGI("Has entity? %d, %d\n", level->getEntity(player->entityId), slowCheck);

	resetPlayer(player);

	// tell server (or other client) that we re-spawned
	RespawnPacket packet(player);
	raknetInstance->send(packet);
}

void Minecraft::onGraphicsReset()
{
#ifndef STANDALONE_SERVER
	textures->clear();
	
	font->onGraphicsReset();
	gui.onGraphicsReset();

	if (levelRenderer) levelRenderer->onGraphicsReset();
	if (gameRenderer) gameRenderer->onGraphicsReset();

	EntityRenderDispatcher::getInstance()->onGraphicsReset();
	TileEntityRenderDispatcher::getInstance()->onGraphicsReset();
#endif
}

// The quality ladder behind the L and K keys. L walks it down, K back up.
//
//   rung 1  render distance as it is, everything on
//   rung 2  same, still everything on
//   rung 3  fog removed, simple graphics
//   rung 4  base distance - 2 chunks
//   rung 5  base distance - 4 chunks
//   rung 6  base distance - 6 chunks
//   rung 7  base distance - 8 chunks
//   rung 8  base distance - 10 chunks, never below two
//
// The rungs used to be ordered the other way round: the distance was only cut
// on rungs 5..8 while the game started on rung 4, so pressing L walked
// 4 -> 3 -> 2 -> 1 and never reached a single distance step. That is why L
// appeared to do nothing to the chunks. The game now starts on rung 1, so L
// walks all the way down and the distance really drops, and neither end wraps
// around (the wrap is what used to make the sky jump).
void Minecraft::stepGraphicsQuality(int delta)
{
	int level = options.graphicsLevel + delta;
	if (level < Options::QUALITY_LEVEL_MIN) level = Options::QUALITY_LEVEL_MIN;
	if (level > Options::QUALITY_LEVEL_MAX) level = Options::QUALITY_LEVEL_MAX;
	if (level == options.graphicsLevel)
		return;
	options.graphicsLevel = level;
	applyGraphicsQuality();
	LOGI("graphics: quality rung %d of %d (distance %d, fog %d, fancy %d)\n",
		level + 1, Options::QUALITY_LEVEL_MAX + 1, options.viewDistance,
		(int)options.fog, (int)options.fancyGraphics);
}

void Minecraft::applyGraphicsQuality()
{
	// The distance the player picked in the settings is the ceiling of the
	// ladder; every rung below 4 shaves chunks off it.
	int base = options.graphicsBaseDistance;
	if (base < Options::RENDER_DISTANCE_MIN) base = Options::RENDER_DISTANCE_MIN;
	if (base > Options::RENDER_DISTANCE_MAX) base = Options::RENDER_DISTANCE_MAX;

	int level = options.graphicsLevel;
	if (level < Options::QUALITY_LEVEL_MIN) level = Options::QUALITY_LEVEL_MIN;
	if (level > Options::QUALITY_LEVEL_MAX) level = Options::QUALITY_LEVEL_MAX;

	// Rungs 1..3 keep the base distance, rungs 4..8 drop two chunks each, so
	// every single step from 4 onwards visibly shrinks the world.
	int distance = base;
	if (level >= 3)
		distance = base - (level - 2) * 2;
	if (distance < Options::QUALITY_MIN_DISTANCE) distance = Options::QUALITY_MIN_DISTANCE;
	if (distance > Options::RENDER_DISTANCE_MAX) distance = Options::RENDER_DISTANCE_MAX;

	// Foliage (fancy graphics) and fog start off on low rungs. Simple opaque
	// leaves save significant fill rate on the weak Tegra 3 GPU.
	const bool wantFog = level == 0 && options.fog;
	const bool wantFancy = level == 0 && options.fancyGraphics;
	const bool fogChanged = options.fog != wantFog;
	const bool fancyChanged = options.fancyGraphics != wantFancy;
	const bool distanceChanged = options.viewDistance != distance;

	options.viewDistance = distance;
	options.fog = wantFog;
	options.fancyGraphics = wantFancy;

	// allChanged() throws away every loaded chunk and rebuilds the whole
	// chunk array, which is the stutter. It is only needed when the number of
	// chunks changes or the foliage atlas has to be rebuilt. Fog does not
	// touch the chunks at all, so rung 2 -> 3 is free.
	//
	// onGraphicsReset() is deliberately not called here: it does
	// textures->clear() plus a full entity/tile-entity reset, and that flush
	// was a far bigger stall than the chunk rebuild it was meant to fix.
	// LevelRenderer::allChanged() already does Tile::leaves->setFancy() and
	// rebuilds the atlas, which is all the foliage change needs.
	if (levelRenderer && (distanceChanged || fancyChanged))
		levelRenderer->allChanged();
	LOGI("graphics: applied rung %d (distance %d, fog %d, fancy %d, reload=%d)\n",
		level + 1, options.viewDistance, (int)options.fog, (int)options.fancyGraphics,
		(int)(distanceChanged || fancyChanged));
}

int Minecraft::getProgressStatusId() {
	return progressStageStatusId;
}

const char* Minecraft::getProgressMessage()
{
	return progressMessages[progressStageStatusId];
}

bool Minecraft::isLevelGenerated()
{
	return level != NULL && !isGeneratingLevel;
}

LevelStorageSource* Minecraft::getLevelSource()
{
	return storageSource;
}

int Minecraft::getLicenseId() {
	if (!LicenseCodes::isReady(_licenseId))
		_licenseId = platform()->checkLicense();
	return _licenseId;
}

void Minecraft::audioEngineOn() {
#ifndef STANDALONE_SERVER
    soundEngine->enable(true);
#endif
}
void Minecraft::audioEngineOff() {
#ifndef STANDALONE_SERVER
    soundEngine->enable(false);
#endif
}

void Minecraft::setIsCreativeMode(bool isCreative)
{
#ifdef CREATORMODE
	delete gameMode;
	gameMode = new CreatorMode(this);
	_isCreativeMode = true;
#else
	if (!gameMode || isCreative != _isCreativeMode)
	{
		delete gameMode;
		if (isCreative) gameMode = new CreativeMode(this);
		else			gameMode = new SurvivalMode(this);
		_isCreativeMode = isCreative;
	}
#endif
	if (player)
		gameMode->initAbilities(player->abilities);
}

bool Minecraft::isCreativeMode() {
	return _isCreativeMode;
}

bool Minecraft::isKindleFire(int kindleVersion) {
	if (kindleVersion != 1)
		return false;

	std::string model = platform()->getPlatformStringVar(PlatformStringVars::DEVICE_BUILD_MODEL);
	std::string modelLower(model);
	std::transform(modelLower.begin(), modelLower.end(), modelLower.begin(), tolower);

	return (modelLower.find("kindle") != std::string::npos) && (modelLower.find("fire") != std::string::npos);
}

bool Minecraft::transformResolution(int* w, int* h)
{
	bool changed = false;

	// Kindle Fire 1: reporting wrong height in
	// certain cases (e.g. after screen lock)
	if (isKindleFire(1) && *h >= 560 && *h <= 620) {
		*h = 580;
		changed = true;
	}

	return changed;
}

ICreator* Minecraft::getCreator()
{
#ifdef CREATORMODE
	return ((CreatorMode*)gameMode)->getCreator();
#else
	return NULL;
#endif
}

void Minecraft::optionUpdated( const Options::Option* option, bool value ) {
	if(netCallback != NULL && option == &Options::Option::SERVER_VISIBLE) {
		ServerSideNetworkHandler* ss = (ServerSideNetworkHandler*) netCallback;
		ss->allowIncomingConnections(value);
	}
#ifndef STANDALONE_SERVER
	// The D-pad geometry is built by the input holder, so a new size or a
	// switch-off only takes effect once it re-reads the options.
	if (option == &Options::Option::TOUCH_BUTTONS
	 || option == &Options::Option::TOUCH_BUTTON_SIZE
	 || option == &Options::Option::LEFT_HANDED) {
		if (inputHolder)
			inputHolder->onConfigChanged(createConfig(this));
	}
#endif
}

void Minecraft::optionUpdated( const Options::Option* option, float value ) {
#ifndef STANDALONE_SERVER
	if(option == &Options::Option::PIXELS_PER_MILLIMETER) {
		pixelCalcUi.setPixelsPerMillimeter(value * Gui::InvGuiScale);
		pixelCalc.setPixelsPerMillimeter(value);
	}
	// The touch look sensitivity is applied by the input holder, so it only
	// takes effect once that re-reads the options.
	if(option == &Options::Option::TOUCH_SENSITIVITY
	 || option == &Options::Option::SENSITIVITY) {
		if (inputHolder)
			inputHolder->onConfigChanged(createConfig(this));
	}
#endif
}

void Minecraft::optionUpdated( const Options::Option* option, int value ) {
#ifndef STANDALONE_SERVER
	if (option == &Options::Option::TOUCH_BUTTON_SIZE) {
		if (inputHolder)
			inputHolder->onConfigChanged(createConfig(this));
	}
	// The render distance slider used to write the new value and nothing
	// else, so the chunk array was never rebuilt and the world looked
	// exactly the same after moving the slider.
	if (option == &Options::Option::RENDER_DISTANCE) {
		if (levelRenderer)
			levelRenderer->allChanged();
	}
#endif
}
