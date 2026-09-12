#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>
#endif
#include "GameConfig.h"
#include "RankDefs.h"
#include "MessageIdentifiers.h"
#include "RakNetworkFactory.h"
#include "RakPeerInterface.h"
#include "RakNetStatistics.h"
#include "RakNetTypes.h"
#include "BitStream.h"
#include "RakSleep.h"
//#include "OgreString.h"
#include "StringCompressor.h"
#include <assert.h>
#include <cstdio>
#include <cstring>
#include <stdlib.h>
#include <sstream>
#ifdef _WIN32
#include <conio.h>
#endif
#include <vector>
#include <iostream>
#include <utility>
#include <string>
#include <fstream>
#include <time.h>

// Get current date/time, format is YYYY-MM-DD.HH:mm:ss
const std::string currentDateTime() {
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    // Visit http://www.cplusplus.com/reference/clibrary/ctime/strftime/
    // for more information about date/time format
    strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

    return buf;
}


#if defined(_CONSOLE_2)
#include "Console2SampleIncludes.h"
#endif

unsigned char GetPacketIdentifier(Packet *p);

#ifdef _CONSOLE_2
_CONSOLE_2_SetSystemProcessParams
#endif

#include "MagixNetworkDefines.h"

using namespace std;

// --------------------------------------------------------------------------
// Secure account passwords: salted SHA-256 via a compact self-contained
// implementation. The .user password line for secured accounts is:
//   HASH$<salt-hex>$<sha256-hex(salt+password)>
// This is a one-way hash (not reversible like XOR7), so the stored password
// cannot be recovered from the file.
// --------------------------------------------------------------------------

