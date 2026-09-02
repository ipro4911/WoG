/*
-----------------------------------------------------------------------------
Filename:    ogremagix.cpp
-----------------------------------------------------------------------------
This is one of the core file for KITF.
(c) KITF derivated work from Impressive Title by KOLVULKD

KITF is public domain, see readme file.
*/

#include "ogremagix.h"

#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#endif

// kind of dumb but let's allow it .. for now
#ifdef __cplusplus
	extern "C" {
#endif

#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
		// conformant winmain.
		INT WINAPI WinMain(_In_ HINSTANCE hInst, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR strCmdLine, _In_ INT nCmdShow)
#else
		// linux or tool mode ?
		int main(int argc, char *argv[])
#endif
		{
			// Create application object
			ogremagixApp theGame;

			try
			{
				// the game should run
				theGame.go();
			}
			catch(Ogre::Exception& e)
			{
				// we should remove OGRE_PLATFORM dependency if it does not have an x64 or so later on.
#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
				MessageBox( NULL, e.getFullDescription().c_str(), "An exception has occured!", MB_OK | MB_ICONERROR | MB_TASKMODAL);
#else
				std::cerr << "An exception has occured: " << 
					e.getFullDescription().c_str() << std::endl;
#endif
			}

			return 0;
		}

#ifdef __cplusplus
	}

#endif
