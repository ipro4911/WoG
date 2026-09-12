// KITFLauncher.cpp - login/news launcher for Wyvern of Guardians.
// Features: auto-update check, manual re-check, ban check, register, music.
#define WIN32_LEAN_AND_MEAN
#include "kitf_common.h"
#include <commctrl.h>
#include <process.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

static const char* DEFAULT_URL = "http://26.114.82.92:40020/";
static std::string gBaseUrl = DEFAULT_URL;
static std::string gGameExe = "ogremagix.exe";
static std::string gTargetDir = ".";
static std::string gBgmFile = "launcher_bgm.mp3";
static std::string gRegHost = "26.114.82.92";
static int gRegPort = 40020;

static HWND gStatus,gUser,gPass,gBtnPlay,gBtnUpdate,gBtnRegister,gBtnMusic,gWnd;
static HWND gRegUser,gRegPass,gRegEmail,gRegStatus,gRegWnd,gDelUser,gDelStatus,gDelWnd;
static bool gClientUpToDate=false;
static volatile LONG gCheckBusy=0;
static std::string gBanlistRaw;
static bool gMciPlaying=false;
static bool gMusicEnabled=true;
enum {WM_APP_NEWS=WM_APP+1,WM_APP_DONE=WM_APP+2,WM_APP_VER=WM_APP+3};
#define UPDATE_POLL_TIMER 1001
#define UPDATE_POLL_MS 60000

static void loadConfig(){
	FILE*f;
	if(fopen_s(&f,"launcher.cfg","r")==0){
		char line[512];
		while(fgets(line,sizeof(line),f)){
			std::string s=line;while(!s.empty()&&(s.back()=='\n'||s.back()=='\r'))s.pop_back();
			if(s.rfind("updateurl=",0)==0)gBaseUrl=s.substr(10);
			else if(s.rfind("gameexe=",0)==0)gGameExe=s.substr(8);
			else if(s.rfind("bgm=",0)==0)gBgmFile=s.substr(4);
			else if(s.rfind("music=",0)==0)gMusicEnabled=(s.substr(6)!="off"&&s.substr(6)!="0");
			else if(s.rfind("registerhost=",0)==0){
				std::string sv=s.substr(13);size_t c=sv.find(':');
				if(c!=std::string::npos){gRegHost=sv.substr(0,c);gRegPort=atoi(sv.substr(c+1).c_str());}
				else gRegHost=sv;
			}
		}
		fclose(f);
	}
	if(gBaseUrl.empty()||gBaseUrl.back()!='/')gBaseUrl+='/';
	gTargetDir=".";
	// Local-only UI overrides (never published, never overwritten by update).
	{FILE*f;
	if(fopen_s(&f,"launcher.local","r")==0){
		char line[512];
		while(fgets(line,sizeof(line),f)){
			std::string s=line;while(!s.empty()&&(s.back()=='\n'||s.back()=='\r'))s.pop_back();
			if(s.rfind("music=",0)==0)gMusicEnabled=(s.substr(6)!="off"&&s.substr(6)!="0");
		}
		fclose(f);
	}}
}
static void setStatus(const std::string& s){SetWindowTextA(gStatus,s.c_str());}
static std::string cacheBusted(const std::string& url){
	// WinINet may serve a cached manifest after the server restarts/publishes;
	// the server ignores the query string, so this forces a fresh fetch.
	char b[32];sprintf_s(b,"?t=%lu",(unsigned long)GetTickCount());
	return url+b;
}
static std::string trimVer(std::string s){
	while(!s.empty()&&(s.back()=='\n'||s.back()=='\r'||s.back()==' '||s.back()=='\t'))s.pop_back();
	size_t st=s.find_first_not_of(" \t\r\n");if(st!=std::string::npos)s=s.substr(st);else s="";
	return s;
}
static std::string readLocalVersion(){
	std::string lv;
	FILE*f;if(fopen_s(&f,"version.txt","r")!=0)return lv;
	char b[64];if(fgets(b,sizeof(b),f))lv=b;
	fclose(f);
	return trimVer(lv);
}
// Background version poll (runs on a worker thread from WM_TIMER): if the
// server's version.txt drifted from ours while the launcher sat open, run
// the normal update check (which auto-updates, same as at startup).
static unsigned __stdcall checkThread(void*);
static void pollVersion(){
	if(InterlockedCompareExchange(&gCheckBusy,0,0)!=0)return; // busy: skip this round
	std::string rv;
	if(!httpGet(cacheBusted(gBaseUrl+"version.txt"),rv))return; // offline: stay silent
	rv=trimVer(rv);
	if(rv.empty())return;
	if(rv!=readLocalVersion()){
		setStatus("Update available - checking...");
		gClientUpToDate=false;
		_beginthreadex(0,0,checkThread,0,0,0);
	}
}
static unsigned __stdcall pollThread(void*){pollVersion();return 0;}

