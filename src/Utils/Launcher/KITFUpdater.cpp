// KITFUpdater.cpp - the patching half of the launcher (like WarRock's wrupdater).
// Fetches an update manifest from an HTTP(S) URL and downloads out-of-date/missing
// files into the game's client folder, verifying each file's MD5.
// Usage: KITFUpdater.exe <manifestURL> <targetDir>
#define WIN32_LEAN_AND_MEAN
#include "kitf_common.h"
#include <commctrl.h>
#include <process.h>

static HWND gProgress,gStatus,gTitle;
static bool gAbort=false;
static int gNeed=0,gDone=0;
static std::string gGameExe="ogremagix.exe";

enum { WM_APP_STATUS=WM_APP+1, WM_APP_DONE=WM_APP+2, WM_APP_PB=WM_APP+3 };

static void setStatus(const std::string& s){ SetWindowTextA(gStatus,s.c_str()); }

static bool progressCb(const char* label,long long total,long long done){
	if(gNeed<=0){PostMessageA(gProgress,PBM_SETPOS,0,0);PostMessageA(gStatus,WM_SETTEXT,0,(LPARAM)label);return !gAbort;}
	if(total>0&&done>=0){
		// map (files completed + this file's fractional progress) onto 0..100
		int whole=gDone*100/gNeed;
		int frac=(int)((double)(done*100/total)/gNeed);
		int pct=whole+frac;if(pct>100)pct=100;
		PostMessageA(gProgress,PBM_SETPOS,(WPARAM)pct,0);
	}else{
		int pct=gDone*100/gNeed;if(pct>100)pct=100;
		PostMessageA(gProgress,PBM_SETPOS,(WPARAM)pct,0);
	}
	return !gAbort;
}

