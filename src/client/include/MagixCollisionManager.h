// physics handling, bad tho.. to be remade.
#ifndef __MagixCollisionManager_h_
#define __MagixCollisionManager_h_

#include "Ogre.h"
#include "MagixStructs.h"

using namespace Ogre;

class MagixCollisionManager
{
protected:
	SceneManager *mSceneMgr;
	
	list<Wall>::type wall;
	list<Portal>::type portal;
	list<Gate>::type gate;
	list<WaterBox>::type waterBox;
	list<CollBox>::type collBox;
	list<CollSphere>::type collSphere;
public:
	list<Collision>::type coll;
	MagixCollisionManager()
	{
		mSceneMgr = 0;
		coll.clear();
		wall.clear();
		portal.clear();
		gate.clear();
		waterBox.clear();
		collBox.clear();
		collSphere.clear();
	}
	~MagixCollisionManager()
	{
		reset();
		destroyAllWaterBoxes();
		destroyAllPortals();
		destroyAllGates();
	}

	void initialize(SceneManager* sceneMgr);
	
	void reset();
	void createSphereMesh(const std::string& strName, const float r, const int nRings = 16, const int nSegments = 16);
	void createCollision(SceneNode *node, const Real &range, const Vector3 &force, const Vector3 &offset=Vector3::ZERO, const short &hp=0, const unsigned char &alliance=0, bool hitAlly=false);
	const list<Collision>::type::iterator destroyCollision(const list<Collision>::type::iterator &it);
	void destroyCollisionByOwnerNode(SceneNode *node);
	const vector<Collision*>::type getCollisionHitList(const unsigned short& unitID, const unsigned short &alliance, const AxisAlignedBox &target);
	const vector<Collision*>::type getCollisionHitListForCritter(const unsigned short& iID, const unsigned short &alliance, const AxisAlignedBox &target);

	void createBoxWall(const Vector3 &center, const Vector3 &range, bool isInside);
	void createSphereWall(const Vector3 &center, const Real &range, bool isInside);
	const list<Wall>::type::iterator destroyWall(const list<Wall>::type::iterator& it);
	void destroyWall(const unsigned short& iID);
	const vector<Wall*>::type getWallHitList(const Vector3 &target);
	void createPortal(const Vector3 &center, const Real &range, String destName);
	// walls have a ref on iterator so maybe it was missing ?
	const list<Portal>::type::iterator destroyPortal(list<Portal>::type::iterator& it);
	void destroyAllPortals();
	Portal* getPortalHit(const AxisAlignedBox &target);
	void getPortalMap(vector<std::pair<Vector2, String>>::type& map);
	void createWaterBox(const Vector3& center, const Real& scaleX, const Real& scaleZ, bool isSolid = false);
	const list<WaterBox>::type::iterator destroyWaterBox(const list<WaterBox>::type::iterator& it);
	void destroyAllWaterBoxes();
	WaterBox* getWaterBoxHit(const Vector3& target);
	void createGate(const Vector3& center, String destName, const Vector3& destVect, const String& matName, bool hasVectY);
	const list<Gate>::type::iterator destroyGate(list<Gate>::type::iterator &it);
	void destroyAllGates();
	Gate* getGateHit(const AxisAlignedBox &target);
	void getGateMap(vector<std::pair<Vector2, String>>::type &map);
	void createCollBox(const Vector3& center, const Vector3& range);
	const vector<CollBox*>::type getCollBoxHitList(const Vector3 &target, const Real &headHeight);
	void createCollSphere(const Vector3& center, const Real &range);
	const vector<CollSphere*>::type getCollSphereHitList(const Vector3& target);
};
#endif