static void stopMusic(){
	// Always issue stop+close, even if our flag desynced from MCI state.
	// A flag-only stop is exactly how "toggle off doesn't stop" happens.
	// The final "close all" is a hammer for decoders that ignore stop/close
	// on a repeat-play alias (observed with some MPEGVideo backends).
	mciSendStringA("stop bgm",0,0,0);
	mciSendStringA("close bgm",0,0,0);
	mciSendStringA("close all",0,0,0);
	gMciPlaying=false;
}
static void saveMusicSetting(){
	// Persist the toggle in launcher.local (NOT launcher.cfg: that file is
	// synced from the server, so writing the pref there would fight every
	// update and reset on each download).
	FILE*f;
	if(fopen_s(&f,"launcher.local","w")==0){
		fputs(gMusicEnabled?"music=on\n":"music=off\n",f);
		fclose(f);
	}
}
static void startMusic(){
	if(!gMusicEnabled||gBgmFile.empty())return;
	stopMusic(); // drop any stale alias before opening (e.g. rapid re-toggle)
	std::string path=gTargetDir+"\\"+gBgmFile;
	if(!fileExists(path))path=gBgmFile;
	if(!fileExists(path))return;
	std::string cmd="open \""+path+"\" type MPEGVideo alias bgm";
	if(mciSendStringA(cmd.c_str(),0,0,0)==0){
		mciSendStringA("setaudio bgm volume to 500",0,0,0);
		if(mciSendStringA("play bgm repeat",0,0,0)==0)gMciPlaying=true;
	}
}
static void writeSettings2Pass(const std::string& pass){
	std::string enc;for(size_t i=0;i<pass.size();i++){
		unsigned char c=(unsigned char)pass[i];c^=(unsigned char)(((i%7)*6+7)+(i%7));
		if(c>126||c<32)c=(unsigned char)pass[i];enc+=(char)c;}
	FILE*f;if(fopen_s(&f,"Settings2.dat","wb")==0){fwrite(enc.data(),1,enc.size(),f);fclose(f);}
}
static void writeSettingsUser(const std::string& user){
	std::vector<std::string> lines;
	{std::ifstream in("Settings.cfg");std::string l;while(std::getline(in,l))lines.push_back(l);}
	char lb[64];sprintf_s(lb,"%s",user.c_str()); // bare value: the game reads line 8 verbatim (saveSettings format), "username=" prefix would become part of the name
	if(lines.size()<8)lines.resize(8);lines[7]=lb;
	std::ofstream out("Settings.cfg");for(auto&l:lines)out<<l<<"\n";out.close();
}
static std::string toLowerStr(const std::string& s){
	std::string r=s;for(auto&c:r)if(c>='A'&&c<='Z')c=c-'A'+'a';return r;}
static bool isUserBanned(const std::string& username){
	std::string low=toLowerStr(username);
	std::istringstream iss(gBanlistRaw);std::string line;
	while(std::getline(iss,line)){
		while(!line.empty()&&(line.back()=='\n'||line.back()=='\r'))line.pop_back();
		if(line.empty()||line[0]=='#')continue;
		std::string name=line;size_t sp=line.find(' ');if(sp!=std::string::npos)name=line.substr(0,sp);
		if(toLowerStr(name)==low)return true;}
	return false;}
static void writeLocalIP(){
	// Keep the game client's localIP.txt override in sync with launcher.cfg's
	// registerhost, so IP changes propagate via update without rebuilding the exe.
	if(gRegHost.empty())return;
	if(gRegHost=="127.0.0.1"||gRegHost=="localhost"){
		// Local testing: remove override so the exe falls back to compiled MAIN_SERVER_IP.
		remove("localIP.txt");return;
	}
	FILE*f;if(fopen_s(&f,"localIP.txt","w")==0){fputs(gRegHost.c_str(),f);fclose(f);}
}
static bool launchGame(const std::string& user,const std::string& pass){
	writeSettingsUser(user);writeSettings2Pass(pass);writeLocalIP();stopMusic();
	// No --autochar: always land on the character screen so the player
	// picks (or creates) a character instead of auto-starting the first.
	std::string cmd="\""+gGameExe+"\" --user "+user+" --pass "+pass+" --auto";
	STARTUPINFOA si={sizeof(si)};PROCESS_INFORMATION pi={0};
	if(!CreateProcessA(0,(LPSTR)cmd.c_str(),0,0,FALSE,0,0,0,&si,&pi)){
		startMusic();setStatus("Failed to launch the game.");return false;}
	CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return true;}
static unsigned __int64 freeSpaceMB(const std::string& path){
	ULARGE_INTEGER free,total,totalfree;free.QuadPart=0;total.QuadPart=0;totalfree.QuadPart=0;
	std::string root=gTargetDir;
	GetDiskFreeSpaceExA(root.c_str(),&free,&total,&totalfree);
	return free.QuadPart/1024/1024;}
static bool requiredFilesPresent(){
	const char* req[]={"ogremagix.exe","fmodex64.dll","OgreMain.dll","resources.cfg",0};
	for(int i=0;req[i];i++){
		if(!fileExists(gTargetDir+"\\"+req[i])&&!fileExists(req[i]))return false;}
	return true;}
