// RankDefs.h - shared account-rank ladder for KITF login server + client.
// Public domain. Dependency-free (only <string>) so both the plain-C++
// login server and the Ogre client can include it as "RankDefs.h"
// (config/ is already on the include path next to GameConfig.h).
//
// Ladder (low to high):
//   normal < vip < trialmod < mod < headmod < gm < headadmin < owner
// Ranks are stored per ACCOUNT (Data/<letter>/<user>.user, line 5 as
// "rank=<name>", XOR7-encoded like the other lines). Missing line = normal.
#ifndef __KITF_RankDefs_h_
#define __KITF_RankDefs_h_

#include <string>
#include <cctype>

enum KITF_Rank
{
	KITF_RANK_NORMAL = 0,
	KITF_RANK_VIP,
	KITF_RANK_TRIALMOD,
	KITF_RANK_MOD,
	KITF_RANK_HEADMOD,
	KITF_RANK_GM,
	KITF_RANK_HEADADMIN,
	KITF_RANK_OWNER,
	KITF_RANK_COUNT
};

// Canonical long names (stored in files, shown in logs).
inline const char* KITF_RankName(int r)
{
	static const char* n[] = { "normal","vip","trialmod","mod","headmod","gm","headadmin","owner" };
	return (r >= 0 && r < KITF_RANK_COUNT) ? n[r] : "normal";
}

// Short wire codes (fit the 8-char StringCompressor fields used on the wire).
inline const char* KITF_RankCode(int r)
{
	static const char* c[] = { "nil","vip","trl","mod","hmd","gm","had","own" };
	return (r >= 0 && r < KITF_RANK_COUNT) ? c[r] : "nil";
}

inline int KITF_RankNormalize(int r)
{
	if(r < 0 || r >= KITF_RANK_COUNT) return KITF_RANK_NORMAL;
	return r;
}

// Accepts "trial mod", "trial_mod", "headadmin", "game master", "admin", ...
// Returns rank 0..7, or -1 when unknown.
inline int KITF_RankFromName(const std::string& in)
{
	std::string s;
	for(size_t i = 0; i < in.size(); i++)
	{
		unsigned char ch = (unsigned char)in[i];
		if(ch == ' ' || ch == '_' || ch == '-') continue;
		if(ch >= 'A' && ch <= 'Z') ch = (unsigned char)(ch - 'A' + 'a');
		s += (char)ch;
	}
	if(s == "normal" || s == "user" || s == "player" || s == "nil" || s == "false" || s.empty()) return KITF_RANK_NORMAL;
	if(s == "vip" || s == "donator" || s == "donor") return KITF_RANK_VIP;
	if(s == "trialmod" || s == "trial" || s == "trl" || s == "trialmoderator") return KITF_RANK_TRIALMOD;
	if(s == "mod" || s == "moderator") return KITF_RANK_MOD;
	if(s == "headmod" || s == "hmd" || s == "headmoderator" || s == "seniormod") return KITF_RANK_HEADMOD;
	if(s == "gm" || s == "gamemaster" || s == "admin" || s == "ok" || s == "ok.") return KITF_RANK_GM;
	if(s == "headadmin" || s == "had" || s == "headgm") return KITF_RANK_HEADADMIN;
	if(s == "owner" || s == "own" || s == "headowner" || s == "root") return KITF_RANK_OWNER;
	return -1;
}

// Reverse of KITF_RankCode, plus legacy client tokens ("ok." -> gm).
// Returns rank 0..7, or -1 when unknown.
inline int KITF_RankFromCode(const std::string& in)
{
	std::string s;
	for(size_t i = 0; i < in.size() && i < 16; i++)
	{
		unsigned char ch = (unsigned char)in[i];
		if(ch >= 'A' && ch <= 'Z') ch = (unsigned char)(ch - 'A' + 'a');
		s += (char)ch;
	}
	if(s == "nil" || s == "false" || s.empty()) return KITF_RANK_NORMAL;
	if(s == "vip") return KITF_RANK_VIP;
	if(s == "trl") return KITF_RANK_TRIALMOD;
	if(s == "mod") return KITF_RANK_MOD;
	if(s == "hmd") return KITF_RANK_HEADMOD;
	if(s == "gm" || s == "ok" || s == "ok.") return KITF_RANK_GM;
	if(s == "had") return KITF_RANK_HEADADMIN;
	if(s == "own") return KITF_RANK_OWNER;
	return -1;
}

// Legacy bool mapping so every existing isAdmin/isMod gate keeps working:
// VIP and normal get no powers; trial..headmod get mod powers;
// gm and above get full admin powers.
inline bool KITF_RankIsAdmin(int r) { return KITF_RankNormalize(r) >= KITF_RANK_GM; }
inline bool KITF_RankIsMod(int r)
{
	r = KITF_RankNormalize(r);
	return r >= KITF_RANK_TRIALMOD && r < KITF_RANK_GM;
}
// May use kick/where/whois/massblock/1-day bans.
inline bool KITF_RankCanModerate(int r) { return KITF_RankNormalize(r) >= KITF_RANK_TRIALMOD; }
// VIP tag holder (no staff powers by default). Gate future VIP-only commands
// on this: client use mDef->getRank()==KITF_RANK_VIP, server use
// accountRank(name)==KITF_RANK_VIP (never trust the client's claim).
inline bool KITF_RankIsVip(int r) { return KITF_RankNormalize(r) == KITF_RANK_VIP; }
// May use /setadmin at all (further hierarchy checks in KITF_RankSetAllowed).
inline bool KITF_RankCanSetAdmin(int r)
{
	r = KITF_RankNormalize(r);
	return r == KITF_RANK_OWNER || r == KITF_RANK_HEADADMIN;
}

// Hierarchy rule for granting ranks. requester/targetCurrent/newRank are
// 0..7 (use KITF_RankNormalize first). Returns true if allowed.
// - owner: may set anyone to anything (console is the backstop for mistakes).
// - headadmin: may only touch accounts below headadmin, and may only grant
//   ranks below headadmin.
// - gm: may only grant/revoke VIP on non-staff accounts (normal<->vip).
//   This is the manual donator workflow; a future automatic donation system
//   writes the same account rank line (rank=vip, XOR7-encoded) and triggers
//   the same live push, so it stays compatible.
inline bool KITF_RankSetAllowed(int requester, int targetCurrent, int newRank)
{
	requester = KITF_RankNormalize(requester);
	targetCurrent = KITF_RankNormalize(targetCurrent);
	newRank = KITF_RankNormalize(newRank);
	if(requester == KITF_RANK_OWNER) return true;
	if(requester == KITF_RANK_HEADADMIN)
	{
		if(targetCurrent >= KITF_RANK_HEADADMIN) return false;
		if(newRank >= KITF_RANK_HEADADMIN) return false;
		return true;
	}
	if(requester == KITF_RANK_GM)
	{
		if(targetCurrent > KITF_RANK_VIP) return false;
		if(newRank > KITF_RANK_VIP) return false;
		return true;
	}
	return false;
}

#endif
