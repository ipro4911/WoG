#pragma once
// master "enums" file, to be put in a namespace OR even a GameEngine class later
#include "Ogre.h"

// game specifics
#include "MagixObject.h"

using namespace Ogre;

enum QueryFlags
{
	OBJECT_MASK = 1 << 0,
	UNIT_MASK = 1 << 1,
	SKY_MASK = 1 << 2,
	WORLDOBJECT_MASK = 1 << 3,
	EFFECT_MASK = 1 << 4,
	ITEM_MASK = 1 << 5,
	CRITTER_MASK = 1 << 6
};

enum OBJECT_TYPE
{
	OBJECT_BASIC,
	OBJECT_ANIMATED,
	OBJECT_LIVING,
	OBJECT_UNIT,
	OBJECT_PLAYER,
	OBJECT_ITEM,
	OBJECT_CRITTER
};