static void setStatusThread(const std::string& s);
static void doUpdate();
static unsigned __stdcall checkThread(void*){
	if(InterlockedCompareExchange(&gCheckBusy,1,0)!=0)return 0;
	gClientUpToDate=false;setStatus("Checking for updates...");
	{std::string bl;httpGet(gBaseUrl+"banlist.txt",bl);gBanlistRaw=bl;}
	std::string status;int outdated=0;bool manOk=false;std::string latest="unknown";
	{std::string manifest;unsigned long st=0;
	if(httpGet(cacheBusted(gBaseUrl+"update.txt"),manifest,&st)){
		Manifest mf;std::string base=gBaseUrl;
		if(!base.empty()&&base.back()=='/')base.pop_back();
		// Hoisted out of the parse block so the tail logic below can see it.
		// The running exe can't replace itself mid-run: track it separately
		// so the user gets a restart prompt instead of silent staleness.
		bool launcherOutdated=false;
		if(mf.parse(manifest,base)){manOk=true;if(!mf.appVersion.empty())latest=mf.appVersion;
			bool exeHere=fileExists(gTargetDir+"\\"+gGameExe)||fileExists(gGameExe);
			if(!exeHere)outdated++;
			for(size_t i=0;i<mf.files.size();i++){
				std::string b=mf.files[i].path;
				size_t sl=b.find_last_of("/\\");if(sl!=std::string::npos)b.erase(0,sl+1);
				std::string lp=gTargetDir+"\\"+mf.files[i].path;
				bool same=fileExists(lp)&&fileSize(lp)==mf.files[i].size&&fileMd5(lp)==mf.files[i].md5;
				if(_stricmp(b.c_str(),"KITFLauncher.exe")==0){
					if(!same)launcherOutdated=true;
					continue;
				}
				if(!same)
					outdated++;}}
		if(manOk&&launcherOutdated&&outdated==0){
			status="Launcher update available - updating, then restart.";
			InterlockedExchange(&gCheckBusy,0);
			doUpdate();
			return 0;}}
	if(!manOk)status="Server offline / could not fetch update manifest.";
	else if(!requiredFilesPresent()){ // fresh/empty folder: auto-install all files
		status="Installing client files...";
		InterlockedExchange(&gCheckBusy,0);
		doUpdate();
		return 0;}
	else if(outdated==0)status="Up to date v"+latest;
	else{ // outdated but files exist: auto-update
		status="Client out of date ("+std::to_string(outdated)+" file(s)) - updating...";
		InterlockedExchange(&gCheckBusy,0);
		doUpdate();
		return 0;}}
	gClientUpToDate=manOk&&outdated==0&&latest!="unknown";
	PostMessageA(gWnd,WM_APP_VER,0,(LPARAM)_strdup(status.c_str()));
	PostMessageA(gBtnUpdate,WM_APP_DONE,0,gClientUpToDate?1:0);InterlockedExchange(&gCheckBusy,0);return 0;}
