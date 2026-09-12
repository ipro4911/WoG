#ifndef __GameDefines_h_
#define __GameDefines_h_

// 1 to enable
// 0 to disable

// PSEUDO CIPHERING VARS
// Enable encrypted items.cfg
// 0 = Reads items.cfg
// 1 = Reads items.dat (like ad1.dat)
#define ENCRYPTED_ITEMS 0
// Enable encrypted zips and your custom file extension (instead of zip - Remember the .)
#define ENCRYPTED_PACKAGES 1
#define ENCRYPTION_EXTENSION ".kzip"
#define ENCRYPTION_XORS { 11, 22, 33, 144, 57, 63, 69, 96, 240, 128 }
//Change the above numbers 1   2   3    4   5   6   7   8    9   10

// BIG FAT WARNING:
// DO NOT CHANGE MAX_EQUIP ON A SERVER THAT IS ALREADY RUNNING
#define MAX_EQUIP 9 // 9 is max recommended
#define MAX_STASH 250
#define MAX_PARTYMEMBERS 6

// BIG FAT WARNING:
// DO NOT DECREASE THESE VALUES ONCE SET. WILL LEAD TO CRASHES
// Read the names of these defines and they'll make sense (Game Max ____)
#define GMAXHEADS 7
#define GMAXMANES 27
#define GMAXTAILS 7
#define GMAXWINGS 4
#define GMAXTUFTS 12
#define GMAXBODYMARKS 21
#define GMAXHEADMARKS 18
#define GMAXTAILMARKS 9

// CHAT COLORS:
#define CHAT_LOCAL_COLOUR_TOP "1 1 1"
#define CHAT_LOCAL_COLOUR_BOTTOM "1 1 1"
#define CHAT_GENERAL_COLOUR_TOP "1 1 1"
#define CHAT_GENERAL_COLOUR_BOTTOM "1 1 1"
#define CHAT_PARTY_COLOUR_TOP "1 1 1"
#define CHAT_PARTY_COLOUR_BOTTOM "1 1 1"

// STANDARD TEXTS
#define USER_ADMIN_TEXT "ADMIN"
#define USER_MOD_TEXT "MOD"
#define USER_VIP_TEXT "VIP"
#define USER_DONATOR_TEXT "[VIP]"

// HOME/DEFAULT SETTINGS
#define MAP_DEFAULT "Default"
#define MAP_SPAWN 2500,2500 // x,z spawn location

// OVERALL LIGHTING
#define AMBIENT_LIGHT_OUTDOOR 0.75, 0.75, 0.75
#define AMBIENT_LIGHT_INDOOR 0.5, 0.5, 0.5

// Uncomment the next line and change the key
// XOR key - obfuscated form of: 7*(i%7+1)  (identical values for each i)
#define XORKEY (((i%7)*6+7)+(i%7))
#define ROTKEY "MyPrivateKey"
#define CUSTOMCRITTERS 1 // allows users custom critters
#define MAXDIMS 100 // Maximum number of dims

#define SERVER_PORT 40000
#define MAIN_SERVER_PORT 40010
// Port for the login server's HTTP account-registration endpoint (launcher Register button).
// This is a small embedded HTTP listener, NOT MySQL. Set 0 to disable it.
#define REGISTER_PORT 40020

// Folder (relative to the login server's working directory) containing the client
// files that the launcher's updater serves to players via the embedded HTTP listener.
#define UPDATE_FILES_DIR "..\\client"

#define MAIN_SERVER_IP "26.114.82.92" // enter your server address

// Server login key - stored encrypted (XOR 0x5A), decrypted at runtime.
// Plaintext is NOT visible in this file. Client and server decode the same
// constant, so login keeps working. Change by re-encoding a new key.
static const unsigned char KITF_ENC_PASS[] = { 0x11,0x13,0x0E,0x1C,0x77,0x6A,0x74,0x6B,0x74,0x68,0x5A };
static inline const char* KITF_GetServerPassword()
{
	static char sBuf[sizeof(KITF_ENC_PASS)];
	static bool sInit = false;
	if (!sInit)
	{
		for (int i = 0; i < (int)sizeof(KITF_ENC_PASS); ++i)
			sBuf[i] = (char)(KITF_ENC_PASS[i] ^ 0x5A);
		sInit = true;
	}
	return sBuf;
}
#define SERVER_PASSWORD KITF_GetServerPassword()

// Game version fallback - NOT the authority anymore. The updater owns the
// version: manifest_build stamps version.txt (synced to clients via the
// update manifest), the client reports version.txt at connect and the game
// server compares against the served copy. GAME_VERSION is only used when
// version.txt is absent (dev runs outside the update flow).
#define GAME_VERSION "0.1.5"

#endif