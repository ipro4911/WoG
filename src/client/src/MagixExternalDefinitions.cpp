#include "MagixExternalDefinitions.h"

void MagixExternalDefinitions::initialize()
{
	loadUnitMeshes("UnitMeshes.cfg");
	loadUnitEmotes("UnitEmotes.cfg");
	loadItems((ENCRYPTED_ITEMS) ? "Items.dat" : "Items.cfg", ENCRYPTED_ITEMS);
	loadHotkeys("Hotkeys.cfg");
	attackList.clear();
	loadAttacks("ad1.dat", false);
	loadAttacks("CustomAttacks.cfg", true);
	critterList.clear();
	loadCritters("cd1.dat", false);
	loadCritters("CustomCritters.cfg", true);
	if (!mMagixUtils.XOR7FileGen("cd2.dat", "cd2.cfg", true, true))
		throw(Exception(9, "Corrupted Data File", "cd2.dat, please run the autopatcher."));
	else _unlink("cd2.cfg");
}
void MagixExternalDefinitions::initializeCapabilities(const RenderSystemCapabilities* capabilities)
{
	hasVertexProgram = capabilities->hasCapability(RSC_VERTEX_PROGRAM);
	hasFragmentProgram = capabilities->hasCapability(RSC_FRAGMENT_PROGRAM);
}

// THIS might be migrated : noted as an asset funct 
// is unit the player ? then there are major changes to do
// NB : IS THIS CHAR MANAGER BULLSHIT ? we need to walk that code
void MagixExternalDefinitions::loadUnitMeshes(const String& filename)
{
	maxHeads = GMAXHEADS;
	for (int i = 1; i <= maxHeads; i++)
	{
		headMesh.push_back("Head" + StringConverter::toString(i));
	}

	maxManes = GMAXMANES;
	maneMesh.push_back("Maneless");
	for (int i = 2; i <= maxManes; i++)
	{
		maneMesh.push_back("Mane" + StringConverter::toString(i));
	}

	maxTails = GMAXTAILS;
	for (int i = 1; i <= maxTails; i++)
	{
		tailMesh.push_back("Tail" + StringConverter::toString(i));
	}

	maxWings = GMAXWINGS;
	wingMesh.push_back("Wingless");
	wingMesh.push_back("Wings");
	for (int i = 2; i < maxWings; i++)
	{
		wingMesh.push_back("Wings" + StringConverter::toString(i));
	}

	maxTufts = GMAXTUFTS;
	tuftMesh.push_back("Tuftless");
	for (int i = 1; i < maxTufts; i++)
	{
		tuftMesh.push_back("Tuft" + StringConverter::toString(i));
	}

	maxBodyMarks = GMAXBODYMARKS;
	maxHeadMarks = GMAXHEADMARKS;
	maxTailMarks = GMAXTAILMARKS;
}