static void setStatusThread(const std::string& s){PostMessageA(gWnd,WM_APP_VER,0,(LPARAM)_strdup(s.c_str()));}
static unsigned __stdcall updateThread(void*){
	if(InterlockedCompareExchange(&gCheckBusy,1,0)!=0){setStatusThread("Update check already in progress.");return 0;}
	// MCI holds launcher_bgm.mp3 open while music plays, which would make
	// its replace fail every time. Stop unconditionally (the gMciPlaying
	// flag can desync from real MCI state) and give MCI a moment to release
	// the file handle before touching anything on disk.
	bool musicWas=gMciPlaying;
	stopMusic();
	Sleep(250);
	unsigned __int64 freeMB=freeSpaceMB(gTargetDir);
	if(freeMB<200){setStatusThread("Not enough free disk space ("+std::to_string(freeMB)+" MB) to update.");if(musicWas)startMusic();InterlockedExchange(&gCheckBusy,0);return 0;}
	std::string baseUrl=gBaseUrl;
	if(!baseUrl.empty()&&baseUrl.back()=='/')baseUrl.pop_back();
	std::string manifest;
	if(!httpGet(cacheBusted(baseUrl+"/update.txt"),manifest)){setStatusThread("Could not fetch update manifest.");if(musicWas)startMusic();InterlockedExchange(&gCheckBusy,0);return 0;}
	Manifest mf;
	if(!mf.parse(manifest,baseUrl)){setStatusThread("Manifest parse failed.");if(musicWas)startMusic();InterlockedExchange(&gCheckBusy,0);return 0;}
	int need=0;
	// The running launcher exe can't overwrite itself: detect its update
	// separately and stage it as KITFLauncher.new.exe + a restart helper.
	const ManifestEntry* launcherEntry=0;
	bool launcherOutdated=false;
	std::string launcherLocal;
	for(auto&e:mf.files){
		std::string b=e.path;
		size_t sl=b.find_last_of("/\\");if(sl!=std::string::npos)b.erase(0,sl+1);
		if(_stricmp(b.c_str(),"KITFLauncher.exe")==0){
			launcherEntry=&e;
			launcherLocal=gTargetDir+"\\"+e.path;
			if(!fileExists(launcherLocal)||fileSize(launcherLocal)!=e.size||fileMd5(launcherLocal)!=e.md5)
				launcherOutdated=true;
			continue;
		}

		std::string local=gTargetDir+"\\"+e.path;
		if(!fileExists(local)||fileSize(local)!=e.size||fileMd5(local)!=e.md5)need++;
	}
	if(need==0&&!launcherOutdated){
		{std::string bl;httpGet(gBaseUrl+"banlist.txt",bl);gBanlistRaw=bl;}
		gClientUpToDate=true;
		setStatusThread("Up to date v"+mf.appVersion);
		if(musicWas)startMusic();
		InterlockedExchange(&gCheckBusy,0);return 0;}
	int done=0,failed=0;
	std::string failedNames; // "file (reason), ..." so failures are diagnosable
	std::string deferredFinal,deferredTemp;
	for(auto&e:mf.files){
		std::string b=e.path;size_t sl=b.find_last_of("/\\");if(sl!=std::string::npos)b.erase(0,sl+1);
		if(_stricmp(b.c_str(),"KITFLauncher.exe")==0)continue; // Ignore launcher self-download

		std::string local=gTargetDir+"\\"+e.path;
		if(fileExists(local)&&fileSize(local)==e.size&&fileMd5(local)==e.md5)continue;
		ensureDirs(local);
		bool isGame=_stricmp(b.c_str(),gGameExe.c_str())==0;
		bool wasMissing=!fileExists(local);
		// Download to temp first and only replace after the checksum
		// verifies: a failed update never deletes a good (or missing-file
		// keeps retrying next run instead of losing data).
		std::string tmpPath=isGame?local+".pkg":local+".new";
		remove(tmpPath.c_str());
		char buf[160];sprintf_s(buf,"%s %s (%d/%d)...",(wasMissing?"Installing":"Updating"),b.c_str(),done+1,need);
		setStatusThread(buf);
		long long got=-1;unsigned long hStat=0;
		for(int attempt=0;attempt<3&&got<0;attempt++)
		{
			if(attempt>0)setStatusThread(std::string("Retrying ")+b+"...");
			got=downloadToFile(e.url,tmpPath,0,0,&hStat);
		}
		if(got<0){failed++;failedNames+=b+" (download), ";remove(tmpPath.c_str());
			{FILE* dbg;if(fopen_s(&dbg,"_kitupdater_dbg.txt","a")==0){fprintf(dbg,"FAIL url=[%s] reason=download got=%lld http=%lu\n",e.url.c_str(),got,hStat);fclose(dbg);}}}
		else if(fileMd5(tmpPath)!=e.md5){
			// Checksum mismatch: retry once (AV scan lock on fresh .exe, or
			// a publish race where the served file changed mid-download).
			// A stale update.txt (build.bat re-staged exes without a
			// republish) fails twice -> real republish needed, not a retry loop.
			std::string firstMd5=fileMd5(tmpPath);
			Sleep(500);
			remove(tmpPath.c_str());
			setStatusThread(std::string("Verifying ")+b+"...");
			got=downloadToFile(e.url,tmpPath,0,0);
			std::string secondMd5=fileMd5(tmpPath);
			{FILE* dbg;if(fopen_s(&dbg,"_kitupdater_dbg.txt","a")==0){fprintf(dbg,"RETRY url=[%s] got=%lld first=[%s] second=[%s] want=[%s]\n",e.url.c_str(),got,firstMd5.c_str(),secondMd5.c_str(),e.md5.c_str());fclose(dbg);}}
			if(got<0||secondMd5!=e.md5){failed++;failedNames+=b+" (checksum), ";remove(tmpPath.c_str());}
			else if(isGame){deferredFinal=local;deferredTemp=tmpPath;}
			else
			{
				SetFileAttributesA(local.c_str(),FILE_ATTRIBUTE_NORMAL);
				bool swapped=false;
				for(int attempt=0;attempt<4&&!swapped;attempt++){
					if(attempt>0)Sleep(300);
					swapped=MoveFileExA(tmpPath.c_str(),local.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
				}
				if(!swapped){failed++;failedNames+=b+" (replace), ";remove(tmpPath.c_str());}
			}
			done++;
			continue;
		}
		else if(isGame){deferredFinal=local;deferredTemp=tmpPath;}
		else
		{
			SetFileAttributesA(local.c_str(),FILE_ATTRIBUTE_NORMAL); // drop read-only so replace works
			bool swapped=false;
			for(int attempt=0;attempt<4&&!swapped;attempt++){
				if(attempt>0)Sleep(300);
				swapped=MoveFileExA(tmpPath.c_str(),local.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
			}
			if(!swapped){failed++;failedNames+=b+" (replace), ";remove(tmpPath.c_str());}
		}
		done++;
	}
	if(!deferredTemp.empty()){
		std::string b=deferredFinal;size_t sl=b.find_last_of("/\\");if(sl!=std::string::npos)b.erase(0,sl+1);
		if(failed>0){remove(deferredTemp.c_str());}
		else{
			SetFileAttributesA(deferredFinal.c_str(),FILE_ATTRIBUTE_NORMAL);
			bool swapped=false;
			for(int attempt=0;attempt<4&&!swapped;attempt++){
				if(attempt>0)Sleep(300);
				swapped=MoveFileExA(deferredTemp.c_str(),deferredFinal.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
			}
			if(!swapped){failed++;failedNames+=b+" (replace, game running?), ";remove(deferredTemp.c_str());}
		}
	}
	std::string tFailDetail;
	if(failed>0&&!failedNames.empty())
	{
		if(failedNames.size()>2&&failedNames.substr(failedNames.size()-2)==", ")failedNames.erase(failedNames.size()-2);
		std::string shown=failedNames;
		if(shown.size()>220)shown=shown.substr(0,220);
		tFailDetail = " (" + shown + ")";
		{FILE* dbg;if(fopen_s(&dbg,"_kitupdater_dbg.txt","a")==0){fprintf(dbg,"UPDATE done=%d failed=%d detail=[%s]\n",done,failed,failedNames.c_str());fclose(dbg);}}
	}
	bool launcherReady=false;
	std::string launcherSwapBat;
	if(launcherOutdated&&launcherEntry){
		// Stage the new launcher next to the running one. The running exe
		// is file-locked, so it cannot be swapped in-process: a helper
		// batch waits for this process to exit, swaps the files, and
		// restarts the new launcher automatically.
		std::string tmpNew=launcherLocal+".new";
		remove(tmpNew.c_str());
		setStatusThread("Updating KITFLauncher.exe...");
		long long got=-1;unsigned long hStat=0;
		for(int attempt=0;attempt<3&&got<0;attempt++){
			if(attempt>0)setStatusThread("Retrying KITFLauncher.exe...");
			got=downloadToFile(launcherEntry->url,tmpNew,0,0,&hStat);
		}
		std::string gotMd5=(got>=0)?fileMd5(tmpNew):"";
		if(got>=0&&gotMd5!=launcherEntry->md5){
			// Same transient causes as data files (AV scan, publish race):
			// one re-download before calling it failed.
			Sleep(500);
			remove(tmpNew.c_str());
			setStatusThread("Verifying KITFLauncher.exe...");
			got=downloadToFile(launcherEntry->url,tmpNew,0,0,&hStat);
			gotMd5=(got>=0)?fileMd5(tmpNew):"";
		}
		{FILE* dbg;if(fopen_s(&dbg,"_kitupdater_dbg.txt","a")==0){fprintf(dbg,"LAUNCHER url=[%s] got=%lld http=%lu md5=[%s] want=[%s]\n",launcherEntry->url.c_str(),got,hStat,gotMd5.c_str(),launcherEntry->md5.c_str());fclose(dbg);}}
		if(got>=0&&gotMd5==launcherEntry->md5){
			// Resolve absolute paths: the helper batch must work no matter
			// what the process working directory is, and paths may contain
			// spaces (Wyvern of Guardians folder). cd /d anchors it.
			std::string absExe=launcherLocal,absNew=tmpNew,absDir="";
			{char full[MAX_PATH];if(GetFullPathNameA(launcherLocal.c_str(),MAX_PATH,full,0))absExe=full;}
			{char full[MAX_PATH];if(GetFullPathNameA(tmpNew.c_str(),MAX_PATH,full,0))absNew=full;}
			{size_t s=absExe.find_last_of("/\\");if(s!=std::string::npos)absDir=absExe.substr(0,s);}
			launcherSwapBat=gTargetDir+"\\update_launcher.bat";
			FILE*bf=0;
			if(fopen_s(&bf,launcherSwapBat.c_str(),"w")==0){
				fprintf(bf,"@echo off\r\n"
					"echo Applying KITFLauncher update...\r\n"
					"cd /d \"%s\"\r\n"
					":waitloop\r\n"
					"del \"%s\" >nul 2>&1\r\n"
					"if exist \"%s\" (ping -n 2 127.0.0.1 >nul & goto waitloop)\r\n"
					"move /y \"%s\" \"%s\" >nul\r\n"
					"if exist \"%s\" (echo Swap failed - file still locked. & pause & exit /b 1)\r\n"
					"echo Launcher updated - restarting...\r\n"
					"start \"\" \"%s\"\r\n"
					"del \"%%~f0\" >nul 2>&1\r\n",
					absDir.c_str(),
					absExe.c_str(),absExe.c_str(),
					absNew.c_str(),absExe.c_str(),
					absNew.c_str(),
					absExe.c_str());
				fclose(bf);
			}
			launcherReady=true;
		}else{
			// Distinguish the two causes: stale update.txt (republish needed)
			// vs server unreachable. Old code always said "(download)".
			failed++;
			if(got<0)failedNames+=std::string("KITFLauncher.exe (download), ");
			else failedNames+=std::string("KITFLauncher.exe (checksum), ");
			remove(tmpNew.c_str());
		}
	}
	if(!tFailDetail.empty()||failed>0){
		// rebuild detail string if the launcher failure appended late
		tFailDetail.clear();
		if(!failedNames.empty()){
			std::string fn=failedNames;
			if(fn.size()>2&&fn.substr(fn.size()-2)==", ")fn.erase(fn.size()-2);
			{FILE* dbg;if(fopen_s(&dbg,"_kitupdater_dbg.txt","a")==0){fprintf(dbg,"UPDATE final failed=%d detail=[%s]\n",failed,fn.c_str());fclose(dbg);}}
			if(fn.size()>220)fn=fn.substr(0,220);
			tFailDetail=" ("+fn+")";
		}
	}
	if(launcherReady){
		// Exit-to-replace: this running exe is file-locked and can never
		// overwrite itself in-process. Close this instance so the helper
		// batch can swap the staged .new file into place, then it reopens
		// the new launcher automatically. Restarts even if data files had
		// failures - the fresh instance re-checks and resumes them.
		if(failed>0)setStatusThread("Launcher updated - restarting to finish...");
		else setStatusThread("Launcher updated - restarting...");
		Sleep(1500); // let the status paint before we go away
		{STARTUPINFOA si={sizeof(si)};PROCESS_INFORMATION pi={0};
		std::string absBat=launcherSwapBat;
		{char full[MAX_PATH];if(GetFullPathNameA(launcherSwapBat.c_str(),MAX_PATH,full,0))absBat=full;}
		std::string cmd="cmd.exe /c \""+absBat+"\"";
		if(CreateProcessA(0,(LPSTR)cmd.c_str(),0,0,FALSE,CREATE_NEW_CONSOLE,0,0,&si,&pi)){
			CloseHandle(pi.hThread);CloseHandle(pi.hProcess);}}
		stopMusic();
		PostMessageA(gWnd,WM_CLOSE,0,0);
		InterlockedExchange(&gCheckBusy,0);return 0;
	}
	{char msg[512];sprintf_s(msg,"Update finished: %d file(s), %d failed%s.",done,failed,tFailDetail.c_str());
	setStatusThread(msg);}
	{std::string bl;httpGet(gBaseUrl+"banlist.txt",bl);gBanlistRaw=bl;}
	gClientUpToDate=(failed==0)&&!launcherReady;
	if(musicWas)startMusic();
	InterlockedExchange(&gCheckBusy,0);return 0;
}
static void doUpdate(){setStatus("Checking for updates...");_beginthreadex(0,0,updateThread,0,0,0);}
static bool gRegDone=false;
static unsigned __stdcall registerThread(void*){
	char ub[64],pb[64],eb[128];
	GetWindowTextA(gRegUser,ub,64);GetWindowTextA(gRegPass,pb,64);GetWindowTextA(gRegEmail,eb,128);
	std::string user=ub,pass=pb,email=eb;
	SetWindowTextA(gRegStatus,"Registering...");
	std::string body="user="+user+"&pass="+pass+"&email="+email;
	std::string url="http://"+gRegHost+":"+std::to_string(gRegPort)+"/register";
	std::string resp;unsigned long st=0;
	if(!httpPost(url,body,resp,&st)){
		SetWindowTextA(gRegStatus,"Login server is offline - cannot register.");gRegDone=true;return 0;}
	std::string msg;
	if(resp=="OK")msg="Registration successful! You can now log in.";
	else if(resp=="ERR|TAKEN")msg="Username already taken.";
	else if(resp=="ERR|BUSER")msg="Invalid username (1-15 chars, letters/numbers/underscores).";
	else if(resp=="ERR|BPASS")msg="Password must be 6-15 characters.";
	else if(resp=="ERR|BEMAIL")msg="Invalid email.";
	else msg="Server error: "+resp;
	SetWindowTextA(gRegStatus,msg.c_str());gRegDone=true;return 0;}
static unsigned __stdcall deleteThread(void*){
	char ub[64];
	GetWindowTextA(gDelUser,ub,64);
	std::string user=ub;
	SetWindowTextA(gDelStatus,"Deleting account...");
	std::string body="user="+user;
	std::string url="http://"+gRegHost+":"+std::to_string(gRegPort)+"/delete";
	std::string resp;unsigned long st=0;
	if(!httpPost(url,body,resp,&st)){
		SetWindowTextA(gDelStatus,"Could not reach server.");return 0;}
	std::string msg;
	if(resp=="OK"){msg="Account deleted.";DestroyWindow(gDelWnd);
		SetWindowTextA(gUser,"");SetWindowTextA(gPass,"");}
	else if(resp=="ERR|NOACCOUNT")msg="No such account.";
	else if(resp=="ERR|BADREQ")msg="Enter a username.";
	else msg="Server error: "+resp;
	SetWindowTextA(gDelStatus,msg.c_str());return 0;}
static LRESULT CALLBACK DelWndProc(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam){
	switch(msg){
	case WM_CREATE:{SetWindowTextA(hWnd,"Delete Account");
		CreateWindowA("STATIC","Enter username to delete:",WS_CHILD|WS_VISIBLE,12,14,180,18,hWnd,0,GetModuleHandle(0),0);
		gDelUser=CreateWindowA("EDIT","",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,12,36,280,22,hWnd,0,GetModuleHandle(0),0);
		gDelStatus=CreateWindowA("STATIC","",WS_CHILD|WS_VISIBLE,12,64,280,26,hWnd,0,GetModuleHandle(0),0);
		CreateWindowA("BUTTON","Delete",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,12,96,90,26,hWnd,(HMENU)2003,GetModuleHandle(0),0);
		CreateWindowA("BUTTON","Cancel",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,110,96,90,26,hWnd,(HMENU)2004,GetModuleHandle(0),0);
		SetFocus(gDelUser);break;}
	case WM_COMMAND:
		if(LOWORD(wParam)==2003){char ub[64];
			GetWindowTextA(gDelUser,ub,64);
			if(!ub[0]){SetWindowTextA(gDelStatus,"Enter a username.");break;}
			_beginthreadex(0,0,deleteThread,0,0,0);}
		else if(LOWORD(wParam)==2004)DestroyWindow(hWnd);break;
	case WM_DESTROY:gDelWnd=0;break;
	default:return DefWindowProcA(hWnd,msg,wParam,lParam);}return 0;}
static void showDeleteDialog(){
	if(gDelWnd){SetForegroundWindow(gDelWnd);return;}
	INITCOMMONCONTROLSEX ie{sizeof(ie),ICC_STANDARD_CLASSES};InitCommonControlsEx(&ie);
	WNDCLASSA wc={0};wc.lpfnWndProc=DelWndProc;wc.hInstance=GetModuleHandle(0);
	wc.hCursor=LoadCursor(0,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);
	wc.lpszClassName="KITFDelWnd";RegisterClassA(&wc);
	gDelWnd=CreateWindowA("KITFDelWnd","Delete Account",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,
		CW_USEDEFAULT,CW_USEDEFAULT,320,175,gWnd,0,GetModuleHandle(0),0);
	if(gDelWnd){ShowWindow(gDelWnd,SW_SHOW);UpdateWindow(gDelWnd);}}
static LRESULT CALLBACK RegWndProc(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam){
	switch(msg){
	case WM_CREATE:{SetWindowTextA(hWnd,"Register Account");
		CreateWindowA("STATIC","Username:",WS_CHILD|WS_VISIBLE,12,12,70,18,hWnd,0,GetModuleHandle(0),0);
		gRegUser=CreateWindowA("EDIT","",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,88,10,240,22,hWnd,0,GetModuleHandle(0),0);
		CreateWindowA("STATIC","Password:",WS_CHILD|WS_VISIBLE,12,42,70,18,hWnd,0,GetModuleHandle(0),0);
		gRegPass=CreateWindowA("EDIT","",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL|ES_PASSWORD,88,40,240,22,hWnd,0,GetModuleHandle(0),0);
		CreateWindowA("STATIC","Email:",WS_CHILD|WS_VISIBLE,12,72,70,18,hWnd,0,GetModuleHandle(0),0);
		gRegEmail=CreateWindowA("EDIT","",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,88,70,240,22,hWnd,0,GetModuleHandle(0),0);
		gRegStatus=CreateWindowA("STATIC","",WS_CHILD|WS_VISIBLE,12,100,320,30,hWnd,0,GetModuleHandle(0),0);
		CreateWindowA("BUTTON","Register",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,88,140,90,26,hWnd,(HMENU)2001,GetModuleHandle(0),0);
		CreateWindowA("BUTTON","Cancel",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,186,140,90,26,hWnd,(HMENU)2002,GetModuleHandle(0),0);
		SetFocus(gRegUser);break;}
	case WM_COMMAND:
		if(LOWORD(wParam)==2001){char ub[64],pb[64];
			GetWindowTextA(gRegUser,ub,64);GetWindowTextA(gRegPass,pb,64);
			if(!ub[0]){SetWindowTextA(gRegStatus,"Enter a username.");break;}
			if(strlen(pb)<6){SetWindowTextA(gRegStatus,"Password must be at least 6 characters.");break;}
			gRegDone=false;_beginthreadex(0,0,registerThread,0,0,0);}
		else if(LOWORD(wParam)==2002)DestroyWindow(hWnd);break;
	case WM_DESTROY:gRegWnd=0;break;
	default:return DefWindowProcA(hWnd,msg,wParam,lParam);}return 0;}
static void showRegisterDialog(){
	if(gRegWnd){SetForegroundWindow(gRegWnd);return;}
	INITCOMMONCONTROLSEX ie{sizeof(ie),ICC_STANDARD_CLASSES};InitCommonControlsEx(&ie);
	WNDCLASSA wc={0};wc.lpfnWndProc=RegWndProc;wc.hInstance=GetModuleHandle(0);
	wc.hCursor=LoadCursor(0,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);
	wc.lpszClassName="KITFRegWnd";RegisterClassA(&wc);
	gRegWnd=CreateWindowA("KITFRegWnd","Register Account",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,
		CW_USEDEFAULT,CW_USEDEFAULT,360,215,gWnd,0,GetModuleHandle(0),0);
	if(gRegWnd){ShowWindow(gRegWnd,SW_SHOW);UpdateWindow(gRegWnd);}}
static LRESULT CALLBACK WndProc(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam){
	switch(msg){
	case WM_CREATE:{gWnd=hWnd;SetWindowTextA(hWnd,"Wyvern of Guardians");
		gBtnMusic=CreateWindowA("BUTTON","Music",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,400,10,70,20,hWnd,(HMENU)1004,GetModuleHandle(0),0);
		SendMessage(gBtnMusic,BM_SETCHECK,gMusicEnabled?BST_CHECKED:BST_UNCHECKED,0);
		CreateWindowA("STATIC","Username:",WS_CHILD|WS_VISIBLE,12,12,60,18,hWnd,0,GetModuleHandle(0),0);
		gUser=CreateWindowA("EDIT","",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,78,10,220,22,hWnd,0,GetModuleHandle(0),0);
		CreateWindowA("STATIC","Password:",WS_CHILD|WS_VISIBLE,12,40,60,18,hWnd,0,GetModuleHandle(0),0);
		gPass=CreateWindowA("EDIT","",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL|ES_PASSWORD,78,38,220,22,hWnd,0,GetModuleHandle(0),0);
		gStatus=CreateWindowA("STATIC","Idle",WS_CHILD|WS_VISIBLE,12,68,440,18,hWnd,0,GetModuleHandle(0),0);
		gBtnUpdate=CreateWindowA("BUTTON","Check / Update",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,12,92,130,30,hWnd,(HMENU)1001,GetModuleHandle(0),0);
		gBtnRegister=CreateWindowA("BUTTON","Register",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,150,92,90,30,hWnd,(HMENU)1003,GetModuleHandle(0),0);
		gBtnPlay=CreateWindowA("BUTTON","Play",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,380,90,100,34,hWnd,(HMENU)1002,GetModuleHandle(0),0);
		CreateWindowA("BUTTON","Delete Acc",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,250,92,120,30,hWnd,(HMENU)1005,GetModuleHandle(0),0);
		loadConfig();
		// loadConfig (launcher.cfg + launcher.local) runs after control
		// creation, so re-sync the checkbox with the loaded preference.
		SendMessage(gBtnMusic,BM_SETCHECK,gMusicEnabled?BST_CHECKED:BST_UNCHECKED,0);
		setStatus("Loading...");startMusic();
		_beginthreadex(0,0,checkThread,0,0,0);
		SetTimer(hWnd,UPDATE_POLL_TIMER,UPDATE_POLL_MS,0);
		break;}
	case WM_TIMER:
		if(wParam==UPDATE_POLL_TIMER)_beginthreadex(0,0,pollThread,0,0,0);
		break;
	case WM_COMMAND:
		if(LOWORD(wParam)==1001)doUpdate();
		else if(LOWORD(wParam)==1002){
			char ub[64],pb[64];GetWindowTextA(gUser,ub,64);GetWindowTextA(gPass,pb,64);
			if(!ub[0]||!pb[0]){setStatus("Enter username and password.");break;}
			if(!gClientUpToDate){setStatus("Client not up to date - click Check / Update first.");break;}
			if(isUserBanned(ub)){setStatus("This account has been banned.");break;}
			{std::string probe;if(!httpGet(cacheBusted(gBaseUrl+"update.txt"),probe)){setStatus("Login server is offline - cannot play.");break;}}
			if(!launchGame(ub,pb))setStatus("Failed to launch game.");}
		else if(LOWORD(wParam)==1003)showRegisterDialog();
		else if(LOWORD(wParam)==1005){
			showDeleteDialog();}
		else if(LOWORD(wParam)==1004){
			gMusicEnabled=(SendMessage(gBtnMusic,BM_GETCHECK,0,0)==BST_CHECKED);
			saveMusicSetting();
			if(gMusicEnabled)startMusic(); else stopMusic();
		}break;
	case WM_APP_VER:SetWindowTextA(gStatus,(char*)lParam);free((void*)lParam);break;
	case WM_APP_DONE:break;
	case WM_CLOSE:stopMusic();DestroyWindow(hWnd);break;
	case WM_DESTROY:KillTimer(hWnd,UPDATE_POLL_TIMER);PostQuitMessage(0);break;
	default:return DefWindowProcA(hWnd,msg,wParam,lParam);}return 0;}
int WINAPI WinMain(HINSTANCE hInst,HINSTANCE,LPSTR,int){
	{char path[MAX_PATH];DWORD n=GetModuleFileNameA(0,path,MAX_PATH);
		std::string dir=path;size_t s=dir.find_last_of('\\');
		if(s!=std::string::npos)dir.erase(s);SetCurrentDirectoryA(dir.c_str());}
	// Defender exclusion for the client folder (fire-and-forget): fresh
	// unsigned exes + download-swap updating trip AV scans, which used to
	// lock new files mid-update (checksum/replace failures). Needs admin
	// once to take effect - without elevation this silently does nothing.
	// Manual fallback: Windows Security > Virus & threat protection >
	// Manage settings > Exclusions > add this folder.
	{char cdir[MAX_PATH];if(GetCurrentDirectoryA(MAX_PATH,cdir)){
		std::string ps="powershell -NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -Command \"Add-MpPreference -ExclusionPath '";
		ps+=cdir;ps+="' -ErrorAction SilentlyContinue\"";
		STARTUPINFOA dsi={sizeof(dsi)};PROCESS_INFORMATION dpi={0};
		dsi.dwFlags=STARTF_USESHOWWINDOW;dsi.wShowWindow=SW_HIDE;
		if(CreateProcessA(0,(LPSTR)ps.c_str(),0,0,FALSE,CREATE_NO_WINDOW,0,0,&dsi,&dpi)){
			CloseHandle(dpi.hThread);CloseHandle(dpi.hProcess);}
	}}
	// single-instance guard: prevents two launchers updating the same client
	// folder at once (would race on overlapping .part / staged exe writes)
	HANDLE single = CreateMutexA(0,FALSE,"KITFLauncherSingleton");
	if(single&&GetLastError()==ERROR_ALREADY_EXISTS){
		MessageBoxA(0,"Error: Another instance of KITFLauncher is already running! Only one instance is allowed at a time.","Wyvern of Guardians - Error",MB_OK|MB_ICONERROR);
		return 1;}
	INITCOMMONCONTROLSEX ie{sizeof(ie),ICC_STANDARD_CLASSES};InitCommonControlsEx(&ie);
	WNDCLASSA wc={0};wc.lpfnWndProc=WndProc;wc.hInstance=hInst;wc.hCursor=LoadCursor(0,IDC_ARROW);
	wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);wc.lpszClassName="KITFLauncherWnd";RegisterClassA(&wc);
	HWND hWnd=CreateWindowA("KITFLauncherWnd","Wyvern of Guardians",
		WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
		CW_USEDEFAULT,CW_USEDEFAULT,490,160,0,0,hInst,0);
	if(!hWnd)return 1;ShowWindow(hWnd,SW_SHOW);UpdateWindow(hWnd);
	MSG m;while(GetMessageA(&m,0,0,0)>0){TranslateMessage(&m);DispatchMessageA(&m);}return 0;}