static inline unsigned int KITF_ROTR32(unsigned int x, int n){return (x>>n)|(x<<(32-n));}
static string KITF_SHA256(const string& in){
	unsigned int h0=0x6a09e667,h1=0xbb67ae85,h2=0x3c6ef372,h3=0xa54ff53a,
	             h4=0x510e527f,h5=0x9b05688c,h6=0x1f83d9ab,h7=0x5be0cd19;
	static const unsigned int k[64]={
	0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
	0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
	0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
	0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
	0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
	0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
	0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
	0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
	vector<unsigned char> msg(in.begin(),in.end());
	unsigned long long bitlen=(unsigned long long)in.size()*8;
	msg.push_back(0x80);
	while((msg.size()%64)!=56)msg.push_back(0);
	for(int i=0;i<8;i++)msg.push_back((unsigned char)((bitlen>>(8*i))&0xff));
	for(size_t i=0;i<msg.size();i+=64){
		unsigned int w[64];
		for(int j=0;j<16;j++)
			w[j]=((unsigned int)msg[i+j*4]<<24)|((unsigned int)msg[i+j*4+1]<<16)|
			     ((unsigned int)msg[i+j*4+2]<<8)|((unsigned int)msg[i+j*4+3]);
		for(int j=16;j<64;j++){
			unsigned int s0=KITF_ROTR32(w[j-15],7)^KITF_ROTR32(w[j-15],18)^(w[j-15]>>3);
			unsigned int s1=KITF_ROTR32(w[j-2],17)^KITF_ROTR32(w[j-2],19)^(w[j-2]>>10);
			w[j]=w[j-16]+s0+w[j-7]+s1;
		}
		unsigned int a=h0,b=h1,c=h2,d=h3,e=h4,f=h5,g=h6,h=h7;
		for(int j=0;j<64;j++){
			unsigned int S1=KITF_ROTR32(e,6)^KITF_ROTR32(e,11)^KITF_ROTR32(e,25);
			unsigned int ch=(e&f)^((~e)&g);
			unsigned int t1=h+S1+ch+k[j]+w[j];
			unsigned int S0=KITF_ROTR32(a,2)^KITF_ROTR32(a,13)^KITF_ROTR32(a,22);
			unsigned int maj=(a&b)^(a&c)^(b&c);
			unsigned int t2=S0+maj;
			h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
		}
		h0+=a;h1+=b;h2+=c;h3+=d;h4+=e;h5+=f;h6+=g;h7+=h;
	}
	char out[65];static const char* hex="0123456789abcdef";
	unsigned int r[8]={h0,h1,h2,h3,h4,h5,h6,h7};
	int p=0;
	for(int i=0;i<8;i++){for(int j=0;j<4;j++){unsigned char b=(unsigned char)((r[i]>>(24-8*j))&0xff);out[p++]=hex[b>>4];out[p++]=hex[b&0xf];}}
	out[64]=0;
	return string(out);
}

static string KITF_RandomSaltHex(int bytes){
	string out;
	out.reserve(bytes*2);
	unsigned int seed=(unsigned int)time(0)^(unsigned int)GetCurrentProcessId();
	srand(seed);
	static const char* hex="0123456789abcdef";
	for(int i=0;i<bytes;i++){
		unsigned char b=(unsigned char)(rand()&0xff);
		out+=hex[b>>4];out+=hex[b&0xf];
	}
	return out;
}

struct PlayerToken
{
	SystemAddress add;
	unsigned char serverID;
	string name;
	unsigned short charID;
	string item[MAX_EQUIP];
	pair<unsigned short,unsigned short> hp;
	vector<pair<string,unsigned char> > skill;
	string pet;
	PlayerToken()
	{
		add = UNASSIGNED_SYSTEM_ADDRESS;
		serverID = MAX_SERVERS;
		name = "";
		charID = 0;
		for(int i=0;i<MAX_EQUIP;i++)item[i] = "";
		hp.first = 0;
		hp.second = 0;
		skill.clear();
		pet = "";
	}
};
struct BanInfo
{
	string name;
	int year;
	int yDay;
	vector<string> IPList;
	BanInfo(const string &n, const int &y, const int &yD)
	{
		name = n;
		year = y;
		yDay = yD;
		IPList.clear();
	}
};

class ServerManager
{
protected:
	RakPeerInterface *server;
	RakNetStatistics *rss;
	unsigned short numClients;
	unsigned short numServers;
	bool showTraffic;
	bool showCommands;
	time_t lastRankSweep;
	SystemAddress serverAdd[MAX_SERVERS];
	SystemAddress serverTunnelAdd[MAX_SERVERS];
	bool serverFull[MAX_SERVERS];
	PlayerToken playerToken[MAX_CLIENTS];
	vector<pair<string,SystemAddress> > loginSession;
	//vector<pair<string,SystemAddress> > loginSession;
	float dayTime;	
	float weatherTime;
	float maintenanceTime;
	time_t prevTime;
	unsigned short lowPing;
	unsigned short highPing;
	vector<BanInfo> banlist;

#if defined(_WIN32)
	SOCKET httpListenSock;
	HANDLE httpThreadH;
#endif
	void startHttpRegister();
	static unsigned __stdcall httpThread(void *arg);
	void handleHttpConnection(SOCKET client);
	bool writeSecureUser(const string &username, const string &password, const string &email);
	// Account-rank storage (Data/<letter>/<user>.user line 5: "rank=<name>",
	// XOR7-encoded like the other lines). Missing line = normal user.
	// (Defined out-of-class below, same pattern as writeSecureUser.)
	static int rankLineValue(const string &line);
	static bool isRankExpLine(const string &line);
	static time_t rankExpValue(const string &line);
	string resolveUserFile(const char *username);
	int accountRankStatus(const char *username, bool &hasRankLine);
	int accountRank(const char *username);
	bool setAccountRank(const char *username, int rank);
	time_t accountRankExpiry(const char *username);
	bool setAccountRankExp(const char *username, int rank, time_t exp);
	int effectiveAccountRank(const char *username);
	void sweepExpiredRanks();
	string sessionUserFor(const SystemAddress &addr);
	SystemAddress sessionAddressForName(const string &lowerName);
	void pushRankToSession(const string &lowerName, int rank);

public:
	ServerManager()
	{
		server=RakNetworkFactory::GetRakPeerInterface();
		server->SetIncomingPassword(SERVER_PASSWORD, (int)strlen(SERVER_PASSWORD));

		numClients = 0;
		numServers = 0;
		showTraffic = false;
		showCommands = true;
		lastRankSweep = 0;
		for(int i=0;i<MAX_SERVERS;i++)
		{
			serverAdd[i] = UNASSIGNED_SYSTEM_ADDRESS;
			serverTunnelAdd[i] = UNASSIGNED_SYSTEM_ADDRESS;
			serverFull[i] = false;
		}
		for(int i=0;i<MAX_CLIENTS;i++)
		{
			playerToken[i] = PlayerToken();
		}
		banlist.clear();
		loginSession.clear();
#if defined(_WIN32)
		httpListenSock = INVALID_SOCKET;
		httpThreadH = NULL;
#endif
	}
	~ServerManager()
	{
		RakNetworkFactory::DestroyRakPeerInterface(server);
	}
	bool initialize()
	{
		puts("Starting server");
		SocketDescriptor socketDescriptor(MAIN_SERVER_PORT,0);
		bool b = server->Startup(MAX_CLIENTS, 30, &socketDescriptor, 1);
		server->SetMaximumIncomingConnections(MAX_CLIENTS);
		if (b)
			puts("Server started, waiting for connections.");
		else
		{
			puts("Server failed to start.  Terminating.");
			return false;
		}
		server->SetOccasionalPing(true);
		printf("Max players allowed: %i\n", MAX_CLIENTS);

		dayTime = 500;
		weatherTime = 0;
		maintenanceTime = 0;
		time(&prevTime);
		loadPingRange();
		loadBanlist();

		startHttpRegister();

		return true;
	}

	void runLoop()
	{
		//char message[512]="";
		string message="";

		// Loop for input
		while (1)
		{

			// This sleep keeps RakNet responsive
			RakSleep(30);

			updateTimer();
			updateServers();
			sweepExpiredRanks();

	#ifdef _WIN32
			if (_kbhit())
			{
				// Notice what is not here: something to keep our network running.  It's
				// fine to block on gets or anything we want
				// Because the network engine was painstakingly written using threads.
				char input[512]="";
				gets_s(input);
				message = input;

				if (message=="quit")
				{
					puts("Quitting.");
					break;
				}

				if (message=="stat")
				{
					char temp[2048]="";
					rss = server->GetStatistics(server->GetSystemAddressFromIndex(0));
					StatisticsToString(rss, temp, 2);
					printf("%s", temp);
					printf("Ping %i\n", server->GetAveragePing(server->GetSystemAddressFromIndex(0)));

					continue;
				}

				if (message=="ban")
				{
					printf("Enter IP to ban.  You can use * as a wildcard\n");
					gets_s(input);
					server->AddToBanList(input);
					printf("IP %s added to ban list.\n", input);

					continue;
				}
				// ban username (with optional duration in days):  banname <user> [days]
				{
					const string cmdBan = "banname ";
					if(message.rfind(cmdBan,0)==0)
					{
						const vector<string> tPart = tokenize(message.substr(cmdBan.size())," \t\r\n");
						if(tPart.empty()){printf("Usage: banname <user> [days]\n");continue;}
						string tName = tPart[0];
						int tDays = 36500; // default ~100 years (effectively permanent)
						if(tPart.size()>1)tDays = atoi(tPart[1].c_str());
						if(tDays<1)tDays=1;

						time_t rawtime; struct tm* timeinfo;
						time(&rawtime); timeinfo=localtime(&rawtime);
						int tYear=timeinfo->tm_year, tDay=timeinfo->tm_yday + tDays;
						while(tDay>365){tYear+=1;tDay-=365;}

						// replace any existing entry for the same username
						bool dup=false;
						for(vector<BanInfo>::iterator it=banlist.begin();it!=banlist.end();)
						{
							if(toLowerCase(it->name)==toLowerCase(tName)){
								if(dup)it=banlist.erase(it); // drop stale duplicate entries
								else {it->year=tYear;it->yDay=tDay;dup=true;it++;}
							}else it++;
						}
						if(!dup)banlist.push_back(BanInfo(tName,tYear,tDay));

						// immediately ban the IP(s) of anyone already online with this name
						const OwnerToken tToken=getTokenByName(tName);
						if(tToken>0)
						{
							banlist.back().IPList.push_back(playerToken[tToken-1].add.ToString(false));
							server->AddToBanList(playerToken[tToken-1].add.ToString(false));
						}
						updateBanlist();
						saveBanlist();
						printf("[BAN] '%s' banned for %d day(s). %s\n",tName.c_str(),tDays,tToken>0?"Online player's IP also banned.":"");
						printf("Reload ban-friendly servers with 'reloadbanlist' if needed.\n");
						continue;
					}
				}
				// unban username:  unban <user>
				{
					const string cmdUnban = "unban ";
					if(message.rfind(cmdUnban,0)==0)
					{
						const vector<string> tPart = tokenize(message.substr(cmdUnban.size())," \t\r\n");
						if(tPart.empty()){printf("Usage: unban <user>\n");continue;}
						string tName = tPart[0];
						bool removed=false;
						for(vector<BanInfo>::iterator it=banlist.begin();it!=banlist.end();)
						{
							if(toLowerCase(it->name)==toLowerCase(tName))
							{
								for(int i=0;i<(int)it->IPList.size();i++)
									server->RemoveFromBanList(it->IPList[i].c_str());
								it=banlist.erase(it);
								removed=true;
							}else it++;
						}
						if(removed){saveBanlist();printf("[BAN] '%s' unbanned.\n",tName.c_str());}
						else printf("[BAN] '%s' is not in the ban list.\n",tName.c_str());
						continue;
					}
				}
				// set account rank from the trusted server console:
				//   setadmin <user> <normal|vip|trialmod|mod|headmod|gm|headadmin|owner> [30d]
				// days (1-3650, vip only, typed like 30d) makes a timed grant
				// that auto-expires. Bootstrap: run this once for yourself to
				// create the first owner.
				{
					const string cmdSetAdmin = "setadmin ";
					if(message.rfind(cmdSetAdmin,0)==0)
					{
						const vector<string> tPart = tokenize(message.substr(cmdSetAdmin.size())," \t\r\n");
						if(tPart.size()<2){printf("Usage: setadmin <user> <rank> [30d]\nRanks: normal/vip/trialmod/mod/headmod/gm/headadmin/owner\n");continue;}
						const int tNewRank = KITF_RankFromName(tPart[1]);
						if(tNewRank<0){printf("Unknown rank '%s'.\n",tPart[1].c_str());continue;}
						int tDays = 0;
						if(tPart.size()>2)
						{
							string tDaysArg = tPart[2];
							if(!tDaysArg.empty() && (tDaysArg[tDaysArg.size()-1]=='d' || tDaysArg[tDaysArg.size()-1]=='D'))
								tDaysArg.erase(tDaysArg.size()-1,1);
							tDays = atoi(tDaysArg.c_str());
							if(tNewRank!=KITF_RANK_VIP){printf("Days only apply to vip grants.\n");continue;}
							if(tDays<1||tDays>3650){printf("Days must be 1-3650 (e.g. 30d).\n");continue;}
						}
						const int tCurRank = effectiveAccountRank(tPart[0].c_str());
						if(tCurRank<0){printf("[SETADMIN] No such account '%s'.\n",tPart[0].c_str());continue;}
						bool tOk = false;
						if(tDays>0)
						{
							time_t tNow;
							time(&tNow);
							tOk = setAccountRankExp(tPart[0].c_str(),tNewRank,tNow+(time_t)tDays*86400);
						}
						else tOk = setAccountRank(tPart[0].c_str(),tNewRank);
						if(tOk)
						{
							if(tDays>0)printf("[SETADMIN] Console set '%s' (%s) -> %s for %d day(s).\n",tPart[0].c_str(),KITF_RankName(tCurRank),KITF_RankName(tNewRank),tDays);
							else printf("[SETADMIN] Console set '%s' (%s) -> %s.\n",tPart[0].c_str(),KITF_RankName(tCurRank),KITF_RankName(tNewRank));
							ofstream slog("security.log",ios::app);
							if(slog.good())slog<<currentDateTime()<<" - SETADMIN - console set "<<tPart[0]<<" to "<<KITF_RankName(tNewRank)<<(tDays>0?string(" for ")+tPart[2]+" day(s)":"")<<"\n";
							slog.close();
							pushRankToSession(toLowerCase(tPart[0]),tNewRank);
						}
						else printf("[SETADMIN] Could not write account file for '%s'.\n",tPart[0].c_str());
						continue;
					}
				}
				// Live server event, seen by every online player at once:
				//   event <message>          banner announcement
				//   event quake [message]    banner + earthquake effect
				{
					const string cmdEvent = "event ";
					if(message.rfind(cmdEvent,0)==0)
					{
						string tRest = message.substr(cmdEvent.size());
						while(!tRest.empty() && (tRest[0]==' '||tRest[0]=='\t'))tRest.erase(0,1);
						unsigned char tEffect = 0;
						if(tRest=="quake"||tRest.rfind("quake ",0)==0)
						{
							tEffect = 1;
							tRest = (tRest.size()>5) ? tRest.substr(6) : "";
							while(!tRest.empty() && (tRest[0]==' '||tRest[0]=='\t'))tRest.erase(0,1);
							if(tRest.empty())tRest = "An earthquake shakes the world!";
						}
						if(tRest.empty()){printf("Usage: event <message> | event quake [message]\n");continue;}
						if(tRest.size()>255)tRest = tRest.substr(0,255);
						RakNet::BitStream tBitStream;
						tBitStream.Write(MessageID(ID_ANNOUNCE));
						tBitStream.Write(tEffect);
						stringCompressor->EncodeString(tRest.c_str(),256,&tBitStream);
						int tSent = 0;
						for(vector<pair<string,SystemAddress> >::const_iterator it = loginSession.begin(); it != loginSession.end(); it++)
						{
							server->Send(&tBitStream, HIGH_PRIORITY, RELIABLE, 1, it->second, false);
							tSent++;
						}
						printf("[EVENT] Announced to %d player(s)%s: %.160s\n",tSent,(tEffect==1?" + earthquake":""),tRest.c_str());
						continue;
					}
					else if(message=="event")
					{
						printf("Usage: event <message> | event quake [message]\n");
						continue;
					}
				}

				if (message=="traffic")
				{
					showTraffic = !showTraffic;
					continue;
				}
				if (message=="commands")
				{
					showCommands = !showCommands;
					printf("Command packet logging %s.\n",showCommands?"on":"off");
					continue;
				}
				if (message=="numclients")
				{
					printf("Current number of connected clients: %i\n",numClients);
					continue;
				}
				/*if (message=="numplayers")
				{
					unsigned int numPlayers = 0;
					for(int i=0; i<MAX_SERVERS; i++)numPlayers += (unsigned int)loggedOnUsernames[i].size();
					printf("Current number of online players: %i\n",numPlayers);
					continue;
				}*/
				if (message=="numservers")
				{
					printf("Current number of servers: %i\n",numServers);
					continue;
				}
				if (message=="servers")
				{
					for(int i=0;i<MAX_SERVERS;i++)
					{
						if(serverTunnelAdd[i]!=UNASSIGNED_SYSTEM_ADDRESS)
						{
							printf("Server %i: %s  ",i+1,serverAdd[i].ToString());
							printf("Tunnel IP %i: %s  ",i+1,serverTunnelAdd[i].ToString());
							printf("Ping: %i ",server->GetAveragePing(serverTunnelAdd[i]));
							printf("Clients: %i",(int)getNumClientsInServer(i));
							if(serverFull[i])printf("(ping overload)\n");
							else printf("\n");
						}
					}
					continue;
				}
				if (message=="update")
				{
					broadcastServerUpdate();
					continue;
				}
				if (message=="time")
				{
					printf("Day time: %i\nWeather time: %i\n",(int)dayTime,(int)weatherTime);
					continue;
				}
				if (message=="pingrange")
				{
					printf("Enter Ping range (low high): ");
					gets_s(input);
					const vector<string> tPart = tokenize(string(input)," \n");
					if(tPart.size()>0)lowPing = atoi(tPart[0].c_str());
					if(tPart.size()>1)highPing = atoi(tPart[1].c_str());
					printf("Servers accept connections at ping below %i\n",(int)lowPing);
					printf("Servers reject connections at ping above %i\n",(int)highPing);
					updateServers(true);
					continue;
				}
				if (message=="pingserver")
				{
					printf("Enter ServerID to Ping: ");
					gets_s(input);
					const string tInput = input;
					const int tID = atoi(tInput.c_str());
					if(tID>0 && tID<=MAX_SERVERS && serverTunnelAdd[tID-1]!=UNASSIGNED_SYSTEM_ADDRESS)
						printf("Server %i Ping: %i\n",tID,server->GetAveragePing(serverTunnelAdd[tID-1]));
					else
						printf("No such server: %i\n",tID);
					continue;
				}
				if (message=="reloadbanlist")
				{
					clearBanlist();
					loadBanlist();
					continue;
				}
			}
	#endif

			// Get a packet from either the server or the client

			Packet *p = server->Receive();

			/*if (p==0)
				continue; // Didn't get any packets*/
			while(p)
			{
				updateTimer();
				updateServers();

			// We got a packet, get the identifier with our handy function
			const unsigned char packetIdentifier = GetPacketIdentifier(p);

			// Check if this is a network message packet
			switch (packetIdentifier)
			{
				case ID_NEW_INCOMING_CONNECTION:
					{
						printf("ID_NEW_INCOMING_CONNECTION from %s\n", p->systemAddress.ToString());
						//sendServerList(p->systemAddress);
					}
					break;

				case ID_DISCONNECTION_NOTIFICATION:
					{
						printf("ID_DISCONNECTION_NOTIFICATION from %s\n", p->systemAddress.ToString());
						unregisterServer(p);
						endSession(p);
					}
					break;

				case ID_CONNECTION_LOST:
					{
						printf("ID_CONNECTION_LOST from %s\n", p->systemAddress.ToString());
						unregisterServer(p);
						endSession(p);
					}
					break;

				case ID_TOKENCONNECTED:
					{
						// Only registered game servers may request player tokens.
						// (Clients broadcast this packet to all connections, so a
						// direct client copy would otherwise mint a garbage token
						// and a garbage sky-time reply on the client.)
						if(getServerID(p->systemAddress)==MAX_SERVERS)
						{
							if(showCommands)printf("[CMD] Ignored ID_TOKENCONNECTED from %s (game servers only).\n",p->systemAddress.ToString());
							break;
						}
						RakNet::BitStream tReceiveBit(p->data, p->length, false);
						MessageID tMessage;
						SystemAddress tAdd;
						char tUsername[16] = "";
						unsigned short tCharIndex = 0;

						tReceiveBit.Read(tMessage);
						tReceiveBit.Read(tAdd);
						stringCompressor->DecodeString(tUsername,16,&tReceiveBit);
						tReceiveBit.Read(tCharIndex);

						const OwnerToken tToken = assignToken(tAdd,getServerID(p->systemAddress),tUsername,tCharIndex);

						if(tToken==0)
						{
							printf("Failed to assign token for %s!\n",tAdd.ToString());
							ofstream outFile("./exceptions.log",ios_base::app);
							if(outFile.good())
							{
								outFile << "Failed to assign token for " << tAdd.ToString() << endl;
							}
							outFile.close();
							break;
						}
						// The rank tail lets the game server enforce per-command
						// auth (kick/find/godspeak/event) without trusting the
						// client. Unknown accounts fail closed to normal.
						const int tRank = effectiveAccountRank(tUsername);
						RakNet::BitStream tBitStream;
						tBitStream.Write(MessageID(ID_TOKENCONNECTED));
						tBitStream.Write(tToken);
						tBitStream.Write(tAdd);
						tBitStream.Write((unsigned short)dayTime);
						tBitStream.Write((unsigned short)weatherTime);
						stringCompressor->EncodeString(KITF_RankCode(tRank<0 ? KITF_RANK_NORMAL : tRank),8,&tBitStream);

						server->Send(&tBitStream, HIGH_PRIORITY, RELIABLE, 0, p->systemAddress, false);
					}
					break;

				case ID_TOKENDISCONNECTED:
					{
						RakNet::BitStream tReceiveBit(p->data, p->length, false);
						MessageID tMessage;
						bool tIsToken;
						OwnerToken tToken;
						SystemAddress tAdd;
						tReceiveBit.Read(tMessage);
						tReceiveBit.Read(tIsToken);
						if(tIsToken)
						{
							tReceiveBit.Read(tToken);
							unassignToken(tToken);
						}
						else
						{
							tReceiveBit.Read(tAdd);
							unassignToken(tAdd);
						}
					}
					break;
				case ID_MODIFIED_PACKET:
					{
						printf("ID_MODIFIED_PACKET from %s\n", p->systemAddress.ToString());
					}
					break;

				case ID_LOGON:
					{
						printf("ID_LOGON from %s\n", p->systemAddress.ToString());

						RakNet::BitStream tReceiveBit(p->data, p->length, false);
						MessageID tMessage;
						char tUsername[16] = "";
						char tPassword[16] = "";
						tReceiveBit.Read(tMessage);
						stringCompressor->DecodeString(tUsername,16,&tReceiveBit);
						stringCompressor->DecodeString(tPassword,16,&tReceiveBit);

						bool tLogonSuccess = false;
						if(strlen(tUsername)>0 && strlen(tPassword)>0)
						{
							bool tRenameFile = false, tEncryptPass = false, tFileExists = false, tResaveFile = false, tReEncryptPass = false;
							char tPassword2[16] = "";
							char tEmail[64] = "";
							char tQuestion[128] = "";
							char tAnswer[128] = "";
							std::ifstream inFile(getFilename(tUsername,".user").c_str(),std::ios::binary);
							if(!inFile.good())
							{
								inFile.clear();
								inFile.open(getFilename(tUsername,".user",false).c_str(),std::ios::binary);
								if(inFile.good())tRenameFile = true;
							}
							if(inFile.good())
							{
								tFileExists = true;
								inFile.getline(tPassword2,16);
								const string tPassStr = tPassword;
								const string tPassStr2 = XOR7(tPassword2);
								if(tPassStr==tPassStr2)tLogonSuccess = true;
							}
							inFile.close();
							if(tRenameFile)rename(getFilename(tUsername,".user",false).c_str(),getFilename(tUsername,".user").c_str());
							//Read file as non binary and try again
							if(tFileExists && !tLogonSuccess)
							{
								inFile.clear();
								std::ifstream inFile(getFilename(tUsername,".user").c_str());
								if(inFile.good())
								{
									inFile.getline(tPassword2,16);
									const string tPassStr = tPassword;
									const string tPassStr2 = XOR7(tPassword2);
									if(tPassStr==tPassStr2)
									{
										tLogonSuccess = true;
										tResaveFile = true;
										inFile.getline(tEmail,64);
										inFile.getline(tQuestion,128);
										inFile.getline(tAnswer,128);
									}
									else if(tPassStr==string(tPassword2))
									{
										tLogonSuccess = true;
										tResaveFile = true;
										tEncryptPass = true;
										inFile.getline(tEmail,64);
										inFile.getline(tQuestion,128);
										inFile.getline(tAnswer,128);
									}
									else if(tPassStr==XOR7OLD(tPassword2))
									{
										tLogonSuccess = true;
										tResaveFile = true;
										tReEncryptPass = true;
										inFile.getline(tEmail,64);
										inFile.getline(tQuestion,128);
										inFile.getline(tAnswer,128);
									}
								}
								inFile.close();
							}
							//Read file as OLDXOR7 and try again
							if(tFileExists && !tLogonSuccess)
							{
								inFile.clear();
								std::ifstream inFile(getFilename(tUsername,".user").c_str(),std::ios::binary);
								if(inFile.good())
								{
									inFile.getline(tPassword2,16);
									const string tPassStr = tPassword;
									const string tPassStr2 = XOR7OLD(tPassword2);
									if(tPassStr==tPassStr2)
									{
										tLogonSuccess = true;
										tResaveFile = true;
										tReEncryptPass = true;
										inFile.getline(tEmail,64);
										inFile.getline(tQuestion,128);
										inFile.getline(tAnswer,128);
									}
								}
								inFile.close();
							}
							//Secured accounts created via the HTTP register endpoint use a
							//salted SHA-256 hash ("HASH$<salt>#<digest>"). Verify against that.
							if(tFileExists && !tLogonSuccess)
							{
								inFile.clear();
								std::ifstream inFile(getFilename(tUsername,".user").c_str(),std::ios::binary);
								if(inFile.good())
								{
									string firstLine;
									std::getline(inFile,firstLine);
									if(firstLine.rfind("HASH$",0)==0){
										size_t sep=firstLine.find('$',5);
										string salt=sep==string::npos?"":firstLine.substr(5,sep-5);
										string digest=sep==string::npos?"":firstLine.substr(sep+1);
										if(KITF_SHA256(salt+string(tPassword))==digest)tLogonSuccess=true;
									}
								}
								inFile.close();
							}
							if(tResaveFile)
							{
								std::ofstream outFile(getFilename(tUsername,".user").c_str(),std::ios::binary);
								if(outFile.good())
								{
									string tBuffer = "";
									if(tEncryptPass)tBuffer = XOR7(tPassword2);
									else if(tReEncryptPass)
									{
										const string tOrigPass = XOR7OLD(tPassword2);
										tBuffer = XOR7(tOrigPass);
									}
									else tBuffer = string(tPassword2);
									tBuffer += "\n";
									outFile.write(tBuffer.c_str(),tBuffer.length());
									tBuffer = XOR7(tEmail);
									tBuffer += "\n";
									outFile.write(tBuffer.c_str(),tBuffer.length());
									tBuffer = XOR7(tQuestion);
									tBuffer += "\n";
									outFile.write(tBuffer.c_str(),tBuffer.length());
									tBuffer = XOR7(tAnswer);
									tBuffer += "\n";
									outFile.write(tBuffer.c_str(),tBuffer.length());
								}
								outFile.close();
							}
						}
						short tCount = 0;
						bool tDoInstantBoot = false;
						if(tLogonSuccess)
						{
							//Check if username is in banlist
							const string tUsernameStr = toLowerCase(string(tUsername));
							for(int i=0;i<(int)banlist.size();i++)
							{
								//Ban this IP
								if(toLowerCase(banlist[i].name)==tUsernameStr)
								{
									const string tIP = p->systemAddress.ToString(false);
									banlist[i].IPList.push_back(tIP);
									server->AddToBanList(tIP.c_str());
									tLogonSuccess = false;
									printf("[BAN] Login rejected for banned user '%s' from %s (IP added to ban list).\n",tUsername,p->systemAddress.ToString());
									break;
								}
							}
							{
								bool tRenameFile = false;
								std::ifstream inFile(getFilename(tUsername,".charlist").c_str());
								if(!inFile.good())
								{
									inFile.clear();
									inFile.open(getFilename(tUsername,".charlist",false).c_str());
									if(inFile.good())tRenameFile = true;
								}
								if(inFile.good())
								{
									while(!inFile.eof())
									{
										char tBuffer[32] = "";
										inFile.getline(tBuffer,32);
										if(strlen(tBuffer)>0)tCount++;
									}
								}
								inFile.close();
								if(tRenameFile)rename(getFilename(tUsername,".charlist",false).c_str(),getFilename(tUsername,".charlist").c_str());
							}
							//add to logfile
							std::ofstream outFile("security.log",ios::app);
							if(outFile.good())
							{
								outFile << currentDateTime() << " - LOGON - " << p->systemAddress.ToString() << "," << tUsername << endl;
							}
							outFile.close();

							//Save logged in data
							for(int i=0;i<MAX_CLIENTS;i++)
							{
								if(toLowerCase(playerToken[i].name)==tUsernameStr)
								{
									savePlayerData(i+1);
									tDoInstantBoot = true;
									break;
								}
							}

							//Restrict one login per username
							for(vector<pair<string,SystemAddress> >::const_iterator it = loginSession.begin(); it != loginSession.end(); it++)
							{
								pair<string,SystemAddress> tSession = *it;
								if(tSession.first==tUsernameStr)
								{
									server->CloseConnection(tSession.second,true);
									break;
								}
							}
							loginSession.push_back(pair<string,SystemAddress>(tUsernameStr,p->systemAddress));

							//Notify servers to boot logged in username
							for(int i=0; i<MAX_SERVERS; i++)
							{
								if(serverTunnelAdd[i]!=UNASSIGNED_SYSTEM_ADDRESS)
								{
									RakNet::BitStream tBitStream;

									tBitStream.Write(MessageID(ID_FORCELOGOUT));
									tBitStream.Write(true);
									stringCompressor->EncodeString(tUsernameStr.c_str(),16,&tBitStream);

									server->Send(&tBitStream, HIGH_PRIORITY, RELIABLE, 0, serverTunnelAdd[i], false);
								}
							}
						}
						RakNet::BitStream tBitStream;

						tBitStream.Write(MessageID(ID_LOGON));
						tBitStream.Write(tLogonSuccess);
						if(tLogonSuccess)
						{
							tBitStream.Write(tCount);
						}

						server->Send(&tBitStream, HIGH_PRIORITY, RELIABLE, 1, p->systemAddress, false);

						if(tLogonSuccess)sendServerList(p->systemAddress,true);

						if(showTraffic)
							if(tLogonSuccess)printf("%s logon success.\n",tUsername);
							else printf("%s logon failed.\n",tUsername);

						// Always log login attempts to the console (register/login/ban
						// events are typed out live so the operator can monitor them).
					if(tLogonSuccess)printf("[LOGIN] '%s' logged in from %s (characters=%d).\n",tUsername,p->systemAddress.ToString(),tCount);
					else printf("[LOGIN] Login FAILED for '%s' from %s (pwlen=%d).\n",tUsername,p->systemAddress.ToString(),(int)strlen(tPassword));
					}
					break;

				case ID_CREATEACCOUNT:
					{
						printf("ID_CREATEACCOUNT from %s\n", p->systemAddress.ToString());

						RakNet::BitStream tReceiveBit(p->data, p->length, false);
						MessageID tMessage;
						char tUsername[16] = "";
						char tPassword[16] = "";
						char tEmail[64] = "";
						char tQuestion[128] = "";
						char tAnswer[128] = "";
						tReceiveBit.Read(tMessage);
						stringCompressor->DecodeString(tUsername,16,&tReceiveBit);
						stringCompressor->DecodeString(tPassword,16,&tReceiveBit);
						stringCompressor->DecodeString(tEmail,64,&tReceiveBit);
						stringCompressor->DecodeString(tQuestion,128,&tReceiveBit);
						stringCompressor->DecodeString(tAnswer,128,&tReceiveBit);
						bool tCreateSuccess = false;
						if(strlen(tUsername)>0 && strlen(tPassword)>0)
						{
							//Check if account (with underscores) exists
							std::ifstream inFile(getFilename(tUsername,".user").c_str());
							if(!inFile.good())
							{
								inFile.clear();
								//Check if account (with spaces) exists
								inFile.open(getFilename(tUsername,".user",false).c_str());
								if(!inFile.good())
								{
									inFile.clear();
									//Check if account (underscores replaced with spaces) exists
									inFile.open(getFilename(tUsername,".user",false,true).c_str());
									if(!inFile.good())
									{
										tCreateSuccess = true;
									}
								}
							}
							inFile.close();
							if(tCreateSuccess)
							{
								std::ofstream outFile(getFilename(tUsername,".user").c_str(),std::ios::binary);
								if(outFile.good())
								{
								string tBuffer = XOR7(tPassword);
								tBuffer += "\n";
								outFile.write(tBuffer.c_str(),tBuffer.length());
								tBuffer = XOR7(tEmail);
								tBuffer += "\n";
								outFile.write(tBuffer.c_str(),tBuffer.length());
								tBuffer = XOR7(tQuestion);
								tBuffer += "\n";
								outFile.write(tBuffer.c_str(),tBuffer.length());
								tBuffer = XOR7(tAnswer);
								tBuffer += "\n";
								outFile.write(tBuffer.c_str(),tBuffer.length());
								// New accounts are always normal users; promotion is via /setadmin only.
								tBuffer = XOR7(string("rank=normal"));
								tBuffer += "\n";
								outFile.write(tBuffer.c_str(),tBuffer.length());
								}
								else tCreateSuccess = false;
								outFile.close();
							}
						}
						RakNet::BitStream tBitStream;

						tBitStream.Write(MessageID(ID_CREATEACCOUNT));
						tBitStream.Write(tCreateSuccess);

						server->Send(&tBitStream, HIGH_PRIORITY, RELIABLE, 1, p->systemAddress, false);

						if(showTraffic)
							if(tCreateSuccess)printf("%s create account success.\n",tUsername);
							else printf("%s create account failed.\n",tUsername);
					}
					break;

				case ID_EDITACCOUNT:
					{
						RakNet::BitStream tReceiveBit(p->data, p->length, false);
						MessageID tMessage;
						char tUsername[16] = "";
						char tPassword[16] = "";
						char tNewPassword[16] = "";
						tReceiveBit.Read(tMessage);
						stringCompressor->DecodeString(tUsername,16,&tReceiveBit);
						stringCompressor->DecodeString(tPassword,16,&tReceiveBit);
						stringCompressor->DecodeString(tNewPassword,16,&tReceiveBit);

						bool tSuccess = false;
						char tEmail[64] = "";
						char tQuestion[128] = "";
						char tAnswer[128] = "";
						if(strlen(tUsername)>0 && strlen(tPassword)>0)
						{
							bool renameFile = false;
							std::ifstream inFile(getFilename(tUsername,".user").c_str(),std::ios::binary);
							if(!inFile.good())
							{
								inFile.clear();
								inFile.open(getFilename(tUsername,".user",false).c_str(),std::ios::binary);
								if(inFile.good())renameFile = true;
							}
							if(inFile.good())
							{
								char tPassword2[32] = "";
								inFile.getline(tPassword2,32);
								const string tPassStr = tPassword;
								const string tPassStr2 = XOR7(tPassword2);
								if(tPassStr==tPassStr2 || tPassStr==string(tPassword2))
								{
									tSuccess = true;
									inFile.getline(tEmail,64);
									inFile.getline(tQuestion,128);
									inFile.getline(tAnswer,128);
								}
							}
							inFile.close();
							if(renameFile)rename(getFilename(tUsername,".user",false).c_str(),getFilename(tUsername,".user").c_str());
						}
						if(tSuccess)
						{
							std::ofstream outFile(getFilename(tUsername,".user").c_str(),std::ios::binary);
							if(outFile.good())
							{
								string tBuffer = XOR7(tNewPassword);
								tBuffer += "\n";
								outFile.write(tBuffer.c_str(),tBuffer.length());
								tBuffer = tEmail;
								tBuffer += "\n";
								outFile.write(tBuffer.c_str(),tBuffer.length());
								tBuffer = tQuestion;
								tBuffer += "\n";
								outFile.write(tBuffer.c_str(),tBuffer.length());
								tBuffer = tAnswer;
								tBuffer += "\n";
								outFile.write(tBuffer.c_str(),tBuffer.length());
							}
							else tSuccess = false;
							outFile.close();
						}

						RakNet::BitStream tBitStream;

						tBitStream.Write(MessageID(ID_EDITACCOUNT));
						tBitStream.Write(tSuccess);

						server->Send(&tBitStream, HIGH_PRIORITY, RELIABLE, 1, p->systemAddress, false);

						if(showTraffic)
							if(tSuccess)printf("%s edit account success.\n",tUsername);
							else printf("%s edit account failed.\n",tUsername);
					}
					break;

				case ID_LOADCHAR:
					{
						printf("ID_LOADCHAR from %s\n", p->systemAddress.ToString());

						RakNet::BitStream tReceiveBit(p->data, p->length, false);
						MessageID tMessage;
						char tUsername[16] = "";
						short tIndex = 0;
						tReceiveBit.Read(tMessage);
						stringCompressor->DecodeString(tUsername,16,&tReceiveBit);
						tReceiveBit.Read(tIndex);

						bool tLoadSuccess = false;
						char tName[32] = "";
						bool renameFile = false;
						std::ifstream inFile(getFilename(tUsername,".charlist").c_str());
						if(!inFile.good())
						{
							inFile.clear();
							inFile.open(getFilename(tUsername,".charlist",false).c_str());
							if(inFile.good())renameFile = true;
						}
						if(inFile.good())
						{
							short tCount = 0;
							while(!inFile.eof() && inFile.good())
							{
								inFile.getline(tName,32);
								if(tIndex==tCount)
								{
									tLoadSuccess = true;
									break;
								}
								tCount++;
							}
						}
						inFile.close();
						if(renameFile)rename(getFilename(tUsername,".charlist",false).c_str(),getFilename(tUsername,".charlist").c_str());

						RakNet::BitStream tBitStream;

						tBitStream.Write(MessageID(ID_LOADCHAR));
						tBitStream.Write(tIndex);
						tBitStream.Write(tLoadSuccess);

						if(tLoadSuccess)
						{
							string tFilename = tUsername;
							tFilename += "_";
							tFilename += tName;

							char tData[512] = "";
							char tAdminToken[16] = "";
							char tItem[MAX_EQUIP][16];
							for(int j=0;j<MAX_EQUIP;j++)strcpy(tItem[j],"");
							char tHP[16] = "";
							char tSkills[512] = "";
							char tPet[32] = "";

							//Load char data
							bool tRenameFile = false;
							inFile.open(getFilename(tFilename.c_str(),".character").c_str());
							if(!inFile.good())
							{
								inFile.clear();
								inFile.open(getFilename(tFilename.c_str(),".character",false).c_str());
								if(inFile.good())tRenameFile = true;
							}
							if(inFile.good())
							{
								inFile.getline(tData,512);
								inFile.getline(tAdminToken,16);
							}
							else tLoadSuccess = false;

							inFile.close();
							if(tRenameFile)rename(getFilename(tFilename.c_str(),".character",false).c_str(),getFilename(tFilename.c_str(),".character").c_str());

							//Load item, HP, skills and pet
							bool tRenameItemFile = false;
							inFile.open(getFilename(tFilename.c_str(),".item").c_str());
							if(!inFile.good())
							{
								inFile.clear();
								inFile.open(getFilename(tFilename.c_str(),".item",false).c_str());
								if(inFile.good())tRenameItemFile = true;
							}
							if(inFile.good())
							{
								for(int j=0;j<MAX_EQUIP;j++)
									if(inFile.good())inFile.getline(tItem[j],16);
								if(inFile.good())inFile.getline(tHP,16);
								if(inFile.good())inFile.getline(tSkills,512);
								if(inFile.good())inFile.getline(tPet,32);
							}
							inFile.close();
							if(tRenameItemFile)rename(getFilename(tFilename.c_str(),".item",false).c_str(),getFilename(tFilename.c_str(),".item").c_str());

						//Write player data
						// Rank is authoritative from the ACCOUNT file (never trust the
						// per-character token or anything the client claims).
						stringCompressor->EncodeString(tData,512,&tBitStream);
						bool tHasRankLine = false;
						int tRank = accountRankStatus(tUsername,tHasRankLine);
						if(tRank < 0)
						{
							// Account file vanished mid-session: fail safe to normal.
							tRank = KITF_RANK_NORMAL;
							tHasRankLine = true;
						}
						if(!tHasRankLine)
						{
							// One-time migration: pre-rank accounts whose character
							// file still carries a legacy "ok"/"mod" token keep
							// their powers as the equivalent account rank.
							int tLegacy = KITF_RANK_NORMAL;
							if(strlen(tAdminToken)>=3 && string(tAdminToken)=="mod") tLegacy = KITF_RANK_MOD;
							else if(strlen(tAdminToken)>=3 && string(tAdminToken).erase(2)=="ok") tLegacy = KITF_RANK_GM;
							if(tLegacy != KITF_RANK_NORMAL && setAccountRank(tUsername,tLegacy))
							{
								tRank = tLegacy;
								printf("[RANK] Migrated legacy char-admin '%s' to account rank %s.\n",tUsername,KITF_RankName(tRank));
							}
						}
						// Expired timed ranks (e.g. VIP) revert to normal here.
						tRank = effectiveAccountRank(tUsername);
						if(tRank < 0) tRank = KITF_RANK_NORMAL;
						const bool tIsAdmin = KITF_RankIsAdmin(tRank);
						const bool tIsMod = KITF_RankIsMod(tRank);
						tBitStream.Write(tIsAdmin);
						tBitStream.Write(tIsMod);
						stringCompressor->EncodeString(KITF_RankCode(tRank),8,&tBitStream);
						// Timed VIP notice data: days left (0 = permanent/no timer).
						// The client shows it automatically at login, no command needed.
						unsigned short tVipDaysLeft = 0;
						if(tRank == KITF_RANK_VIP)
						{
							const time_t tExp = accountRankExpiry(tUsername);
							if(tExp > 0)
							{
								time_t tNow;
								time(&tNow);
								if(tExp > tNow)
								{
									const long long tLeft = (long long)(tExp - tNow);
									tVipDaysLeft = (unsigned short)((tLeft + 86399) / 86400);
									if(tVipDaysLeft == 0) tVipDaysLeft = 1;
								}
							}
						}
						tBitStream.Write(tVipDaysLeft);
						if(tIsAdmin)stringCompressor->EncodeString("0 0 0",16,&tBitStream);

							//Write items
							for(int j=0;j<MAX_EQUIP;j++)
							{
								const bool tHasItem = (strlen(tItem[j])>0);
								tBitStream.Write(tHasItem);
								if(tHasItem)stringCompressor->EncodeString(tItem[j],16,&tBitStream);
							}

							//Write HP
							const bool tHasHP = (strlen(tHP)>0);
							tBitStream.Write(tHasHP);
							if(tHasHP)stringCompressor->EncodeString(tHP,16,&tBitStream);

							//Write skills
							const bool tHasSkills = (strlen(tSkills)>0);
							tBitStream.Write(tHasSkills);
							if(tHasSkills)stringCompressor->EncodeString(tSkills,512,&tBitStream);

							//Write Pet
							const bool tHasPet = (strlen(tPet)>0);
							tBitStream.Write(tHasPet);
							if(tHasPet)stringCompressor->EncodeString(tPet,32,&tBitStream);
						}

						server->Send(&tBitStream, HIGH_PRIORITY, RELIABLE_SEQUENCED, 1, p->systemAddress, false);

						if(showTraffic)
							if(tLoadSuccess){}//printf("%s load char success.\n",tUsername);
							else printf("%s load char failed.\n",tUsername);
					}
					break;

				case ID_CREATECHAR:
					{
						printf("ID_CREATECHAR from %s\n", p->systemAddress.ToString());

						RakNet::BitStream tReceiveBit(p->data, p->length, false);
						MessageID tMessage;
						char tUsername[16] = "";
						char tName[32] = "";
						char tData[512] = "";
						tReceiveBit.Read(tMessage);
						stringCompressor->DecodeString(tUsername,16,&tReceiveBit);
						stringCompressor->DecodeString(tName,32,&tReceiveBit);
						stringCompressor->DecodeString(tData,512,&tReceiveBit);

						bool tCreateSuccess = false;

						bool tNameUsed = true;
						while(tNameUsed)
						{
							tNameUsed = false;
							bool tRenameFile = false;
							std::ifstream inFile(getFilename(tUsername,".charlist").c_str());
							if(!inFile.good())
							{
								inFile.clear();
								inFile.open(getFilename(tUsername,".charlist",false).c_str());
								if(inFile.good())tRenameFile = true;
							}
							if(inFile.good())
							{
								while(!inFile.eof() && inFile.good())
								{
									char tBuffer[32] = "";
									inFile.getline(tBuffer,32);
									if(strcmp(tName,tBuffer)==0)
									{
										tNameUsed = true;
										strcat(tName,"2");
										break;
									}
								}
							}
							inFile.close();
							if(tRenameFile)rename(getFilename(tUsername,".charlist",false).c_str(),getFilename(tUsername,".charlist").c_str());
						}

						//Write charlist data
						std::ofstream outFile(getFilename(tUsername,".charlist").c_str(),std::ios::app);
						string tBuffer = tName;
						tBuffer += "\n";
						outFile.write(tBuffer.c_str(),tBuffer.length());
						outFile.close();

						string tFilename = tUsername;
						tFilename += "_";
						tFilename += tName;

						//Write char data
						outFile.open(getFilename(tFilename.c_str(),".character").c_str());
						tBuffer = tData;
						tBuffer += "\n";
						tBuffer += "false\n";
						outFile.write(tBuffer.c_str(),tBuffer.length());
						outFile.close();

						//Write item file
						outFile.open(getFilename(tFilename.c_str(),".item").c_str());
						outFile.close();

						//Confirm file creation success
						std::ifstream inFile(getFilename(tFilename.c_str(),".character").c_str());
						tCreateSuccess = inFile.good();
						inFile.close();

						RakNet::BitStream tBitStream;

						tBitStream.Write(MessageID(ID_CREATECHAR));
						tBitStream.Write(tCreateSuccess);

						server->Send(&tBitStream, HIGH_PRIORITY, RELIABLE, 1, p->systemAddress, false);

						if(showTraffic)
							if(tCreateSuccess)printf("%s create char success.\n",tUsername);
							else printf("%s create char failed.\n",tUsername);
					}
					break;

				case ID_DELETECHAR:
					{
						printf("ID_DELETECHAR from %s\n", p->systemAddress.ToString());

						RakNet::BitStream tReceiveBit(p->data, p->length, false);
						MessageID tMessage;
						char tUsername[16] = "";
						short tIndex = 0;
						tReceiveBit.Read(tMessage);
						stringCompressor->DecodeString(tUsername,16,&tReceiveBit);
						tReceiveBit.Read(tIndex);

						string tName = "";
						string tBuffer = "";
						bool tSuccess = false;
						std::ifstream inFile(getFilename(tUsername,".charlist").c_str());
						if(inFile.good())
						{
							short tCount = 0;
							while(!inFile.eof() && inFile.good())
							{
								char tLine[32] = "";
								inFile.getline(tLine,32);
								if(tIndex==tCount)
								{
									tName = tLine;
									tSuccess = true;
								}
								else if(strlen(tLine)>0)
								{
									tBuffer += tLine;
									tBuffer += "\n";
								}
								tCount++;
							}
						}
						inFile.close();

						if(tSuccess)
						{
							//Edit charlist
							std::ofstream outFile(getFilename(tUsername,".charlist").c_str());
							outFile.write(tBuffer.c_str(),tBuffer.length());
							outFile.close();

							string tFilename = tUsername;
							tFilename += "_";
							tFilename += tName;
							//Delete character file
							unlink(getFilename(tFilename.c_str(),".character").c_str());
							//Delete item file
							unlink(getFilename(tFilename.c_str(),".item").c_str());
						}

						if(showTraffic)
							if(tSuccess)printf("%s delete char success.\n",tUsername);
							else printf("%s delete char failed.\n",tUsername);
					}
					break;

				case ID_EDITCHAR:
					{
						printf("ID_EDITCHAR from %s\n", p->systemAddress.ToString());

						RakNet::BitStream tReceiveBit(p->data, p->length, false);
						MessageID tMessage;
						char tUsername[16] = "";
						short tIndex = 0;
						char tData[512] = "";
						tReceiveBit.Read(tMessage);
						stringCompressor->DecodeString(tUsername,16,&tReceiveBit);
						tReceiveBit.Read(tIndex);
						stringCompressor->DecodeString(tData,512,&tReceiveBit);

						string tName = "";
						bool tSuccess = false;
						std::ifstream inFile(getFilename(tUsername,".charlist").c_str());
						if(inFile.good())
						{
							short tCount = 0;
							while(!inFile.eof() && inFile.good())
							{
								char tLine[32] = "";
								inFile.getline(tLine,32);
								if(tIndex==tCount)
								{
									tName = tLine;
									tSuccess = true;
									break;
								}
								tCount++;
							}
						}
						inFile.close();

						if(tSuccess)
						{
							string tFilename = tUsername;
							tFilename += "_";
							tFilename += tName;

							string tBuffer = tData;
							tBuffer += "\n";

							//Read old character file
							std::ifstream inFile(getFilename(tFilename.c_str(),".character").c_str());
							if(inFile.good())
							{
								tSuccess = true;
								char tUnusedData[512];
								inFile.getline(tUnusedData,512);	//read char data
								char tLine[16] = "";
								inFile.getline(tLine,16);	//read admin data
								tBuffer += tLine;
								tBuffer += "\n";
							}
							inFile.close();

							//Write new character file
							if(tSuccess)
							{
								std::ofstream outFile(getFilename(tFilename.c_str(),".character").c_str());
								outFile.write(tBuffer.c_str(),tBuffer.length());
								outFile.close();

								//Confirm edit success
								inFile.open(getFilename(tFilename.c_str(),".character").c_str());
								tSuccess = inFile.good();
								inFile.close();
							}
						}

						RakNet::BitStream tBitStream;

						tBitStream.Write(MessageID(ID_EDITCHAR));
						tBitStream.Write(tSuccess);

						server->Send(&tBitStream, HIGH_PRIORITY, RELIABLE, 1, p->systemAddress, false);

						if(showTraffic)
							if(tSuccess)printf("%s edit char success.\n",tUsername);
							else printf("%s edit char failed.\n",tUsername);
					}
					break;

				/*
				case ID_PLAYERDATA:
					{
						RakNet::BitStream tReceiveBit(p->data, p->length, false);
						MessageID tMessage;
						bool tEnter;
						char tUsername[16] = "";
						tReceiveBit.Read(tMessage);
						tReceiveBit.Read(tEnter);
						stringCompressor->DecodeString(tUsername,16,&tReceiveBit);

						short tServerIndex = -1;
						for(int i=0; i<MAX_SERVERS; i++)
							if(serverAdd[i]==p->systemAddress)
							{
								tServerIndex = i;
								break;
							}
						if(tServerIndex==-1)break;

						if(tEnter)
						{
							bool tIsUsed = false;
							const string tUsernameStr = toLowerCase(string(tUsername));
							for(int i=0; i<MAX_SERVERS; i++)
							{
								for(std::vector<string>::iterator it=loggedOnUsernames[i].begin(); it!=loggedOnUsernames[i].end(); it++)
								{
									if(*it==tUsernameStr)
									{
										tIsUsed = true;
										break;
									}
								}
								if(tIsUsed)break;
							}
							if(!tIsUsed)
							{
								loggedOnUsernames[tServerIndex].push_back(toLowerCase(string(tUsername)));
								if(showTraffic)printf("%s entered game.\n",tUsername);
							}
						}
						else
						{
							const string tUsernameStr = toLowerCase(string(tUsername));
							bool tFound = false;
							for(int i=0; i<MAX_SERVERS; i++)
							{
								for(std::vector<string>::iterator it=loggedOnUsernames[i].begin(); it!=loggedOnUsernames[i].end(); it++)
								{
									if(*it==tUsernameStr)
									{
										loggedOnUsernames[i].erase(it);
										tFound = true;
										break;
									}
								}
								if(tFound)break;
							}
							if(showTraffic)printf("%s left game.\n",tUsername);
						}
					}
					break;*/

				case ID_IMASERVER:
					sendServerList(p->systemAddress,false);
					registerServer(p);
					break;

				/*
				case ID_SAVEITEM:
					{
						RakNet::BitStream tReceiveBit(p->data, p->length, false);
						MessageID tMessage;
						char tUsername[16] = "";
						unsigned short tIndex = 0;
						char tItem[MAX_EQUIP][16];
						char tHP[16] = "";
						char tSkills[512] = "";
						char tPet[32] = "";

						tReceiveBit.Read(tMessage);
						stringCompressor->DecodeString(tUsername,16,&tReceiveBit);
						tReceiveBit.Read(tIndex);
						for(int j=0;j<MAX_EQUIP;j++)stringCompressor->DecodeString(tItem[j],16,&tReceiveBit);
						stringCompressor->DecodeString(tHP,16,&tReceiveBit);
						stringCompressor->DecodeString(tSkills,512,&tReceiveBit);
						stringCompressor->DecodeString(tPet,32,&tReceiveBit);

						string tName = "";
						bool tSuccess = false;
						std::ifstream inFile(getFilename(tUsername,".charlist").c_str());
						if(inFile.good())
						{
							unsigned short tCount = 0;
							while(!inFile.eof() && inFile.good())
							{
								char tLine[32] = "";
								inFile.getline(tLine,32);
								if(tIndex==tCount)
								{
									tName = tLine;
									tSuccess = true;
									break;
								}
								tCount++;
							}
						}
						inFile.close();

						if(tSuccess)
						{
							//Write new item file
							string tFilename = tUsername;
							tFilename += "_";
							tFilename += tName;

							//Items
							string tBuffer = "";
							for(int j=0;j<MAX_EQUIP;j++)
							{
								tBuffer += tItem[j];
								tBuffer += "\n";
							}
							//HP
							tBuffer += tHP;
							tBuffer += "\n";
							//Skills
							tBuffer += tSkills;
							tBuffer += "\n";
							//Pet
							tBuffer += tPet;
							tBuffer += "\n";

							std::ofstream outFile(getFilename(tFilename.c_str(),".item").c_str());
							outFile.write(tBuffer.c_str(),tBuffer.length());
							outFile.close();
						}

						if(showTraffic)
							if(tSuccess)printf("%s[%i] save item success.\n",tUsername,tIndex);
							else printf("%s[%i] save item failed.\n",tUsername,tIndex);
					}
					break;*/

				// /setadmin <target> <rank>: owner/headadmin promotes or demotes an
			// account. Fully server-authoritative: the requester is resolved
			// from the live login session (NOT from the packet claim) and
			// both ranks are re-read from disk. Hierarchy enforced by
			// KITF_RankSetAllowed: owner can set anyone to anything;
			// headadmin can only touch accounts below headadmin and grant
			// ranks below headadmin.
				case ID_SETADMIN:
					{
						RakNet::BitStream tReceiveBit(p->data, p->length, false);
						MessageID tMessage;
						char tRequester[16] = "";
						char tTarget[16] = "";
						char tRankName[16] = "";
						tReceiveBit.Read(tMessage);
						stringCompressor->DecodeString(tRequester,16,&tReceiveBit);
						stringCompressor->DecodeString(tTarget,16,&tReceiveBit);
						stringCompressor->DecodeString(tRankName,16,&tReceiveBit);

						bool tSuccess = false;
						string tOut = "Denied.";
						// Bind the requester to the authenticated login session:
						// a forged packet claiming to be someone else is rejected.
						const string tSessionUser = sessionUserFor(p->systemAddress);
						const string tClaimed = toLowerCase(string(tRequester));
						if(tSessionUser.empty() || tSessionUser != tClaimed)
						{
							tOut = "Denied: you must be logged in to use /setadmin.";
							printf("[SETADMIN] Rejected packet from %s (no matching login session, claimed '%s').\n",p->systemAddress.ToString(),tRequester);
						}
						else
						{
							const int tReqRank = effectiveAccountRank(tRequester);
							const int tNewRank = KITF_RankFromName(string(tRankName));
							const int tCurRank = effectiveAccountRank(tTarget);
							if(tReqRank < 0)
								tOut = "Denied: requester account not found.";
							else if(!KITF_RankCanSetAdmin(tReqRank))
							{
								tOut = "Denied: /setadmin requires owner or headadmin.";
								printf("[SETADMIN] '%s' (rank %s) tried to set '%s' -> denied (insufficient rank).\n",tRequester,KITF_RankName(tReqRank),tTarget);
							}
							else if(tNewRank < 0)
								tOut = "Denied: unknown rank. Use normal/vip/trialmod/mod/headmod/gm/headadmin/owner.";
							else if(tCurRank < 0)
								tOut = "Denied: no such account.";
							else if(!KITF_RankSetAllowed(tReqRank,tCurRank,tNewRank))
							{
								tOut = "Denied: rank hierarchy (headadmin cannot touch headadmin/owner).";
								printf("[SETADMIN] '%s' (rank %s) tried to set '%s' (%s) to %s -> denied (hierarchy).\n",tRequester,KITF_RankName(tReqRank),tTarget,KITF_RankName(tCurRank),KITF_RankName(tNewRank));
							}
							else if(setAccountRank(tTarget,tNewRank))
							{
								tSuccess = true;
								char tMsgBuf[64] = "";
								sprintf(tMsgBuf,"%.15s is now %.10s.",tTarget,KITF_RankName(tNewRank));
								tOut = tMsgBuf;
								printf("[SETADMIN] '%s' (rank %s) set '%s' (%s) -> %s.\n",tRequester,KITF_RankName(tReqRank),tTarget,KITF_RankName(tCurRank),KITF_RankName(tNewRank));
								ofstream slog("security.log",ios::app);
								if(slog.good())slog<<currentDateTime()<<" - SETADMIN - "<<tRequester<<" set "<<tTarget<<" to "<<KITF_RankName(tNewRank)<<"\n";
								slog.close();
								// Live update: online target applies the rank at once.
								pushRankToSession(toLowerCase(string(tTarget)),tNewRank);
							}
							else tOut = "Denied: could not write account file.";
						}

						RakNet::BitStream tBitStream;
						tBitStream.Write(MessageID(ID_SETADMIN));
						tBitStream.Write(tSuccess);
						stringCompressor->EncodeString(tOut.c_str(),64,&tBitStream);
						server->Send(&tBitStream, HIGH_PRIORITY, RELIABLE, 1, p->systemAddress, false);
					}
					break;

			// /setvip <target> <days> [reason]: admins (GM+) grant timed VIP
			// (e.g. donators). VIP auto-reverts to normal at expiry via the
			// sweeper; the reply reuses ID_SETADMIN so the client shows it
			// with the existing handler. Days: 1..3650.
				case ID_SETVIP:
					{
						RakNet::BitStream tReceiveBit(p->data, p->length, false);
						MessageID tMessage;
						char tRequester[16] = "";
						char tTarget[16] = "";
						unsigned short tDays = 0;
						char tReasonBuf[64] = "";
						tReceiveBit.Read(tMessage);
						stringCompressor->DecodeString(tRequester,16,&tReceiveBit);
						stringCompressor->DecodeString(tTarget,16,&tReceiveBit);
						tReceiveBit.Read(tDays);
						stringCompressor->DecodeString(tReasonBuf,64,&tReceiveBit);

						bool tSuccess = false;
						string tOut = "Denied.";
						// Same session binding as /setadmin: forged packets
						// claiming to be someone else are rejected.
						const string tSessionUser = sessionUserFor(p->systemAddress);
						const string tClaimed = toLowerCase(string(tRequester));
						if(tSessionUser.empty() || tSessionUser != tClaimed)
						{
							tOut = "Denied: you must be logged in to use /setvip.";
							printf("[SETVIP] Rejected packet from %s (no matching login session, claimed '%s').\n",p->systemAddress.ToString(),tRequester);
						}
						else
						{
							const int tReqRank = effectiveAccountRank(tRequester);
							const int tCurRank = effectiveAccountRank(tTarget);
							// Sanitize the reason for the log file (no newlines).
							string tReason = tReasonBuf;
							for(size_t i = 0; i < tReason.size(); i++)
								if(tReason[i]=='\r' || tReason[i]=='\n') tReason[i] = ' ';
							if(tReqRank < 0)
								tOut = "Denied: requester account not found.";
							else if(!KITF_RankIsAdmin(tReqRank))
							{
								tOut = "Denied: /setvip requires admin (GM+).";
								printf("[SETVIP] '%s' (rank %s) tried to set '%s' -> denied (not admin).\n",tRequester,KITF_RankName(tReqRank),tTarget);
							}
							else if(tDays < 1 || tDays > 3650)
								tOut = "Denied: days must be 1-3650.";
							else if(tCurRank < 0)
								tOut = "Denied: no such account.";
							else if(tCurRank != KITF_RANK_NORMAL && tCurRank != KITF_RANK_VIP)
							{
								tOut = "Denied: target already has a staff rank.";
								printf("[SETVIP] '%s' tried to set staff '%s' (%s) -> denied.\n",tRequester,tTarget,KITF_RankName(tCurRank));
							}
							else
							{
								time_t tNow;
								time(&tNow);
								const time_t tExp = tNow + (time_t)tDays * 86400;
								if(setAccountRankExp(tTarget,KITF_RANK_VIP,tExp))
								{
									tSuccess = true;
									char tMsgBuf[64] = "";
									sprintf(tMsgBuf,"%.15s is vip for %u day(s).",tTarget,tDays);
									tOut = tMsgBuf;
									printf("[SETVIP] '%s' set '%s' to vip for %u day(s). Reason: %s\n",tRequester,tTarget,tDays,tReason.c_str());
									ofstream slog("security.log",ios::app);
									if(slog.good())slog<<currentDateTime()<<" - SETVIP - "<<tRequester<<" set "<<tTarget<<" to vip for "<<tDays<<" day(s). Reason: "<<tReason<<"\n";
									slog.close();
									pushRankToSession(toLowerCase(string(tTarget)),KITF_RANK_VIP);
								}
								else tOut = "Denied: could not write account file.";
							}
						}

						RakNet::BitStream tBitStream;
						tBitStream.Write(MessageID(ID_SETADMIN));
						tBitStream.Write(tSuccess);
						stringCompressor->EncodeString(tOut.c_str(),64,&tBitStream);
						server->Send(&tBitStream, HIGH_PRIORITY, RELIABLE, 1, p->systemAddress, false);
					}
					break;

			// weirdly in magix mainserver is used only 3 times for a adding server, spawn (of pets ?) and damage received by another user ?
				case ID_GODSPEAK:
					{
						RakNet::BitStream tReceiveBit(p->data, p->length, false);
						// Server-side check (was an open rebroadcast): clients may
						// only godspeak with an admin rank; game servers relay.
						const bool tFromServer = (getServerID(p->systemAddress)!=MAX_SERVERS);
						string tGodBy = "gameserver";
						if(!tFromServer)
						{
							tGodBy = sessionUserFor(p->systemAddress);
							if(tGodBy.empty()) tGodBy = p->systemAddress.ToString();
							const int tRank = (tGodBy==p->systemAddress.ToString()) ? KITF_RANK_NORMAL : effectiveAccountRank(tGodBy.c_str());
							if(!KITF_RankIsAdmin(tRank))
							{
								printf("[GODSPEAK] Rejected non-admin godspeak from %s.\n",p->systemAddress.ToString());
								break;
							}
						}
						if(showCommands)
						{
							RakNet::BitStream tLogBit(p->data, p->length, false);
							MessageID tLogID;
							char tCap[512] = "";
							tLogBit.Read(tLogID);
							stringCompressor->DecodeString(tCap,512,&tLogBit);
							printf("[CMD] recv ID_GODSPEAK from '%s': %.160s\n",tGodBy.c_str(),tCap);
						}
						server->Send(&tReceiveBit, MEDIUM_PRIORITY, RELIABLE, 4, p->systemAddress, true);
					}
					break;

				case ID_ITEMSTASH:
					{
						RakNet::BitStream tReceiveBit(p->data, p->length, false);
						MessageID tMessage;
						OwnerToken tToken;
						bool tIsRequest;

						tReceiveBit.Read(tMessage);
						tReceiveBit.Read(tToken);
						tReceiveBit.Read(tIsRequest);

						if(tToken>MAX_CLIENTS || tToken<=0 || playerToken[tToken-1].add==UNASSIGNED_SYSTEM_ADDRESS)break;

						const char *tUsername = playerToken[tToken-1].name.c_str();

						//Retrieve and send stash data
						// to do from sql
						if(tIsRequest)
						{
							RakNet::BitStream tBitStream;

							tBitStream.Write(MessageID(ID_ITEMSTASH));
							tBitStream.Write(tToken);

							std::ifstream inFile(getFilename(tUsername, ".stash").c_str());
							if(inFile.good())
							{
								while (!inFile.eof() && inFile.good())
								{
									char tLine[16] = "";
									inFile.getline(tLine, 16);

									if (strlen(tLine) > 0)
									{
										tBitStream.Write(true);
										stringCompressor->EncodeString(tLine, 16, &tBitStream);
									}
								}
							}
							inFile.close();

							server->Send(&tBitStream, HIGH_PRIORITY, RELIABLE, 6, p->systemAddress, false);
						}
						//Update stash data
						else
						{
							bool tIsAdd = true;
							tReceiveBit.Read(tIsAdd);
							//Add item
							if(tIsAdd)
							{
								string tItem = "";
								unsigned short tSlot;
								tReceiveBit.Read(tSlot);

								//Unequip item from player
								if(tSlot>=0 && tSlot<MAX_EQUIP)
								{
									tItem = playerToken[tToken-1].item[tSlot];
									playerToken[tToken-1].item[tSlot] = "";
								}

								//Save item stash
								if(tItem.length()>0)
								{
									std::ofstream outFile(getFilename(tUsername,".stash").c_str(),ios_base::app);
									const string tBuffer = tItem + "\n";
									outFile.write(tBuffer.c_str(),tBuffer.length());
									outFile.close();
								}
							}
							//Remove item
							else
							{
								unsigned short tLineID;
								tReceiveBit.Read(tLineID);
								string tBuffer = "";
								unsigned short tCount = 0;
								string tItem = "";

								//Extract item from stash
								std::ifstream inFile(getFilename(tUsername,".stash").c_str());
								if(inFile.good())
								{
									while(!inFile.eof() && inFile.good())
									{
										char tLine[16] = "";
										inFile.getline(tLine,16);
										if(strlen(tLine)>0)
										{
											if(tCount!=tLineID)
											{
												tBuffer += tLine;
												tBuffer += "\n";
											}
											else tItem = tLine;
										}
										tCount++;
									}
								}
								inFile.close();
								
								std::ofstream outFile(getFilename(tUsername, ".stash").c_str());
								if (tBuffer.length() > 0)
								{
									outFile.write(tBuffer.c_str(), tBuffer.length());
								}
								
								outFile.close();

								//Equip item on player
								unsigned short tSlot;
								tReceiveBit.Read(tSlot);
								if(tSlot>=0 && tSlot<MAX_EQUIP)playerToken[tToken-1].item[tSlot] = tItem;
							}
						}
					}
					break;
				case ID_ITEMEQUIP:
					{
						RakNet::BitStream tReceiveBit(p->data, p->length, false);
						MessageID tMessage;
						OwnerToken tToken;

						tReceiveBit.Read(tMessage);
						tReceiveBit.Read(tToken);

						if(tToken>MAX_CLIENTS || tToken<=0 || playerToken[tToken-1].add==UNASSIGNED_SYSTEM_ADDRESS)break;

						char tItem[256];
						unsigned short tSlot;
						stringCompressor->DecodeString(tItem,16,&tReceiveBit);
						tReceiveBit.Read(tSlot);

						if(tSlot>=0 && tSlot<MAX_EQUIP)playerToken[tToken-1].item[tSlot] = tItem;
					}
					break;
				case ID_ITEMUNEQUIP:
					{
						RakNet::BitStream tReceiveBit(p->data, p->length, false);
						MessageID tMessage;
						OwnerToken tToken;

						tReceiveBit.Read(tMessage);
						tReceiveBit.Read(tToken);

						if(tToken>MAX_CLIENTS || tToken<=0 || playerToken[tToken-1].add==UNASSIGNED_SYSTEM_ADDRESS)break;

						unsigned short tSlot;
						tReceiveBit.Read(tSlot);

						if(tSlot>=0 && tSlot<MAX_EQUIP)playerToken[tToken-1].item[tSlot] = "";
					}
					break;
				case ID_UPDATEHP:
					{
						RakNet::BitStream tReceiveBit(p->data, p->length, false);
						MessageID tMessage;
						OwnerToken tToken;

						tReceiveBit.Read(tMessage);
						tReceiveBit.Read(tToken);

						if(tToken>MAX_CLIENTS || tToken<=0 || playerToken[tToken-1].add==UNASSIGNED_SYSTEM_ADDRESS)break;

						unsigned short tHP = 500;
						bool tHasMaxHP = false;
						tReceiveBit.Read(tHP);
						playerToken[tToken-1].hp.first = tHP;
						tReceiveBit.Read(tHasMaxHP);
						if(tHasMaxHP)
						{
							unsigned short tMaxHP = 500;
							tReceiveBit.Read(tMaxHP);
							playerToken[tToken-1].hp.second = tMaxHP;
						}
					}
					break;
				case ID_UPDATESKILLS:
					{
						RakNet::BitStream tReceiveBit(p->data, p->length, false);
						MessageID tMessage;
						OwnerToken tToken;

						tReceiveBit.Read(tMessage);
						tReceiveBit.Read(tToken);

						if(tToken>MAX_CLIENTS || tToken<=0 || playerToken[tToken-1].add==UNASSIGNED_SYSTEM_ADDRESS)break;

						char tSkill[32] = "";
						unsigned char tStock = 0;
						stringCompressor->DecodeString(tSkill,32,&tReceiveBit);
						tReceiveBit.Read(tStock);
						if(strlen(tSkill)>0)
						{
							bool tFound = false;
							for(vector<pair<string,unsigned char> >::iterator it=playerToken[tToken-1].skill.begin();it!=playerToken[tToken-1].skill.end();it++)
							{
								pair<string,unsigned char> *tPair = &*it;
								if(tPair->first==string(tSkill))
								{
									tFound = true;
									if(tStock>0)tPair->second = tStock;
									else playerToken[tToken-1].skill.erase(it);
									break;
								}
							}
							if(!tFound && tStock>0)playerToken[tToken-1].skill.push_back(pair<string,unsigned char>(tSkill,tStock));
						}
					}
					break;
				case ID_UPDATEPET:
					{
						RakNet::BitStream tReceiveBit(p->data, p->length, false);
						MessageID tMessage;
						OwnerToken tToken;

						tReceiveBit.Read(tMessage);
						tReceiveBit.Read(tToken);

						if(tToken>MAX_CLIENTS || tToken<=0 || playerToken[tToken-1].add==UNASSIGNED_SYSTEM_ADDRESS)break;

						char tPet[32] = "";
						stringCompressor->DecodeString(tPet,32,&tReceiveBit);
						playerToken[tToken-1].pet = tPet;
					}
					break;
				case ID_VIOLATION:
					{
						RakNet::BitStream tReceiveBit(p->data, p->length, false);
						MessageID tMessage;
						SystemAddress tAdd;
						char tInfo[32]="";
						std::ostringstream sin;

						tReceiveBit.Read(tMessage);
						tReceiveBit.Read(tAdd);
						stringCompressor->DecodeString(tInfo,32,&tReceiveBit);

						const OwnerToken tToken = getTokenByAdd(tAdd);

						ofstream outFile("./violations.log",ios_base::app);
						if(outFile.good())
						{
							outFile << tInfo << " -> " << tAdd.ToString();
							if(tToken>0 && tToken<=MAX_CLIENTS)
							{
								outFile << " -> " << playerToken[tToken-1].name << " [" << playerToken[tToken-1].charID << "]";
							}
							outFile << " -> " << currentDateTime() << endl;
						}
						outFile.close();
					}
					break;
				case ID_KICK:
					{
						RakNet::BitStream tReceiveBit(p->data, p->length, false);
						MessageID tMessage;
						char tName[16];
						bool tIsBanned = false;
						unsigned short tNumDays = 0;

						tReceiveBit.Read(tMessage);
						stringCompressor->DecodeString(tName,16,&tReceiveBit);
						tReceiveBit.Read(tIsBanned);
						// Server-side check (was fully trusted): a kick/ban notice
						// is only honored from a registered game server, or from a
						// login session whose account has moderator powers.
						const bool tKickFromServer = (getServerID(p->systemAddress)!=MAX_SERVERS);
						string tKickBy = "gameserver";
						if(!tKickFromServer)
						{
							tKickBy = sessionUserFor(p->systemAddress);
							if(tKickBy.empty()) tKickBy = p->systemAddress.ToString();
							const int tRank = (tKickBy==p->systemAddress.ToString()) ? KITF_RANK_NORMAL : effectiveAccountRank(tKickBy.c_str());
							if(!KITF_RankCanModerate(tRank))
							{
								printf("[KICK] Rejected kick/ban request from %s (insufficient rank).\n",p->systemAddress.ToString());
								break;
							}
						}
						if(tIsBanned)
						{
							tReceiveBit.Read(tNumDays);

							time_t rawtime;
							struct tm * timeinfo;
							time(&rawtime);
							timeinfo = localtime(&rawtime);
							int tYear = timeinfo->tm_year;
							int tDay = timeinfo->tm_yday + tNumDays;
							while(tDay>365)
							{
								tYear += 1;
								tDay -= 365;
							}
							BanInfo tInfo(tName,tYear,tDay);
							const OwnerToken tToken = getTokenByName(tName);
							if(tToken>0)
							{
								tInfo.IPList.push_back(playerToken[tToken-1].add.ToString(false));
								banlist.push_back(tInfo);
								server->AddToBanList(playerToken[tToken-1].add.ToString(false));
								updateBanlist();
								saveBanlist();
							}
						}
						if(showCommands)
						{
							if(tIsBanned)printf("[CMD] recv ID_KICK: '%s' kicked+banned '%s' for %u day(s).\n",tKickBy.c_str(),tName,tNumDays);
							else printf("[CMD] recv ID_KICK: '%s' kicked '%s'.\n",tKickBy.c_str(),tName);
						}
					}
					break;

				default:
					break;
					/*printf("%i:",(int)packetIdentifier);
					puts((char*)p->data);*/
			}

			server->DeallocatePacket(p);
			p = server->Receive();
			}
		}
	}
	const string getFilename(const char *name, const char *fileExtension, bool replaceSpaces=true, bool replaceUnderscores=false)
	{
		//Get first character to determine folder
		string firstChar = "0";
		string cname;
		if(strlen(name)>0 && name[0]>='A' && name[0]<='z')
		{
			firstChar[0] = name[0];
			if(name[0]>='a')
				firstChar[0] = name[0]-'a'+'A';
		}
		string filename = "Data/" + firstChar + "/";
		cname = toLowerCase(name);
		// Anti path-traversal / injection sanitization: only allow safe filename
		// characters. Anything else (path separators, dots, '..', quotes, control
		// chars) is stripped so a hostile username in a network packet can never
		// build a path that leaves the Data directory.
		{
			string safe;
			for(size_t i=0;i<cname.length();i++){
				unsigned char ch=(unsigned char)cname[i];
				bool ok=(ch>='a'&&ch<='z')||(ch>='0'&&ch<='9')||ch=='_';
				if(ok)safe+= (char)ch;
			}
			safe=safe.substr(0,24); // keep length sane
			cname=safe;
		}
		filename += cname;
		//Convert all spaces to underscores
		if(replaceSpaces)
		{
			for(int i=0;i<(int)filename.length();i++)
				if(filename[i]==' ')filename[i] = '_';
		}
		else if(replaceUnderscores)
		{
			for(int i=0;i<(int)filename.length();i++)
				if(filename[i]=='_')filename[i] = ' ';
		}
		filename += fileExtension;
		return filename;
	}
	void shutdown()
	{
		server->Shutdown(300);
		savePingRange();
		saveBanlist();
#if defined(_WIN32)
		if(httpListenSock!=INVALID_SOCKET){
			closesocket(httpListenSock);
			httpListenSock=INVALID_SOCKET;
		}
		if(httpThreadH){WaitForSingleObject(httpThreadH,2000);CloseHandle(httpThreadH);httpThreadH=NULL;}
		WSACleanup();
#endif
	}
	const string toLowerCase(string text)
	{
		const char tDiff = char('A') - char('a');
		for(int i=0;i<(int)text.length();i++)
		{
			if(text[i]>=char('A')&&text[i]<=char('Z'))text[i] -= tDiff;
		}
		return text;
	}
	const unsigned char registerServer(Packet *p)
	{
		for(int i=0; i<MAX_SERVERS; i++)
		{
			if(serverTunnelAdd[i]==UNASSIGNED_SYSTEM_ADDRESS)
			{
				RakNet::BitStream tReceiveBit(p->data, p->length, false);
				MessageID tMessage;
				char buffer[64] = "";
				tReceiveBit.Read(tMessage);
				stringCompressor->DecodeString(buffer,64,&tReceiveBit);

				SystemAddress broadcastAdd;
				broadcastAdd.SetBinaryAddress(buffer);
				broadcastAdd.port = p->systemAddress.port;
				serverAdd[i] = broadcastAdd;

				serverTunnelAdd[i] = p->systemAddress;
				serverFull[i] = false;
				numServers++;
				printf("Server %i connected, Tunnel: %s, IP: %s\n",i+1,p->systemAddress.ToString(),broadcastAdd.ToString());
				broadcastServerConnected(i,p->systemAddress);
				notifyServerID(i,p->systemAddress);
				return i;
			}
		}
		return 0;
	}
	const unsigned char getServerID(const SystemAddress &add)
	{
		for(int i=0; i<MAX_SERVERS; i++)
		{
			if(serverTunnelAdd[i]==add)
			{
				return i;
			}
		}
		return MAX_SERVERS;
	}
	bool unregisterServer(Packet *p)
	{
		for(int i=0; i<MAX_SERVERS; i++)
		{
			if(serverTunnelAdd[i]==p->systemAddress)
			{
				//loggedOnUsernames[i].clear();
				unassignTokens(p->systemAddress);
				printf("Server %i disconnected, Tunnel: %s, IP: %s\n",i+1,p->systemAddress.ToString(),serverAdd[i].ToString());
				broadcastServerDisconnected(i,p->systemAddress);
				serverAdd[i] = UNASSIGNED_SYSTEM_ADDRESS;
				serverTunnelAdd[i] = UNASSIGNED_SYSTEM_ADDRESS;
				serverFull[i] = false;
				numServers--;
				return true;
			}
		}
		return false;
	}
	void sendServerList(const SystemAddress &target, const bool &isClient)
	{
		RakNet::BitStream tBitStream;

		tBitStream.Write(MessageID(ID_SERVERLIST));
		for(int i=0; i<MAX_SERVERS; i++)
		{
			if(serverTunnelAdd[i]!=UNASSIGNED_SYSTEM_ADDRESS)
			{
				tBitStream.Write(true);
				tBitStream.Write((unsigned char)i);
				if(isClient)tBitStream.Write(SystemAddress(serverAdd[i]));
				else tBitStream.Write(SystemAddress(serverTunnelAdd[i]));
				tBitStream.Write(serverFull[i]);
			}
		}
		server->Send(&tBitStream, HIGH_PRIORITY, RELIABLE, 0, target, false);
	}
	void notifyServerID(const unsigned char &iID, const SystemAddress &add)
	{
		RakNet::BitStream tBitStream;

		tBitStream.Write(MessageID(ID_SERVERCONNECTED));
		tBitStream.Write(true);
		tBitStream.Write(iID);
		tBitStream.Write(SystemAddress(serverAdd[iID]));
		tBitStream.Write(SystemAddress(serverTunnelAdd[iID]));
		tBitStream.Write(true);

		server->Send(&tBitStream, HIGH_PRIORITY, RELIABLE, 0, add, false);
	}
	void broadcastServerConnected(const unsigned char &iID, const SystemAddress &exceptionAdd)
	{
		RakNet::BitStream tBitStream;

		tBitStream.Write(MessageID(ID_SERVERCONNECTED));
		tBitStream.Write(true);
		tBitStream.Write(iID);
		tBitStream.Write(SystemAddress(serverAdd[iID]));
		tBitStream.Write(SystemAddress(serverTunnelAdd[iID]));

		server->Send(&tBitStream, HIGH_PRIORITY, RELIABLE, 0, exceptionAdd, true);
	}
	void broadcastServerDisconnected(const unsigned char &iID, const SystemAddress &exceptionAdd)
	{
		RakNet::BitStream tBitStream;

		tBitStream.Write(MessageID(ID_SERVERCONNECTED));
		tBitStream.Write(false);
		tBitStream.Write(SystemAddress(serverAdd[iID]));
		tBitStream.Write(SystemAddress(serverTunnelAdd[iID]));

		server->Send(&tBitStream, HIGH_PRIORITY, RELIABLE, 0, exceptionAdd, true);
	}
	void broadcastMaintenance(const unsigned char &time)
	{
		RakNet::BitStream tBitStream;

		tBitStream.Write(MessageID(ID_MAINTENANCE));
		tBitStream.Write(time);

		server->Send(&tBitStream, HIGH_PRIORITY, RELIABLE, 0, UNASSIGNED_SYSTEM_ADDRESS, true);
	}
	const OwnerToken assignToken(const SystemAddress &add, const unsigned char &serverID, const string &name, const unsigned short &charID)
	{
		for(int i=0;i<MAX_CLIENTS;i++)
		{
			if(playerToken[i].add==UNASSIGNED_SYSTEM_ADDRESS)
			{
				playerToken[i].add = add;
				playerToken[i].serverID = serverID;
				playerToken[i].name = name;
				playerToken[i].charID = charID;
				loadPlayerData(i+1);
				numClients++;
				return (i+1);
			}
		}
		return 0;
	}
	void unassignToken(const OwnerToken &token)
	{
		if(token>0 && token<=MAX_CLIENTS)
		{
			savePlayerData(token);
			playerToken[token-1] = PlayerToken();
			numClients--;
		}
	}
	void unassignToken(const SystemAddress &add)
	{
		for(int i=0;i<MAX_CLIENTS;i++)
		{
			if(playerToken[i].add==add)
			{
				unassignToken(i+1);
				return;
			}
		}
	}
	const OwnerToken getTokenByAdd(const SystemAddress &add)
	{
		for(int i=0;i<MAX_CLIENTS;i++)
		{
			if(playerToken[i].add==add)
			{
				return (i+1);
			}
		}
		return 0;
	}
	const OwnerToken getTokenByName(const string &name)
	{
		for(int i=0;i<MAX_CLIENTS;i++)
		{
			if(toLowerCase(playerToken[i].name)==toLowerCase(name))
			{
				return (i+1);
			}
		}
		return 0;
	}
	void unassignTokens(const SystemAddress &serverAdd)
	{
		const unsigned char tServerID = getServerID(serverAdd);
		for(int i=0;i<MAX_CLIENTS;i++)
		{
			if(playerToken[i].add!=UNASSIGNED_SYSTEM_ADDRESS && playerToken[i].serverID==tServerID)
			{
				unassignToken(i+1);
			}
		}
	}
	void broadcastServerUpdate()
	{
		for(int i=0; i<MAX_SERVERS; i++)
		{
			if(serverTunnelAdd[i]!=UNASSIGNED_SYSTEM_ADDRESS)
			{
				RakNet::BitStream tBitStream;

				tBitStream.Write(MessageID(ID_SERVERUPDATE));

				server->Send(&tBitStream, HIGH_PRIORITY, RELIABLE, 0, serverTunnelAdd[i], false);
			}
		}
	}
	void loadPlayerData(const OwnerToken &token)
	{
		if(token<=0 || token>MAX_CLIENTS)return;

		string tName = "";
		bool tSuccess = false;
		std::ifstream inFile(getFilename(playerToken[token-1].name.c_str(),".charlist").c_str());
		if(inFile.good())
		{
			unsigned short tCount = 0;
			while(!inFile.eof() && inFile.good())
			{
				char tLine[32] = "";
				inFile.getline(tLine,32);
				if(playerToken[token-1].charID==tCount)
				{
					tName = tLine;
					tSuccess = true;
					break;
				}
				tCount++;
			}
		}
		inFile.close();
		if(!tSuccess)return;

		string tFilename = playerToken[token-1].name;
		tFilename += "_";
		tFilename += tName;

		//Load everything
		char tItem[MAX_EQUIP][16];
		for(int j=0;j<MAX_EQUIP;j++)strcpy(tItem[j],"");
		char tHP[16] = "";
		char tSkills[512] = "";
		char tPet[32] = "";
		inFile.open(getFilename(tFilename.c_str(),".item").c_str());
		if(inFile.good())
		{
			for(int j=0;j<MAX_EQUIP;j++)
				if(inFile.good())inFile.getline(tItem[j],16);
			if(inFile.good())inFile.getline(tHP,16);
			if(inFile.good())inFile.getline(tSkills,512);
			if(inFile.good())inFile.getline(tPet,32);
		}
		inFile.close();

		for(int j=0;j<MAX_EQUIP;j++)playerToken[token-1].item[j] = tItem[j];
		const vector<string> tHPPart = tokenize(string(tHP),";\n");
		if(tHPPart.size()==2)
		{
			playerToken[token-1].hp.first = atoi(tHPPart[0].c_str());
			playerToken[token-1].hp.second = atoi(tHPPart[1].c_str());
		}
		else
		{
			playerToken[token-1].hp.first = 500;
			playerToken[token-1].hp.second = 500;
		}
		const vector<string> tSkillLine = tokenize(string(tSkills),"|\n");
		for(int i=0;i<(int)tSkillLine.size();i++)
		{
			const vector<string> tSkillPart = tokenize(tSkillLine[i],";");
			if(tSkillPart.size()==2)
			{
				playerToken[token-1].skill.push_back(pair<string,unsigned char>(tSkillPart[0],atoi(tSkillPart[1].c_str())));
			}
		}
		playerToken[token-1].pet = tPet;
	}
	void savePlayerData(const OwnerToken &token)
	{
		if(token<=0 || token>MAX_CLIENTS)return;

		string tName = "";
		bool tSuccess = false;
		std::ostringstream sin;
		std::ifstream inFile(getFilename(playerToken[token-1].name.c_str(),".charlist").c_str());
		if(inFile.good())
		{
			unsigned short tCount = 0;
			while(!inFile.eof() && inFile.good())
			{
				char tLine[32] = "";
				inFile.getline(tLine,32);
				if(playerToken[token-1].charID==tCount)
				{
					tName = tLine;
					tSuccess = true;
					break;
				}
				tCount++;
			}
		}
		inFile.close();
		if(!tSuccess)return;

		//Write new item file
		string tFilename = playerToken[token-1].name;
		tFilename += "_";
		tFilename += tName;

		//Items
		string tBuffer = "";
		for(int j=0;j<MAX_EQUIP;j++)
		{
			tBuffer += playerToken[token-1].item[j];
			tBuffer += "\n";
		}

		//HP
		char tHP[6] = "";
		char tMaxHP[6] = "";
		sprintf(tHP,"%i",playerToken[token-1].hp.first);
		sprintf(tMaxHP,"%i",(playerToken[token-1].hp.second==0?500:playerToken[token-1].hp.second));
		string tHPStr = tHP;
		tHPStr += ";";
		tHPStr += tMaxHP;

		tBuffer += tHPStr;
		tBuffer += "\n";

		//Skills
		string tSkill = "";
		for(int j=0;j<(int)playerToken[token-1].skill.size();j++)
		{
			tSkill += playerToken[token-1].skill[j].first;
			tSkill += ";";
			char tStock[4] = "";
			sprintf(tStock,"%i",playerToken[token-1].skill[j].second);
			tSkill += tStock;
			tSkill += "|";
		}
		if(tSkill.length()>=512)
		{
			printf("(%i)%s skill length longer than 512\n",(int)token,playerToken[token-1].name.c_str());
			ofstream outFile("./exceptions.log",ios_base::app);
			char tBuffer[128] = "";
			sin << (int)token;
			std::string val = sin.str();
			strcpy(tBuffer,val.c_str());
			//itoa((int)token,tBuffer,10);
			outFile.write(tBuffer,strlen(tBuffer));
			strcpy(tBuffer,playerToken[token-1].name.c_str());
			outFile.write(tBuffer,strlen(tBuffer));
			strcpy(tBuffer," skill length longer than 512\n");
			outFile.write(tBuffer,strlen(tBuffer));
			outFile.close();
		}

		tBuffer += tSkill;
		tBuffer += "\n";

		//Pet
		tBuffer += playerToken[token-1].pet;
		tBuffer += "\n";

		std::ofstream outFile(getFilename(tFilename.c_str(),".item").c_str());
		outFile.write(tBuffer.c_str(),tBuffer.length());
		outFile.close();
	}
	const vector<string> tokenize(const string& str, const string& delimiters = " ")
	{
		vector<string> tokens;
		// Skip delimiters at beginning.
		string::size_type lastPos = str.find_first_not_of(delimiters, 0);
		// Find first "non-delimiter".
		string::size_type pos     = str.find_first_of(delimiters, lastPos);

		while (string::npos != pos || string::npos != lastPos)
		{
			// Found a token, add it to the vector.
			tokens.push_back(str.substr(lastPos, pos - lastPos));
			// Skip delimiters.  Note the "not_of"
			lastPos = str.find_first_not_of(delimiters, pos);
			// Find next "non-delimiter"
			pos = str.find_first_of(delimiters, lastPos);
		}

		return tokens;
	}
	void updateTimer()
	{
		time_t currTime;
		time(&currTime);
		const float timeSinceLastUpdate = (float)difftime(currTime,prevTime);
		prevTime = currTime;
		dayTime += timeSinceLastUpdate;
		if(dayTime>=2400)dayTime = 0;
		weatherTime += timeSinceLastUpdate;
		if(weatherTime>=7000)weatherTime = 0;

		//4 hourly maintenance
		const unsigned short MAINTENANCE_PERIOD = 14400;
		/*unsigned char tPrepareMark=0;
		if(maintenanceTime<MAINTENANCE_PERIOD-60)tPrepareMark = 60;
		else if(maintenanceTime<MAINTENANCE_PERIOD-30)tPrepareMark = 30;
		else if(maintenanceTime<MAINTENANCE_PERIOD-15)tPrepareMark = 15;*/
		maintenanceTime += timeSinceLastUpdate;
		//Check if timer crossed the mark
		//if(tPrepareMark!=0 && maintenanceTime>=MAINTENANCE_PERIOD-tPrepareMark)broadcastMaintenance(tPrepareMark);
		if(maintenanceTime>=MAINTENANCE_PERIOD)
		{
			updateBanlist();
			saveBanlist();
			//broadcastServerUpdate();
			maintenanceTime = 0;
		}
	}
	void updateServers(bool forceUpdate=false)
	{
		if(!forceUpdate && (int(dayTime)%10!=0))return;
		for(int i=0;i<MAX_SERVERS;i++)
		{
			if(serverTunnelAdd[i]!=UNASSIGNED_SYSTEM_ADDRESS)
			{
				const int tPing = server->GetAveragePing(serverTunnelAdd[i]);
				if(tPing>=highPing)
				{
					serverFull[i] = true;
				}
				else if(tPing<=lowPing)
				{
					serverFull[i] = false;
				}
			}
		}
	}
	const unsigned short getNumClientsInServer(const unsigned char &serverID)
	{
		unsigned short tCount = 0;
		for(int i=0;i<MAX_CLIENTS;i++)
		{
			if(playerToken[i].serverID==serverID)
			{
				tCount++;
			}
		}
		return tCount;
	}
	const string XOR7(const string &input)
	{
		string output = "";
		for(int i=0;i<(int)input.length();i++)
		{
			char tC = input[i];
			tC ^= 7*(i%7+1);
			//Replace illegal characters with original character
			if(tC>char(126) || tC<char(32))tC = input[i];
			output += tC;
		}
		return output;
	}
	const string XOR7OLD(const string &input)
	{
		string output = "";
		for(int i=0;i<(int)input.length();i++)
		{
			char tC = input[i];
			tC ^= 7*(i%7+1);
			output += tC;
		}
		return output;
	}
	void saveBanlist()
	{
		std::ostringstream sin;
		std::ofstream outFile("banlist.txt");
		string tBuffer;
		for(int i=0;i<(int)banlist.size();i++)
		{
			char tYear[6] = "";
			char tDay[6] = "";
			sin << banlist[i].year;
			std::string val = sin.str();
			strcpy(tYear,val.c_str());
			sin << banlist[i].yDay;
			val = sin.str();
			strcpy(tDay,val.c_str());
			//itoa(banlist[i].year,tYear,6,10);
			//itoa(banlist[i].yDay,tDay,6,10);
			tBuffer = banlist[i].name + ";" + tYear + ";" + tDay + "|";
			outFile.write(tBuffer.c_str(),tBuffer.length());
			for(int j=0;j<(int)banlist[i].IPList.size();j++)
			{
				tBuffer = banlist[i].IPList[j] + ";";
				outFile.write(tBuffer.c_str(),tBuffer.length());
			}
			tBuffer = "\n";
			outFile.write(tBuffer.c_str(),tBuffer.length());
		}
		outFile.close();
	}
	void loadBanlist()
	{
		std::ifstream inFile("banlist.txt");
		while(inFile.good() && !inFile.eof())
		{
			char tBuffer[1024] = "";
			inFile.getline(tBuffer,1024);
			const vector<string> tPart = tokenize(tBuffer,"|");
			if(tPart.size()>1)
			{
				const vector<string> tInfo = tokenize(tPart[0],";");
				if(tInfo.size()>2)
				{
					BanInfo tBanInfo(tInfo[0],atoi(tInfo[1].c_str()),atoi(tInfo[2].c_str()));
					const vector<string> tIPs = tokenize(tPart[1],";");
					for(int i=0;i<(int)tIPs.size();i++)
					{
						tBanInfo.IPList.push_back(tIPs[i]);
						server->AddToBanList(tIPs[i].c_str());
					}
					banlist.push_back(tBanInfo);
				}
			}
		}
		inFile.close();
	}
	void updateBanlist()
	{
		time_t rawtime;
		struct tm * timeinfo;
		time(&rawtime);
		timeinfo = localtime(&rawtime);
		const int tCurrentYear = timeinfo->tm_year;
		const int tCurrentDay = timeinfo->tm_yday;

		vector<BanInfo>::iterator it = banlist.begin();
		while(it!=banlist.end())
		{
			BanInfo tInfo = *it;
			const int tYear = tInfo.year - tCurrentYear;
			const int tDay = tInfo.yDay - tCurrentDay;
			if(tYear<0 || tYear==0&&tDay<=0)
			{
				for(int i=0;i<(int)tInfo.IPList.size();i++)
					server->RemoveFromBanList(tInfo.IPList[i].c_str());
				tInfo.IPList.clear();
				it = banlist.erase(it);
				continue;
			}
			it++;
		}
	}
	void clearBanlist()
	{
		vector<BanInfo>::iterator it = banlist.begin();
		while(it!=banlist.end())
		{
			BanInfo tInfo = *it;
			for(int i=0;i<(int)tInfo.IPList.size();i++)
				server->RemoveFromBanList(tInfo.IPList[i].c_str());
			tInfo.IPList.clear();
			it = banlist.erase(it);
		}
	}
	void endSession(Packet *p)
	{
		unsigned int k = 0;
		for(vector<pair<string,SystemAddress> >::iterator it = loginSession.begin(); it != loginSession.end(); it++)
		{
			pair<string,SystemAddress> tSession = *it;
			if(tSession.second==p->systemAddress)
			{
				for(int i=0; i<MAX_SERVERS; i++)
				{
					if(serverTunnelAdd[i]!=UNASSIGNED_SYSTEM_ADDRESS)
					{
						RakNet::BitStream tBitStream;

						tBitStream.Write(MessageID(ID_FORCELOGOUT));
						tBitStream.Write(false);
						stringCompressor->EncodeString(tSession.first.c_str(),16,&tBitStream);

						server->Send(&tBitStream, HIGH_PRIORITY, RELIABLE, 0, serverTunnelAdd[i], false);
					}
				}
				loginSession.erase(it);
				break;
			}
			k++;
		}
	}
	void loadPingRange()
	{
		lowPing = 40;
		highPing = 50;
		char tBuffer[16] = "";
		std::ifstream inFile("pingrange.txt");
		if(inFile.good() && !inFile.eof())
		{
			inFile.getline(tBuffer,16);
			const unsigned short tPing = atoi(tBuffer);
			if(tPing!=0)lowPing = tPing;
		}
		if(inFile.good() && !inFile.eof())
		{
			strcpy(tBuffer,"");
			inFile.getline(tBuffer,16);
			const unsigned short tPing = atoi(tBuffer);
			if(tPing!=0)highPing = tPing;
		}
		printf("Servers accept connections at ping below %i\n",(int)lowPing);
		printf("Servers reject connections at ping above %i\n",(int)highPing);
		inFile.close();
	}
	void savePingRange()
	{
		std::ofstream outFile("pingrange.txt");
		std::ostringstream sin;
		char tLowPing[16] = "", tHighPing[16] = "";
		sin << lowPing;
		std::string val = sin.str();
		strcpy(tLowPing,val.c_str());
		//itoa(lowPing,tLowPing,16,10);
		sin << highPing;
		val = sin.str();
		strcpy(tHighPing,val.c_str());
		//itoa(highPing,tHighPing,16,10);
		string tBuffer = tLowPing;
		tBuffer += "\n";
		outFile.write(tBuffer.c_str(),tBuffer.length());
		tBuffer = tHighPing;
		tBuffer += "\n";
		outFile.write(tBuffer.c_str(),tBuffer.length());
		outFile.close();
	}
};

// --------------------------------------------------------------------------
// Account-rank storage (declared in class ServerManager above; defined here
// out-of-class, same pattern as writeSecureUser).
// --------------------------------------------------------------------------

int ServerManager::rankLineValue(const string &line)
{
	const string prefix = "rank=";
	if(line.compare(0,prefix.size(),prefix)==0)
		return KITF_RankFromName(line.substr(prefix.size()));
	return -2; // not a rank line
}

// Resolve the on-disk .user file, trying the same name variants the
// logon path understands (underscores / spaces). Returns "" if none.
string ServerManager::resolveUserFile(const char *username)
{
	const string p1 = getFilename(username,".user");
	{ std::ifstream f(p1.c_str(),std::ios::binary); if(f.good()) return p1; }
	const string p2 = getFilename(username,".user",false);
	{ std::ifstream f(p2.c_str(),std::ios::binary); if(f.good()) return p2; }
	const string p3 = getFilename(username,".user",false,true);
	{ std::ifstream f(p3.c_str(),std::ios::binary); if(f.good()) return p3; }
	return "";
}

// -1 = no account file. Otherwise 0..7 (missing/unparseable line = normal).
// hasRankLine tells whether an explicit rank line was present (used for
// one-time migration of legacy per-character admin tokens).
int ServerManager::accountRankStatus(const char *username, bool &hasRankLine)
{
	hasRankLine = false;
	const string path = resolveUserFile(username);
	if(path.empty()) return -1;
	std::ifstream inFile(path.c_str(),std::ios::binary);
	if(!inFile.good()) return -1;
	string line;
	int found = -2;
	while(std::getline(inFile,line))
	{
		if(!line.empty() && line[line.size()-1]=='\r') line.erase(line.size()-1);
		int v = rankLineValue(line);
		if(v == -2) v = rankLineValue(XOR7(line));
		if(v != -2) found = v;
	}
	inFile.close();
	if(found == -2) return KITF_RANK_NORMAL;
	hasRankLine = true;
	if(found < 0) return KITF_RANK_NORMAL;
	return KITF_RankNormalize(found);
}

int ServerManager::accountRank(const char *username)
{
	bool dummy = false;
	return accountRankStatus(username,dummy);
}

bool ServerManager::setAccountRank(const char *username, int rank)
{
	rank = KITF_RankNormalize(rank);
	const string path = resolveUserFile(username);
	if(path.empty()) return false;
	std::ifstream inFile(path.c_str(),std::ios::binary);
	if(!inFile.good()) return false;
	vector<string> lines;
	string line;
	while(std::getline(inFile,line))
	{
		if(!line.empty() && line[line.size()-1]=='\r') line.erase(line.size()-1);
		lines.push_back(line);
	}
	inFile.close();
	// A plain set clears any expiry timer (use setAccountRankExp for timed).
	vector<string> out;
	out.reserve(lines.size()+1);
	bool replaced = false;
	for(size_t i = 0; i < lines.size(); i++)
	{
		if(isRankExpLine(lines[i]) || isRankExpLine(XOR7(lines[i]))) continue;
		int v = rankLineValue(lines[i]);
		if(v == -2) v = rankLineValue(XOR7(lines[i]));
		if(v != -2)
		{
			out.push_back(XOR7(string("rank=") + KITF_RankName(rank)));
			replaced = true;
		}
		else out.push_back(lines[i]);
	}
	if(!replaced) out.push_back(XOR7(string("rank=") + KITF_RankName(rank)));
	lines.swap(out);
	std::ofstream outFile(path.c_str(),std::ios::binary | std::ios::trunc);
	if(!outFile.good()) return false;
	for(size_t i = 0; i < lines.size(); i++)
	{
		outFile.write(lines[i].c_str(),lines[i].length());
		outFile.write("\n",1);
	}
	outFile.close();
	return true;
}

// Lowercase username bound to a client address at ID_LOGON ("" if none).
string ServerManager::sessionUserFor(const SystemAddress &addr)
{
	for(vector<pair<string,SystemAddress> >::const_iterator it = loginSession.begin(); it != loginSession.end(); it++)
	{
		if(it->second == addr) return it->first;
	}
	return "";
}

SystemAddress ServerManager::sessionAddressForName(const string &lowerName)
{
	for(vector<pair<string,SystemAddress> >::const_iterator it = loginSession.begin(); it != loginSession.end(); it++)
	{
		if(it->first == lowerName) return it->second;
	}
	return UNASSIGNED_SYSTEM_ADDRESS;
}

// Best-effort live rank push: if the account is online on the login
// server right now, its client applies the new rank immediately
// (no relog), and every game server hosting the player syncs its
// stored authority rank for per-command auth. The file write is
// authoritative; offline players pick the rank up at the next
// ID_LOADCHAR / ID_TOKENCONNECTED.
void ServerManager::pushRankToSession(const string &lowerName, int rank)
{
	rank = KITF_RankNormalize(rank);
	const SystemAddress tgt = sessionAddressForName(lowerName);
	if(tgt != UNASSIGNED_SYSTEM_ADDRESS)
	{
		RakNet::BitStream tBitStream;
		tBitStream.Write(MessageID(ID_SETRANK));
		stringCompressor->EncodeString(KITF_RankCode(rank),8,&tBitStream);
		server->Send(&tBitStream, HIGH_PRIORITY, RELIABLE, 1, tgt, false);
	}
	// Sync game servers: layout [address][rankcode] so the game knows
	// WHICH client slot to update (it never forwards this to players).
	bool tSent[MAX_SERVERS];
	for(int i = 0; i < MAX_SERVERS; i++) tSent[i] = false;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(playerToken[i].add == UNASSIGNED_SYSTEM_ADDRESS) continue;
		if(toLowerCase(playerToken[i].name) != lowerName) continue;
		const unsigned char tServerID = playerToken[i].serverID;
		if(tServerID >= MAX_SERVERS) continue;
		if(serverTunnelAdd[tServerID] == UNASSIGNED_SYSTEM_ADDRESS) continue;
		if(tSent[tServerID]) continue;
		tSent[tServerID] = true;
		RakNet::BitStream tBitStream;
		tBitStream.Write(MessageID(ID_SETRANK));
		tBitStream.Write(playerToken[i].add);
		stringCompressor->EncodeString(KITF_RankCode(rank),8,&tBitStream);
		server->Send(&tBitStream, HIGH_PRIORITY, RELIABLE, 0, serverTunnelAdd[tServerID], false);
	}
}

