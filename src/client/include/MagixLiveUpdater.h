#ifndef __MagixLiveUpdater_h_
#define __MagixLiveUpdater_h_

// MagixLiveUpdater - in-game data hot updater (no restart needed).
//
// What it does: fetches update.txt from the update server (same manifest the
// launcher uses), downloads changed DATA files to update_staging\, then swaps
// them into place. The caller re-parses whichever definition files changed.
//
// Hard limits (by design, not bugs):
// - .exe/.dll changes are NEVER applied live (Windows locks running
//   binaries). If any differ, nothing is touched and needsRestart is set.
// - Files locked by the renderer (e.g. open .kzip packs) fail the swap and
//   land in `skipped`: they apply at the next launcher run.
// - Already-cached art (textures/meshes loaded before the swap) stays until
//   relog; NEW files (new maps, models, items) work immediately.
// - Chat history is never touched; results are only APPENDED as new lines.
//
// Runs synchronously on the calling thread (brief hitch on big downloads).
#include <string>
#include <vector>
#include <cctype>
#include "../../Utils/Launcher/kitf_common.h"

struct LiveUpdateResult
{
	bool checked;        // manifest fetched and parsed
	bool upToDate;       // nothing differed
	bool needsRestart;   // an exe/dll (or nothing downloadable) requires relaunch
	std::string newVersion;
	std::vector<std::string> applied; // manifest-relative paths swapped live
	std::vector<std::string> skipped; // failed to swap (locked) - launcher takes them
	std::string error;
	LiveUpdateResult()
	{
		checked = false;
		upToDate = false;
		needsRestart = false;
	}
};

class MagixLiveUpdater
{
protected:
	std::string mBaseUrl;
	bool mHasBase;
public:
	MagixLiveUpdater()
	{
		mHasBase = false;
	}
	// Read updateurl= from launcher.cfg in the working directory.
	bool loadBaseUrl()
	{
		mHasBase = false;
		FILE* f;
		if(fopen_s(&f,"launcher.cfg","r")!=0)return false;
		char line[512];
		while(fgets(line,sizeof(line),f))
		{
			std::string s = line;
			while(!s.empty()&&(s.back()=='\n'||s.back()=='\r'))s.pop_back();
			if(s.rfind("updateurl=",0)==0)
			{
				mBaseUrl = s.substr(10);
				if(!mBaseUrl.empty()&&mBaseUrl.back()!='/')mBaseUrl += '/';
				mHasBase = !mBaseUrl.empty();
			}
		}
		fclose(f);
		return mHasBase;
	}
	static bool isBinaryName(const std::string& path)
	{
		std::string b = path;
		size_t s = b.find_last_of("/\\");
		if(s!=std::string::npos)b = b.substr(s+1);
		std::string low = "";
		for(size_t i = 0; i < b.size(); i++)
		{
			unsigned char ch = (unsigned char)b[i];
			if(ch>='A'&&ch<='Z')ch = (unsigned char)(ch-'A'+'a');
			low += (char)ch;
		}
		if(low.size()>=4&&(low.compare(low.size()-4,4,".exe")==0||low.compare(low.size()-4,4,".dll")==0))return true;
		return false;
	}
	static std::string toLocalPath(const std::string& manifestPath)
	{
		std::string local = manifestPath;
		for(size_t i = 0; i < local.size(); i++)if(local[i]=='/')local[i]='\\';
		return local;
	}
	// Full check + data apply. See header comment for the contract.
	LiveUpdateResult sync()
	{
		LiveUpdateResult r;
		if(!mHasBase && !loadBaseUrl()){r.error = "no update server (launcher.cfg)";return r;}
		std::string manifest;
		if(!httpGet(mBaseUrl+"update.txt",manifest))
		{
			r.error = "could not reach update server";
			return r;
		}
		Manifest mf;
		std::string base = mBaseUrl;
		if(!base.empty()&&base.back()=='/')base.erase(base.size()-1);
		if(!mf.parse(manifest,base)){r.error = "bad update manifest";return r;}
		r.checked = true;
		r.newVersion = mf.appVersion;

		std::vector<ManifestEntry> todo;
		for(size_t i = 0; i < mf.files.size(); i++)
		{
			const std::string local = toLocalPath(mf.files[i].path);
			if(!fileExists(local)||fileSize(local)!=mf.files[i].size||fileMd5(local)!=mf.files[i].md5)
			{
				// All-or-nothing per version: a binary change means the new
				// data may not match this exe, so touch nothing and relaunch.
				if(isBinaryName(mf.files[i].path)){r.needsRestart = true;return r;}
				todo.push_back(mf.files[i]);
			}
		}
		if(todo.empty()){r.upToDate = true;return r;}

		for(size_t i = 0; i < todo.size(); i++)
		{
			const std::string local = toLocalPath(todo[i].path);
			const std::string staged = std::string("update_staging\\") + local;
			ensureDirs(staged);
			remove(staged.c_str());
			bool ok = false;
			if(downloadToFile(todo[i].url,staged,0,0)>=0 && fileMd5(staged)==todo[i].md5)
			{
				ensureDirs(local);
				if(MoveFileExA(staged.c_str(),local.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0)
					ok = true;
			}
			if(ok)r.applied.push_back(todo[i].path);
			else{r.skipped.push_back(todo[i].path);remove(staged.c_str());}
		}
		return r;
	}
};

#endif
