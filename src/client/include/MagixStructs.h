#pragma once
// master "structs" file, to be put in a namespace OR even a GameEngine class later

//#include "MagixNetworkDefines.h"
//#include "GameConfig.h"
//#include <OgreStreamSerialiser.h>
//#include <OgreException.h>
#include "Ogre.h"

// game specifics
#include "MagixObject.h"

using namespace Ogre;

struct WeatherEvent
{
	Real start;
	Real end;
	ColourValue skyShader;
	ColourValue skyAdder;
	String type;
	Real rate;
	vector<String>::type effect;
	Real effectFrequency;
	bool isConstant;
	WeatherEvent()
	{
		start = 0;
		end = 7000;
		skyShader = ColourValue::White;
		skyAdder = ColourValue::Black;
		type = "";
		rate = 0;
		effect.clear();
		effectFrequency = 0;
		isConstant = false;
	}
};

struct AttackFX
{
	String bone;
	String trailMat;
	Real trailWidth;
	ColourValue colour;
	ColourValue colourChange;
	String trailHeadMat;
	AttackFX()
	{
		bone = "";
		trailMat = "";
		trailWidth = 2;
		colour = ColourValue(1, 0.9, 0.75);
		colourChange = ColourValue(7, 7, 7, 7);
		trailHeadMat = "";
	}
};
struct Attack
{
	String name;
	String anim;
	vector<AttackFX>::type FX;
	Real range;
	Vector3 hitForce;
	Vector3 offset;
	Real hpMin;
	Real hpMax;
	bool hitAlly;
	bool autoTarget;
	Vector3 moveForce;
	bool singleTarget;
	Real speedMultiplier;
	bool soundRoar;
	unsigned short attackRange;
	Attack()
	{
		name = "";
		anim = "";
		FX.clear();
		range = 0;
		hitForce = Vector3::ZERO;
		offset = Vector3::ZERO;
		hpMin = 0;
		hpMax = 0;
		hitAlly = false;
		autoTarget = true;
		moveForce = Vector3::ZERO;
		singleTarget = false;
		speedMultiplier = 1;
		soundRoar = false;
		attackRange = 0;
	}
};

struct CritterAttack
{
	Real hp;
	Real range;
	Vector3 hitForce;
	bool hitAlly;
	CritterAttack(const Real& h, const Real& r, const Vector3& hF, bool hA = false)
	{
		hp = h;
		range = r;
		hitForce = hF;
		hitAlly = hA;
	}
};

struct Critter
{
	String type;
	String mesh;
	Real hp;
	bool flying;
	bool friendly;
	bool invulnerable;
	bool isDrawPoint;
	bool isUncustomizable;
	vector<std::pair<String, Real>>::type dropList;
	std::pair<String, unsigned char> skillDrop;
	Real maxSpeed;
	unsigned char decisionMin;
	unsigned char decisionDeviation;
	Real scale;
	vector<CritterAttack>::type attackList;
	String sound;
	String material;
	Critter()
	{
		type = "";
		mesh = "";
		hp = 100;
		flying = false;
		friendly = false;
		invulnerable = false;
		isDrawPoint = false;
		isUncustomizable = false;
		dropList.clear();
		skillDrop = std::pair<String, unsigned char>("", 0);
		maxSpeed = 100;
		decisionMin = 1;
		decisionDeviation = 9;
		scale = 1;
		attackList.clear();
		sound = "";
		material = "";
	}
};

struct WorldCritter
{
	String type;
	Real spawnRate;
	bool hasRoamArea;
	unsigned short roamAreaID;
	WorldCritter(const String& t = "", const Real& sR = 0, bool hRA = false, const unsigned short& rAID = 0)
	{
		type = t;
		spawnRate = sR;
		hasRoamArea = hRA;
		roamAreaID = rAID;
	}
};

/// <summary>
/// All physics structcs (but some may get removed such as portal / wall)
/// </summary>

struct Collision
{
	SceneNode* mNode;
	Real range;
	Vector3 offset;
	Vector3 force;
	vector<unsigned short>::type hitID;
	vector<unsigned short>::type critterHitID;
	short hp;
	unsigned char alliance;
	bool hitAlly;
	Collision()
	{
		mNode = 0;
		range = 0;
		offset = Vector3::ZERO;
		force = Vector3::ZERO;
		hitID.clear();
		critterHitID.clear();
		hp = 0;
		alliance = 0;
		hitAlly = false;
	}
	const Vector3 getPivot()
	{
		//if(!mCollOwnerNode[iID])return Vector3::ZERO;
		//return mCollOwnerNode[iID]->getPosition()+mCollOwnerNode[iID]->getOrientation()*collOffset[iID];
		if (!mNode)return Vector3::ZERO;
		return mNode->getPosition() + mNode->getOrientation() * offset;
	}
	const Vector3 getForce(bool fromPivot = false, const Vector3& targetPosition = Vector3::ZERO)
	{
		if (fromPivot)
		{
			const Vector3 tVect = targetPosition - getPivot();
			const Quaternion tQuat = force.getRotationTo(tVect);
			return tQuat * force;
		}
		return force;
	}
	bool collides(const AxisAlignedBox& target)
	{
		if (!mNode)return false;
		//if(mCollEnt[iID]->getWorldBoundingSphere().intersects(target))return true;
		if (Sphere(getPivot(), range).intersects(target))return true;
		return false;
	}
	bool hasHitID(const unsigned short& id)
	{
		for (int i = 0; i < (int)hitID.size(); i++)
		{
			if (hitID[i] == id)return true;
		}
		return false;
	}
	bool hasCritterHitID(const unsigned short& id)
	{
		for (int i = 0; i < (int)critterHitID.size(); i++)
		{
			if (critterHitID[i] == id)return true;
		}
		return false;
	}
};