#if defined(_WIN32)
// --------------------------------------------------------------------------
// HTTP account-registration listener (embedded, no MySQL).
//   POST /register   body: user=<u>&pass=<p>&email=<e>   (form-url-encoded)
//   Responses: 200 body "OK" or "ERR|<code>"
// --------------------------------------------------------------------------

static string KITF_UrlDecode(const string &in){
	string out;
	out.reserve(in.size());
	for(size_t i=0;i<in.size();i++){
		if(in[i]=='+'){out+=' ';continue;}
		if(in[i]=='%'&&i+2<in.size()){
			auto hd=[](char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return 0;};
			out+=(char)((hd(in[i+1])<<4)|hd(in[i+2]));i+=2;continue;
		}
		out+=in[i];
	}
	return out;
}

static string KITF_FormField(const string &body, const string &key){
	string needle="&"+key+"=";
	size_t pos=body.find(needle);
	if(pos==string::npos){needle=key+"=";pos=body.find(needle);if(pos==0){pos=0;}else return "";}
	size_t vp=pos+needle.size();
	size_t end=body.find('&',vp);
	if(end==string::npos)end=body.size();
	return KITF_UrlDecode(body.substr(vp,end-vp));
}

// Timed ranks (e.g. VIP for X days): extra account-file line
// "rankexp=<unix epoch>", XOR7-encoded like the rank line. Absent = permanent.
bool ServerManager::isRankExpLine(const string &line)
{
	const string prefix = "rankexp=";
	return line.compare(0,prefix.size(),prefix)==0;
}

