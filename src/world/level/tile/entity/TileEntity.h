#ifndef NET_MINECRAFT_WORLD_LEVEL_TILE_ENTITY__TileEntity_H__
#define NET_MINECRAFT_WORLD_LEVEL_TILE_ENTITY__TileEntity_H__

//package net.minecraft.world.level.tile.entity;

#include <map>
#include <string>
#include <vector>

#include "TileEntityRendererId.h"

class Level;
class Tile;
class TilePos;
class CompoundTag;
class Packet;

namespace TileEntityType
{
	const int Furnace = 0;
	const int Chest = 1;
	const int NetherReactor = 2;
	const int Sign = 3;
};

class TileEntity
{
public:
	typedef std::map<std::string, int> MapIdType;
	typedef std::map<int, std::string> MapTypeId;

	static void initTileEntities();
	static void teardownTileEntities();

	static TileEntity* loadStatic(CompoundTag* tag);

	static bool isType(TileEntity* te, int Type);

	TileEntity(int tileEntityType);

	virtual void load(CompoundTag* tag);
	virtual bool save(CompoundTag* tag);
	virtual bool shouldSave() { return true; }
	virtual void tick() {}
	virtual void triggerEvent(int b0, int b1);
	virtual void setRemoved();
	virtual void clearCache();
	virtual bool isFinished();
	virtual Packet* getUpdatePacket();
	virtual void setLevelAndPos(Level* level, int x, int y, int z);

	int getData();
	void setData(int data);
	void setChanged();
	float distanceToSqr(float xPlayer, float yPlayer, float zPlayer);
	Tile* getTile();
	bool isRemoved() const;
	void clearRemoved();
	bool isType(int Type);

	static MapIdType idClassMap;
	static MapTypeId classIdMap;
	static int _runningId;

	int x, y, z;
	int data;
	int type;
	bool remove;
	Level* level;
	Tile* tile;
	bool clientSideOnly;
	TileEntityRendererId rendererId;
	int runningId;

protected:
	static void setId(int type, const std::string& id);
};

class TileEntityFactory
{
public:
	static TileEntity* createTileEntity(int type);
};

int partitionTileEntities(const std::vector<TileEntity*>& in, std::vector<TileEntity*>& keep, std::vector<TileEntity*>& dontKeep);

#endif /*NET_MINECRAFT_WORLD_LEVEL_TILE_ENTITY__TileEntity_H__*/