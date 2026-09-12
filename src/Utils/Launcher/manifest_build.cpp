// manifest_build.cpp - dev tool: generate update.txt manifest + info.txt from a
// local folder, for hosting on your web server.
// Usage: manifest_build <sourceDir> [version] [outputDir]
//   sourceDir : folder whose contents are what players should have (e.g. the
//               fully updated client/ folder).
//   version   : version string, e.g. 0.1.2 (default 0.1.2)
//   outputDir : where to write update.txt and info.txt (default sourceDir)
#include <windows.h>
#include <cstdio>
#include <string>
#include <vector>
#include <fstream>
#include "kitf_common.h"

// Runtime artifacts never shipped to players; excluded from the manifest.
// NOTE: KITFLauncher.exe IS shipped (the launcher self-updates via a
// .new + auto-restart helper) and KITFUpdater.exe IS shipped too (it is
// never running during a launcher-driven update, so in-place replace is
// safe). publish.bat / manifest_build.exe stay on the operator machine only.
static bool hasSuffix(const std::string& f,const char* suf){
	size_t n=strlen(suf);
	if(f.size()<n)return false;
	std::string tail=f.substr(f.size()-n);
	for(size_t i=0;i<n;i++)if(tolower((unsigned char)tail[i])!=tolower((unsigned char)suf[i]))return false;
	return true;
}
static bool isExcluded(const std::string& f){
	const char* exc[]={"_kitupdater_result.txt","_kitupdater_dbg.txt","update.txt","info.txt",
		"update_launcher.bat","KITFLauncher.new.exe","KITFLauncher.exe.new",
		"banlist.txt","Settings.cfg","Settings2.dat","Settings3.dat","localIP.txt",
		"useWindowsCursor.dat","launcher.local",
		// Player-mutable local settings: the game rewrites these at runtime
		// (Ogre saves render setup to ogre.cfg on start/exit, the game saves
		// Hotkeys.cfg on exit). Syncing them would fight the player forever:
		// any local change becomes a permanent "(checksum)" failure, and an
		// update would wipe personal settings. Missing files are handled by
		// the game itself (Ogre first-run dialog, built-in hotkey defaults).
		"ogre.cfg","Hotkeys.cfg",
		"ogre.log","Ogre.log","manifest_build.exe","publish.bat",0};
	for(int i=0;exc[i];i++)if(f==exc[i])return true;
	// Stale download temps / logs must never be published, even if a
	// previous interrupted run left them in the served folder.
	const char* ext[]={".part",".pkg",".new",".tmp",".log",".bak",".dat.cfg",".sav",0};
	for(int i=0;ext[i];i++)if(hasSuffix(f,ext[i]))return true;
	return false;
}
static void walkDir(const std::string& base, const std::string& dir, std::vector<std::string>& out){
	WIN32_FIND_DATAA fd;
	std::string pat = (dir.empty()? base : base+"\\"+dir) + "\\*";
	HANDLE h = FindFirstFileA(pat.c_str(), &fd);
	if(h==INVALID_HANDLE_VALUE)return;
	do{
		if(strcmp(fd.cFileName,".")==0||strcmp(fd.cFileName,"..")==0)continue;
		if(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY){
			walkDir(base, dir.empty()?fd.cFileName:dir+"\\"+fd.cFileName, out);
		}else{
			std::string rel=dir.empty()?fd.cFileName:dir+"\\"+fd.cFileName;
			if(!isExcluded(rel))out.push_back(rel);
		}
	}while(FindNextFileA(h,&fd));
	FindClose(h);
}

int main(int argc,char**argv){
	if(argc<2){fprintf(stderr,"Usage: manifest_build <sourceDir> [version] [outDir]\n");return 1;}
	std::string src=argv[1],ver=(argc>2)?argv[2]:"0.1.2",outdir=(argc>3)?argv[3]:argv[1];
	// The version arg is the single source of truth: stamp it into
	// version.txt BEFORE walking, so clients download it via the normal
	// file sync and report it at connect (ID_CLIENTVERSION). Neither the
	// game client nor the game server needs a recompile to change version.
	// Written to src (so it is included in the manifest) and to outdir
	// when different (so the served folder has it for the game server).
	{
		std::string vp=src+"\\version.txt";
		std::ofstream vf(vp.c_str(),std::ios::binary|std::ios::trunc);
		if(vf.good()){vf.write(ver.c_str(),ver.size());vf.write("\n",1);}
		vf.close();
		if(outdir!=src){
			std::string op=outdir+"\\version.txt";
			std::ofstream of(op.c_str(),std::ios::binary|std::ios::trunc);
			if(of.good()){of.write(ver.c_str(),ver.size());of.write("\n",1);}
		}
	}
	std::vector<std::string> files;
	walkDir(src,"",files);
	std::ofstream mf(outdir+"\\update.txt");
	mf<<"KITFUP 1\nVER "<<ver<<"\n";
	for(auto&f:files){
		std::string fp=src+"\\"+f;
		std::string local=f;
		for(size_t i=0;i<local.size();i++)if(local[i]=='\\')local[i]='/';
		long long sz=fileSize(fp);
		std::string md5=fileMd5(fp);
		mf<<"FILE "<<local<<"|"<<sz<<"|"<<md5<<"|"<<local<<"\n";
	}
	mf<<"END\n";
	mf.close();
	printf("Manifest: %d files, version %s\n",(int)files.size(),ver.c_str());
	// Self-verify: re-read what we just wrote and confirm size/md5 still
	// match disk. Catches the classic race where build.bat re-staged an exe
	// AFTER the manifest was generated (clients then fail checksum forever
	// until the next publish). Any mismatch = republish needed.
	{
		std::ifstream vf(outdir+"\\update.txt",std::ios::binary);
		std::string text((std::istreambuf_iterator<char>(vf)),std::istreambuf_iterator<char>());
		Manifest vm;
		if(vm.parse(text,"")){
			int stale=0;
			for(auto&e:vm.files){
				std::string fp=src+"\\"+e.path;
				for(size_t i=0;i<fp.size();i++)if(fp[i]=='/')fp[i]='\\';
				if(!fileExists(fp)||fileSize(fp)!=e.size||fileMd5(fp)!=e.md5){
					fprintf(stderr,"STALE: %s changed after manifest walk (republish!)\n",e.path.c_str());
					stale++;
				}
			}
			if(stale==0)printf("Verify: all %d entries match disk.\n",(int)vm.files.size());
			else{printf("Verify FAILED: %d stale entries - staged files changed during build, republish.\n",stale);return 2;}
		}
	}
	return 0;
}