time_t ServerManager::rankExpValue(const string &line)
{
	const string prefix = "rankexp=";
	if(line.compare(0,prefix.size(),prefix)!=0) return 0;
	long long v = atoll(line.substr(prefix.size()).c_str());
	if(v<=0) return 0;
	return (time_t)v;
}

// 0 = no (valid) expiry stored.
time_t ServerManager::accountRankExpiry(const char *username)
{
	const string path = resolveUserFile(username);
	if(path.empty()) return 0;
	std::ifstream inFile(path.c_str(),std::ios::binary);
	if(!inFile.good()) return 0;
	string line;
	time_t found = 0;
	while(std::getline(inFile,line))
	{
		if(!line.empty() && line[line.size()-1]=='\r') line.erase(line.size()-1);
		time_t v = rankExpValue(line);
		if(v==0) v = rankExpValue(XOR7(line));
		if(v!=0) found = v;
	}
	inFile.close();
	return found;
}

// Set rank WITH expiry (overwrites any previous timer).
bool ServerManager::setAccountRankExp(const char *username, int rank, time_t exp)
{
	rank = KITF_RankNormalize(rank);
	const string path = resolveUserFile(username);
	if(path.empty()) return false;
	std::ifstream inFile(path.c_str(),std::ios::binary);
	if(!inFile.good()) return false;
	vector<string> lines;
	string line;
	while(std::getline(inFile,line))
	{
		if(!line.empty() && line[line.size()-1]=='\r') line.erase(line.size()-1);
		lines.push_back(line);
	}
	inFile.close();
	char tExpBuf[32] = "";
	sprintf(tExpBuf,"rankexp=%lld",(long long)exp);
	const string tExpLine = XOR7(string(tExpBuf));
	vector<string> out;
	out.reserve(lines.size()+2);
	bool rRep = false, eRep = false;
	for(size_t i = 0; i < lines.size(); i++)
	{
		if(isRankExpLine(lines[i]) || isRankExpLine(XOR7(lines[i])))
		{
			out.push_back(tExpLine);
			eRep = true;
			continue;
		}
		int v = rankLineValue(lines[i]);
		if(v == -2) v = rankLineValue(XOR7(lines[i]));
		if(v != -2)
		{
			out.push_back(XOR7(string("rank=") + KITF_RankName(rank)));
			rRep = true;
		}
		else out.push_back(lines[i]);
	}
	if(!rRep) out.push_back(XOR7(string("rank=") + KITF_RankName(rank)));
	if(!eRep) out.push_back(tExpLine);
	std::ofstream outFile(path.c_str(),std::ios::binary | std::ios::trunc);
	if(!outFile.good()) return false;
	for(size_t i = 0; i < out.size(); i++)
	{
		outFile.write(out[i].c_str(),out[i].length());
		outFile.write("\n",1);
	}
	outFile.close();
	return true;
}