static unsigned __stdcall updateThread(void* arg){
	// arg: "url\ntargetdir"
	char* s=(char*)arg;
	std::string url=s;
	char* t=strchr(s,'\n');
	std::string target= t? (t+1): ".";
	free(arg);

	PostMessageA(gStatus,WM_APP_STATUS,0,(LPARAM)"Contacting update server...");
	std::string manifest;
	unsigned long st=0;
	if(!httpGet(url,manifest,&st)){
		FILE* dbg;if(fopen_s(&dbg,"_kitupdater_dbg.txt","a")==0){fprintf(dbg,"URL=[%s] hStat=%lu\n",url.c_str(),st);fclose(dbg);}
		PostMessageA(gStatus,WM_APP_STATUS,0,(LPARAM)_strdup(("Failed to fetch manifest (HTTP "+std::to_string(st)+")").c_str()));
		PostMessageA(gTitle,WM_APP_DONE,0,2);
		return 0;
	}
	Manifest mf;
	if(!mf.parse(manifest,url.substr(0,url.find_last_of('/')))){
		PostMessageA(gStatus,WM_APP_STATUS,0,(LPARAM)"Manifest parse failed.");
		PostMessageA(gTitle,WM_APP_DONE,0,2);
		return 0;
	}
	// figure out how many need updating first (for overall progress)
	int need=0;
	for(auto&e:mf.files){
		std::string local=target+"\\"+e.path;
		if(!fileExists(local)||fileSize(local)!=e.size||fileMd5(local)!=e.md5)need++;
	}
	PostMessageA(gTitle,WM_APP_STATUS,0,(LPARAM)_strdup(("Updating client to version "+mf.appVersion).c_str()));
	SetWindowTextA(gTitle,("KITFUpdater - update to "+mf.appVersion).c_str());
	gNeed=need?need:1;gDone=0;
	PostMessageA(gProgress,PBM_SETRANGE32,0,100);
	PostMessageA(gProgress,PBM_SETPOS,0,0);
	int done=0,failed=0;
	std::string failedNames;
	std::string deferredFinal,deferredTemp;
	for(auto&e:mf.files){
		std::string local=target+"\\"+e.path;
		bool ok=true;
		if(!fileExists(local)||fileSize(local)!=e.size||fileMd5(local)!=e.md5){
			ensureDirs(local);
			// The game executable is always fetched to a temp name and only moved
			// into place after the rest of the update finishes, so a player never
			// sees a stale/mid-update executable in the client folder.
			std::string base=e.path;size_t sl=base.find_last_of("/\\");if(sl!=std::string::npos)base.erase(0,sl+1);
			bool isGame=_stricmp(base.c_str(),gGameExe.c_str())==0;
			// Download to a side temp, verify checksum, THEN swap into place.
			// Never touch the live file until the new bytes verify: a failed
			// download must keep the old file, not delete it.
			std::string outPath= isGame? (local+".pkg") : (local+".new");
			remove(outPath.c_str());
			long long got=-1;
			for(int attempt=0;attempt<3&&got<0;attempt++)
				got=downloadToFile(e.url,outPath,progressCb,&gAbort);
			{
				FILE* dbg;if(fopen_s(&dbg,"_kitupdater_dbg.txt","a")==0){fprintf(dbg,"DL url=[%s] got=%lld md5=[%s] want=[%s]\n",e.url.c_str(),got,fileMd5(outPath).c_str(),e.md5.c_str());fclose(dbg);}
			}
			if(got<0){failed++;failedNames+=base+" (download), ";ok=false;remove(outPath.c_str());}
			else if(fileMd5(outPath)!=e.md5){
				// Transient first failure (AV scan on fresh .exe, publish
				// race): one re-download before calling it failed.
				std::string firstMd5=fileMd5(outPath);
				Sleep(500);
				remove(outPath.c_str());
				got=downloadToFile(e.url,outPath,progressCb,&gAbort);
				std::string secondMd5=fileMd5(outPath);
				{
					FILE* dbg;if(fopen_s(&dbg,"_kitupdater_dbg.txt","a")==0){fprintf(dbg,"RETRY url=[%s] got=%lld first=[%s] second=[%s] want=[%s]\n",e.url.c_str(),got,firstMd5.c_str(),secondMd5.c_str(),e.md5.c_str());fclose(dbg);}
				}
				if(got<0||secondMd5!=e.md5){failed++;failedNames+=base+" (checksum), ";ok=false;remove(outPath.c_str());}
				else if(isGame){deferredFinal=local;deferredTemp=outPath;}
			}
			else if(isGame){// keep temp, defer final placement to end of update
				deferredFinal=local;deferredTemp=outPath;
			}
			else if(got==0 && e.size==0){/* empty file is fine - fall through to swap */}
			if(ok&&!isGame){
				SetFileAttributesA(local.c_str(),FILE_ATTRIBUTE_NORMAL);
				bool swapped=false;
				for(int attempt=0;attempt<3&&!swapped;attempt++){
					if(attempt>0)Sleep(200);
					swapped=MoveFileExA(outPath.c_str(),local.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
				}
				if(!swapped){failed++;failedNames+=base+" (replace), ";ok=false;remove(outPath.c_str());}
			}
		}
		if(ok){
			done++;gDone++;
			PostMessageA(gProgress,PBM_SETPOS,(WPARAM)((gDone*100)/gNeed),0);
		}
		if(gAbort)break;
	}
	if(!deferredTemp.empty()){
		if(gAbort||failed>0){remove(deferredTemp.c_str());}
		else{
			ensureDirs(deferredFinal);
			SetFileAttributesA(deferredFinal.c_str(),FILE_ATTRIBUTE_NORMAL);
			if(MoveFileExA(deferredTemp.c_str(),deferredFinal.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
				{} // exe staged in place only after the update completes
			else {failed++;failedNames+=" (game replace, game running?), ";remove(deferredTemp.c_str());}
		}
	}
	char msg[512];
	std::string detail=failedNames;
	if(detail.size()>2&&detail.substr(detail.size()-2)==", ")detail.erase(detail.size()-2);
	if(detail.size()>300)detail=detail.substr(0,300);
	if(failed>0&&!detail.empty())
		sprintf_s(msg,"Update %s: %d/%d files, %d failed (%s).",gAbort?"cancelled":"finished",done,(int)mf.files.size(),failed,detail.c_str());
	else
		sprintf_s(msg,"Update %s: %d/%d files, %d failed.",gAbort?"cancelled":"finished",done,(int)mf.files.size(),failed);
	SetWindowTextA(gStatus,msg);
	PostMessageA(gTitle,WM_APP_DONE,0,gAbort?3:(failed?2:1));
	return 0;
}

static LRESULT CALLBACK WndProc(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam){
	switch(msg){
	case WM_CREATE:{
		gTitle=hWnd;
		SetWindowTextA(hWnd,"KITFUpdater");
		INITCOMMONCONTROLSEX ie{sizeof(ie),ICC_PROGRESS_CLASS};
		InitCommonControlsEx(&ie);
		gStatus=CreateWindowA("STATIC","Idle",WS_CHILD|WS_VISIBLE,12,12,360,20,hWnd,0,GetModuleHandle(0),0);
		gProgress=CreateWindowA(PROGRESS_CLASSA,"",WS_CHILD|WS_VISIBLE|PBS_SMOOTH,12,40,360,24,hWnd,0,GetModuleHandle(0),0);
		SendMessageA(gProgress,PBM_SETRANGE32,0,100);
		SendMessageA(gProgress,PBM_SETPOS,0,0);
		CreateWindowA("BUTTON","Cancel",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,150,74,100,26,hWnd,(HMENU)1001,GetModuleHandle(0),0);
		break;}
	case WM_COMMAND:
		if(LOWORD(wParam)==1001)gAbort=true;
		break;
	case WM_APP_STATUS:
		SetWindowTextA(gStatus,(char*)lParam);
		break;
	case WM_APP_DONE:{
		int res=(int)lParam;
		SetWindowTextA(gStatus,res==1?"Update complete.":res==2?"Update had errors.":"Update cancelled.");
		if(res!=1)MessageBeep(MB_ICONWARNING);
		// give the launcher / calling process a chance to read a temp result
		FILE*f;
		if(fopen_s(&f,"_kitupdater_result.txt","w")==0){fprintf(f,"%d\n",res);fclose(f);}
		Sleep(1200);
		PostQuitMessage(0);
		break;}
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProcA(hWnd,msg,wParam,lParam);
	}
	return 0;
}

int WINAPI WinMain(HINSTANCE hInst,HINSTANCE,LPSTR cmdLine,int){
	// The command line may or may not include the exe path as the first token,
	// and args may be wrapped in double-quotes. Robust approach: split into
	// tokens, then the first token starting with http:// or https:// is the
	// manifest URL and the following token is the target dir.
	std::vector<std::string> toks;
	{
		const char* c=cmdLine;
		while(*c){
			while(*c==' '||*c=='\t'){c++;if(!*c)break;}
			if(!*c)break;
			std::string t;
			if(*c=='"'){c++;const char* e=strchr(c,'"');if(e){t.assign(c,e-c);c=e+1;}else{t.assign(c);c+=t.size();}}
			else{const char* e=c;while(*e&&*e!=' '&&*e!='\t')e++;t.assign(c,e-c);c=e;}
			if(!t.empty())toks.push_back(t);
		}
	}
	std::string url,target=".";
	int i=0;
	for(;i<(int)toks.size();i++){
		if(toks[i].rfind("http://",0)==0||toks[i].rfind("https://",0)==0){url=toks[i];break;}
	}
	if(url.empty()&&!toks.empty())url=toks[0]; // fallback: first token as url
	for(int j=i+1;j<(int)toks.size();j++){ // first token after url = target
		if(j==i+1){target=toks[j];if(j+1<(int)toks.size())gGameExe=toks[j+1];break;}
	}
	if(url.empty()){
		MessageBoxA(0,"Usage: KITFUpdater <manifestURL> [targetDir]","KITFUpdater",MB_ICONINFORMATION);
		return 1;
	}
	if(target.empty())target=".";
	{
		FILE* dbg;if(fopen_s(&dbg,"_kitupdater_dbg.txt","a")==0){fprintf(dbg,"CMD=[%s]\nURL=[%s] TARGET=[%s]\n",cmdLine,url.c_str(),target.c_str());fclose(dbg);}
	}
	WNDCLASSA wc={0};
	wc.lpfnWndProc=WndProc;wc.hInstance=hInst;wc.hCursor=LoadCursor(0,IDC_ARROW);
	wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);wc.lpszClassName="KITFUpdaterWnd";
	RegisterClassA(&wc);
	HWND hWnd=CreateWindowA("KITFUpdaterWnd","KITFUpdater",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
		CW_USEDEFAULT,CW_USEDEFAULT,392,140,0,0,hInst,0);
	if(!hWnd)return 1;
	ShowWindow(hWnd,SW_SHOW);
	UpdateWindow(hWnd);

	// allocate args + launch thread
	char* arg=(char*)malloc(url.size()+target.size()+2);
	sprintf_s(arg,url.size()+target.size()+2,"%s\n%s",url.c_str(),target.c_str());
	if(_beginthreadex(0,0,updateThread,arg,0,0)==0)return 1;

	MSG m;
	while(GetMessageA(&m,0,0,0)>0){TranslateMessage(&m);DispatchMessageA(&m);}
	return 0;
}