struct Wall
{
	bool isActive;
	bool isSphere;
	Vector3 center;
	Vector3 range;
	bool isInside;
	Wall()
	{
		isActive = false;
		isSphere = false;
		center = Vector3::ZERO;
		range = Vector3::ZERO;
		isInside = false;
	}
	bool collides(const Vector3& target)
	{
		if (!isActive)return false;

		if (isSphere)
		{
			//Fix y coordinate to that of target's to create a Cylinder wall
			const Sphere tSphere(Vector3(center.x, target.y, center.z), range.x);
			if (isInside)return(tSphere.intersects(target));
			else return(!tSphere.intersects(target));
		}
		else
		{
			//Fix y coordinate to that of target's to create an infinitely tall wall
			const AxisAlignedBox tBox(Vector3(center.x - range.x / 2, target.y, center.z - range.z / 2),
				Vector3(center.x + range.x / 2, target.y, center.z + range.z / 2));
			if (isInside)return(tBox.intersects(target));
			else return(!tBox.intersects(target));
		}
		return false;
	}
};

struct Portal
{
	SceneNode *mNode;
	Entity *mEnt;
	String dest;
	Sphere mColSphere;

	Portal()
	{
		mNode = 0;
		mEnt = 0;
		dest = "";
	}

	bool collides(const AxisAlignedBox& target)
	{
		if(!mEnt || dest == "")
		{
			return false;
		}

		//if(mEnt->getWorldBoundingSphere().intersects(target))// getWorldBoundingSphere().intersects(target))
		if(mColSphere.intersects(target))
		{
			// this is kinda moronic : why is this still used if this was wrong ??
			return true; // getWorldBoundingSphere doesn't work properly 
		}

		return false;
	}

	void disable()
	{
		dest = "";
	}
};

struct Gate
{
	SceneNode *mNode;
	Entity *mEnt;
	String dest;
	Vector3 destVect;
	bool hasVectY;
	Sphere mColSphere;

	Gate()
	{
		mNode = 0;
		mEnt = 0;
		dest = "";
		destVect = Vector3::ZERO;
		hasVectY = false;
	}

	bool collides(const AxisAlignedBox &target)
	{
		if (!mEnt || dest == "")
		{
			return false;
		}

		/*if (mEnt->getWorldBoundingSphere().intersects(target))*/
		if(mColSphere.intersects(target))
		{
			return true; // getWorldBoundingSphere doesn't work properly
		}

		return false;
	}

	void disable()
	{
		dest = "";
	}
};

struct WaterBox
{
	bool isActive;
	Vector3 center;
	Vector2 scale;
	bool isSolid;
	WaterBox()
	{
		isActive = false;
		center = Vector3::ZERO;
		scale = Vector2::ZERO;
		isSolid = false;
	}
	bool collides(const Vector3& target)
	{
		if (!isActive)return false;
		if (target.y > center.y)return false;
		//Create infinite depth box
		const AxisAlignedBox tBox(Vector3(center.x - scale.x / 2, target.y, center.z - scale.y / 2),
			Vector3(center.x + scale.x / 2, target.y, center.z + scale.y / 2));
		if (tBox.intersects(target))return true;
		return false;
	}
};

struct CollBox
{
	//Collbox takes center like a typical mesh, i.e. midpoint of "feet"
	Vector3 center;
	Vector3 range;
	CollBox()
	{
		center = Vector3::ZERO;
		range = Vector3::ZERO;
	}
	bool collides(const Vector3& target, const Real& headHeight)
	{
		//Extend below by unit height to detect head collision
		const AxisAlignedBox tBox(Vector3(center.x - range.x / 2, center.y - headHeight, center.z - range.z / 2),
			Vector3(center.x + range.x / 2, center.y + range.y, center.z + range.z / 2));
		if (tBox.intersects(target))return true;
		return false;
	}
};

struct CollSphere
{
	Vector3 center;
	Real range;
	CollSphere()
	{
		center = Vector3::ZERO;
		range = 0;
	   }
	bool collides(const Vector3& target)
	{
		const Sphere tSphere(center, range);
		if (tSphere.intersects(target))return true;
		return false;
	}
};

///
/// Player specific struct
/// 
struct Skill
{
	String name;
	unsigned char stock;
	Skill(const String& skillName, const unsigned char& skillStock)
	{
		name = skillName;
		stock = skillStock;
	}
};

// specific to pets
struct PetFlags
{
	bool stay;
	MagixLiving *attackTarget;
	bool shrink;
	bool evolve;
	bool hasHeal;

	PetFlags()
	{
		stay = false;
		attackTarget = 0;
		shrink = false;
		evolve = false;
		hasHeal = false;
	}

	void reset()
	{
		attackTarget = 0;
	}
};

struct HitInfo
{
	unsigned short ID;
	Real hp;
	Vector3 force;
	bool isMine;
	HitInfo(const unsigned short& i, const Real& h, const Vector3& f, bool iM = false)
	{
		ID = i;
		hp = h;
		force = f;
		isMine = iM;
	}
};