// Authoritative rank: same as accountRank, but an expired timer lazily
// reverts the account to normal (file rewritten, caller pushes live).
// Returns -1 when the account file is missing.
int ServerManager::effectiveAccountRank(const char *username)
{
	bool tHas = false;
	const int tRank = accountRankStatus(username,tHas);
	if(tRank < 0) return -1;
	if(tRank == KITF_RANK_NORMAL) return KITF_RANK_NORMAL;
	const time_t tExp = accountRankExpiry(username);
	if(tExp<=0) return tRank;
	time_t tNow;
	time(&tNow);
	if(tExp > tNow) return tRank;
	if(setAccountRank(username,KITF_RANK_NORMAL))
	{
		printf("[RANK] Timed rank expired for '%s' (%s) -> normal.\n",username,KITF_RankName(tRank));
		ofstream slog("security.log",ios::app);
		if(slog.good())slog<<currentDateTime()<<" - RANKEXPIRE - "<<username<<" reverted to normal\n";
		slog.close();
	}
	return KITF_RANK_NORMAL;
}

// Called every server loop (throttled to ~60s): downgrade online players
// whose timers elapsed and push the change live (no relog needed).
void ServerManager::sweepExpiredRanks()
{
	time_t tNow;
	time(&tNow);
	if(lastRankSweep!=0 && difftime(tNow,lastRankSweep)<60) return;
	lastRankSweep = tNow;
	for(vector<pair<string,SystemAddress> >::const_iterator it = loginSession.begin(); it != loginSession.end(); it++)
	{
		const string tUser = it->first;
		const time_t tExp = accountRankExpiry(tUser.c_str());
		if(tExp<=0 || tExp>tNow) continue;
		bool tHas = false;
		const int tCur = accountRankStatus(tUser.c_str(),tHas);
		if(tCur<=KITF_RANK_NORMAL) continue;
		if(setAccountRank(tUser.c_str(),KITF_RANK_NORMAL))
		{
			printf("[RANK] Timed rank expired for '%s' (%s) -> normal.\n",tUser.c_str(),KITF_RankName(tCur));
			ofstream slog("security.log",ios::app);
			if(slog.good())slog<<currentDateTime()<<" - RANKEXPIRE - "<<tUser<<" reverted to normal\n";
			slog.close();
			pushRankToSession(tUser,KITF_RANK_NORMAL);
		}
	}
}

bool ServerManager::writeSecureUser(const string &username, const string &password, const string &email){
	string salt=KITF_RandomSaltHex(16); // 32 hex chars
	string digest=KITF_SHA256(salt+password);
	ofstream outFile(getFilename(username.c_str(),".user").c_str(),ios::binary);
	if(!outFile.good())return false;
	string buf="HASH$"+salt+"$"+digest+"\n";
	outFile.write(buf.c_str(),buf.length());
	buf=XOR7(email)+"\n";
	outFile.write(buf.c_str(),buf.length());
	buf=XOR7(string(""))+"\n";
	outFile.write(buf.c_str(),buf.length());
	buf=XOR7(string(""))+"\n";
	outFile.write(buf.c_str(),buf.length());
	// New accounts are always normal users; promotion is via /setadmin only.
	buf=XOR7(string("rank=normal"))+"\n";
	outFile.write(buf.c_str(),buf.length());
	outFile.close();
	return true;
}

void ServerManager::startHttpRegister(){
#if defined(_WIN32)
	if(REGISTER_PORT==0){puts("HTTP register endpoint disabled (REGISTER_PORT=0).");return;}
	WSADATA wsa;
	if(WSAStartup(MAKEWORD(2,2),&wsa)!=0){printf("WSAStartup failed for register endpoint.\n");return;}
	httpListenSock=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if(httpListenSock==INVALID_SOCKET){printf("register socket failed.\n");WSACleanup();return;}
	{int v=1;setsockopt(httpListenSock,SOL_SOCKET,SO_REUSEADDR,(const char*)&v,sizeof(v));}
	sockaddr_in addr;
	memset(&addr,0,sizeof(addr));
	addr.sin_family=AF_INET;
	addr.sin_addr.s_addr=htonl(INADDR_ANY);
	addr.sin_port=htons((u_short)REGISTER_PORT);
	if(bind(httpListenSock,(sockaddr*)&addr,sizeof(addr))==SOCKET_ERROR){
		printf("register bind to port %d failed (in use?).\n",REGISTER_PORT);
		closesocket(httpListenSock);httpListenSock=INVALID_SOCKET;WSACleanup();return;
	}
	if(listen(httpListenSock,SOMAXCONN)==SOCKET_ERROR){
		printf("register listen failed on port %d.\n",REGISTER_PORT);
		closesocket(httpListenSock);httpListenSock=INVALID_SOCKET;WSACleanup();return;
	}
	httpThreadH=(HANDLE)_beginthreadex(0,0,httpThread,this,0,0);
	printf("Account registration HTTP endpoint listening on port %d.\n",REGISTER_PORT);
#else
	printf("HTTP register endpoint not supported on this platform.\n");
#endif
}

unsigned __stdcall ServerManager::httpThread(void *arg){
	ServerManager *self=(ServerManager*)arg;
	while(self->httpListenSock!=INVALID_SOCKET){
		sockaddr_in cli;
		int cliLen=sizeof(cli);
		SOCKET c=accept(self->httpListenSock,(sockaddr*)&cli,&cliLen);
		if(c==INVALID_SOCKET)break;
		self->handleHttpConnection(c);
	}
	return 0;
}

static void sendHttpBytes(SOCKET client,const string &status,const string &ctype,
	const char *data,size_t len){
	string head="HTTP/1.1 "+status+"\r\nContent-Type: "+ctype+
		"\r\nContent-Length: "+to_string((int)len)+"\r\nConnection: close\r\n\r\n";
	send(client,head.c_str(),(int)head.length(),0);
	size_t off=0;
	while(off<len){
		size_t chunk=len-off;if(chunk>65536)chunk=65536;
		int s=send(client,data+off,(int)chunk,0);
		if(s<=0)break;
		off+=(size_t)s;
	}
	closesocket(client);
}
void ServerManager::handleHttpConnection(SOCKET client){
	char req[16384]="";
	// Capture the connecting client's address for request logging.
	string peerIP="unknown";
	{
		sockaddr_in peer; int plen=sizeof(peer);
		if(getpeername(client,(sockaddr*)&peer,&plen)==0)
			peerIP=inet_ntoa(peer.sin_addr);
	}
	// Read the whole request, honoring Content-Length, so the POST body (which can
	// arrive in a later TCP segment than the headers) is always captured.
	string raw;
	unsigned long contentLength=0;
	for(int guard=0; guard<64; guard++){
		char buf[4096];
		int n=recv(client,buf,sizeof(buf),0);
		if(n<=0)break;
		raw.append(buf,n);
		if(contentLength==0){
			size_t hl=raw.find("\r\n\r\n");
			if(hl!=string::npos){
				string hdr=raw.substr(0,hl);
				size_t clp=hdr.find("\r\nContent-Length:");
				if(clp!=string::npos){
					size_t vs=clp+18; size_t ve=hdr.find("\r\n",vs);
					if(ve==string::npos)ve=hdr.size();
					contentLength=(unsigned long)atoll(hdr.substr(vs,ve-vs).c_str());
				}
				// we have full header; body starts at hl+4
				size_t have=raw.size()>hl+4?raw.size()-hl-4:0;
				if(have>=contentLength)break;
			}else continue; // headers not complete yet
		}else{
			size_t hl=raw.find("\r\n\r\n");
			size_t bodyStart=hl+4;
			if(raw.size()>=bodyStart+contentLength)break;
		}
	}
	strncpy(req,raw.c_str(),sizeof(req)-1);req[sizeof(req)-1]=0;
	// If the HTTP server sits behind a reverse proxy / CDN, use X-Forwarded-For
	// (leftmost entry) as the real client IP instead of the proxy's socket IP.
	{
		string hdrs=raw;
		size_t hend=hdrs.find("\r\n\r\n");
		if(hend!=string::npos)hdrs=hdrs.substr(0,hend);
		else{hend=hdrs.find("\n\n");if(hend!=string::npos)hdrs=hdrs.substr(0,hend);}
		for(size_t i=0;i<hdrs.size();i++)if(hdrs[i]>='A'&&hdrs[i]<='Z')hdrs[i]+=' ';
		size_t xfp=hdrs.find("\r\nx-forwarded-for:");
		if(xfp==string::npos)xfp=hdrs.find("x-forwarded-for:");
		if(xfp!=string::npos){
			xfp=hdrs.find(':',xfp)+1;
			while(xfp<hdrs.size()&&(hdrs[xfp]==' '||hdrs[xfp]=='\t'))xfp++;
			size_t xfe=xfp;
			while(xfe<hdrs.size()&&hdrs[xfe]!=','&&hdrs[xfe]!='\r'&&hdrs[xfe]!='\n')xfe++;
			string xip=hdrs.substr(xfp,xfe-xfp);
			while(!xip.empty()&&(xip.back()==' '||xip.back()=='\t'))xip.pop_back();
			if(!xip.empty())peerIP=xip;
		}
	}
	int n=(int)raw.size();
	string answerBody="ERR|BADREQ";
	string outStatus="500 Internal Server Error";
	if(n>0){
		string r(raw);
		// Parse request line: METHOD SP PATH SP HTTP/1.x
		size_t sp=r.find(' ');
		size_t sp2 = (sp==string::npos)? string::npos : r.find(' ',sp+1);
		string method = (sp==string::npos)? "" : r.substr(0,sp);
		string path   = (sp2==string::npos)? "" : r.substr(sp+1,sp2-sp-1);
		// Log launcher/updater check-ins (the control requests the launcher makes
		// at startup / update-check) so the operator can see incoming connections.
		{
			string lc=path;
			if(!lc.empty()&&lc[0]=='/')lc.erase(0,1);
			if(lc.find('?')!=string::npos)lc=lc.substr(0,lc.find('?'));
			if(lc=="info.txt")
				printf("[CONNECT] Launcher check-in from %s (startup / update check).\n",peerIP.c_str());
			else if(lc=="banlist.txt")
				printf("[CONNECT] Ban check from %s.\n",peerIP.c_str());
			else if(lc=="update.txt"||lc=="launcher.cfg")
				printf("[CONNECT] Update manifest fetch from %s.\n",peerIP.c_str());
		}
		// Parse body (after \r\n\r\n or \n\n)
		size_t hd=r.find("\r\n\r\n");
		string body;
		if(hd==string::npos)hd=r.find("\n\n");
		if(hd!=string::npos)body=r.substr(hd+(r[hd]=='\r'?4:2));

		// Serve update files to the launcher's updater via GET.
		if(method=="GET"){
			if(path.empty()||path[0]!='/')path="/";
			if(path.find('?')!=string::npos)path=path.substr(0,path.find('?'));
			string rel=KITF_UrlDecode(path.substr(1));
			if(rel=="register"||rel=="register/")
				return sendHttpBytes(client,"404 Not Found","text/plain","Not Found",9);
			// Prevent traversal outside the update folder.
			string updateRoot=UPDATE_FILES_DIR;
			if(!updateRoot.empty()&&updateRoot.back()!='\\')updateRoot+='\\';
			string lc=rel;
			for(size_t i=0;i<lc.size();i++)if(lc[i]=='/')lc[i]='\\';
			if(lc.find("..")!=string::npos||rel.find("..\0")!=string::npos)
				return sendHttpBytes(client,"403 Forbidden","text/plain","Forbidden",9);
			string fp=updateRoot+lc;
			std::ifstream in(fp.c_str(),ios::binary|ios::ate);
			if(!in.good())
				return sendHttpBytes(client,"404 Not Found","text/plain","Not Found",9);
			streampos szp=in.tellg();
			if(szp<0)return sendHttpBytes(client,"500 Internal Server Error","text/plain","Error",5);
			size_t fsz=(size_t)szp;
			in.seekg(0,ios::beg);
			vector<char> buf(fsz);
			if(fsz>0)in.read(buf.data(),fsz);
			in.close();
			string ctype="application/octet-stream";
			if(lc=="update.txt"||lc=="info.txt"||lc=="banlist.txt"||lc=="launcher.cfg"||lc=="version.txt")ctype="text/plain";
			return sendHttpBytes(client,"200 OK",ctype,buf.data(),fsz);
		}
		if(method=="POST" && (path=="/register" || path=="/register/")){
			string user=KITF_FormField(body,"user");
			string pass=KITF_FormField(body,"pass");
			string email=KITF_FormField(body,"email");
			printf("[REGISTER] Incoming register request for '%s' from %s.\n",user.c_str(),peerIP.c_str());
			// Validation
			bool okUser=!user.empty()&&user.size()<=15;
			for(size_t i=0;i<user.size()&&okUser;i++){
				char c=user[i];
				if(!( (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_' ))okUser=false;
			}
			bool okPass=pass.size()>=6 && pass.size()<=15;
			bool okEmail=email.size()<=63;
			// disallow benign fishing: HASH line length limits
			if(!okUser){answerBody="ERR|BUSER";}
			else if(!okPass){answerBody="ERR|BPASS";}
			else if(!okEmail){answerBody="ERR|BEMAIL";}
			else{
				// username uniqueness (mirror ID_CREATEACCOUNT checks)
				bool exists=false;
				ifstream inFile(getFilename(user.c_str(),".user").c_str());
				if(inFile.good())exists=true;
				else{
					inFile.clear();
					inFile.open(getFilename(user.c_str(),".user",false).c_str());
					if(inFile.good())exists=true;
					else{
						inFile.clear();
						inFile.open(getFilename(user.c_str(),".user",false,true).c_str());
						if(inFile.good())exists=true;
					}
				}
				if(exists)answerBody="ERR|TAKEN";
				else if(writeSecureUser(user,pass,email)){
					answerBody="OK";
					ofstream slog("security.log",ios::app);
					if(slog.good())slog<<currentDateTime()<<" - REGISTER - "<<user<<"\n";
					slog.close();
					printf("[REGISTER] New account created: '%s'.\n",user.c_str());
				}else answerBody="ERR|WRITE";
				printf("[REGISTER] '%s' attempt -> %s\n",user.c_str(),answerBody.c_str());
			}
			outStatus="200 OK";
		}
		if(method=="POST" && (path=="/delete" || path=="/delete/")){
			string user=KITF_FormField(body,"user");
			string pass=KITF_FormField(body,"pass");
			printf("[DELETE] Incoming delete request for '%s' from %s.\n",user.c_str(),peerIP.c_str());
			// Sanitize like the login path so file lookups stay inside Data/
			string userSafe=user;
			{
				string safe;
				for(size_t i=0;i<userSafe.size();i++){
					unsigned char ch=(unsigned char)userSafe[i];
					bool ok=(ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')||ch=='_';
					if(ok)safe+=(char)ch;
				}
				userSafe=safe.substr(0,15);
			}
			if(userSafe.empty()){answerBody="ERR|BADREQ";}
			else{
				// locate account file (HASH$ secured)
				string fpath=getFilename(userSafe.c_str(),".user");
				bool found=false;
				{
					std::ifstream inFile(fpath.c_str(),std::ios::binary);
					if(inFile.good())found=true;
					inFile.close();
				}
				if(!found)answerBody="ERR|NOACCOUNT";
				else{
					remove(fpath.c_str());
					remove(getFilename(userSafe.c_str(),".charlist").c_str());
					remove(getFilename(userSafe.c_str(),".server").c_str());
					answerBody="OK";
					ofstream dlog("security.log",ios::app);
					if(dlog.good())dlog<<currentDateTime()<<" - DELETE - "<<userSafe<<"\n";
					dlog.close();
					printf("[DELETE] Account '%s' deleted.\n",userSafe.c_str());
				}
			}
			outStatus="200 OK";
		}
	}
	string resp="HTTP/1.1 "+outStatus+"\r\nContent-Type: text/plain\r\nContent-Length: "+to_string((int)answerBody.length())+"\r\nConnection: close\r\n\r\n"+answerBody;
	send(client,resp.c_str(),(int)resp.length(),0);
	closesocket(client);
}
#endif

int main(void)
{
	setvbuf(stdout,NULL,_IONBF,0); // unbuffered: log lines appear live in console or file
	setvbuf(stderr,NULL,_IONBF,0);
	ServerManager mServerMgr;

	bool inited = false;
	while(!inited)
	{
		inited = mServerMgr.initialize();
		if(!inited)RakSleep(5000);
	}

	mServerMgr.runLoop();
	mServerMgr.shutdown();

	return 0;
}

// Copied from Multiplayer.cpp
// If the first byte is ID_TIMESTAMP, then we want the 5th byte
// Otherwise we want the 1st byte
MessageID GetPacketIdentifier(Packet *p)
{
	if (p==0)
		return 255;

	if ((unsigned char)p->data[0] == ID_TIMESTAMP)
	{
		assert(p->length > sizeof(unsigned char) + sizeof(unsigned long));
		return (unsigned char) p->data[sizeof(unsigned char) + sizeof(unsigned long)];
	}
	else
		return (unsigned char) p->data[0];
}

