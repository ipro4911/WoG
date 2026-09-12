#ifndef __MagixAutoLogin_h_
#define __MagixAutoLogin_h_

// Simplified auto-login (launcher integration). The launcher launches the game as:
//   ogremagix.exe --user <u> --pass <p> --auto [--autochar]
// --auto      : skip the logon screen and log in with the given credentials.
// --autochar  : after login, automatically pick the first character and play.
#include <string>

class MagixAutoLogin
{
public:
	static bool enabled;
	static bool autoChar;
	static std::string user;
	static std::string pass;

	static void reset()
	{
		enabled = false;
		autoChar = false;
		user.clear();
		pass.clear();
	}

	static std::string argValue(const std::string& args, const std::string& key)
	{
		// find "key " or "key" at start
		const std::string token = key + " ";
		size_t pos = 0;
		while (true)
		{
			pos = args.find(key, pos);
			if (pos == std::string::npos) return "";
			// ensure it's a whole-token match at a boundary
			if (pos > 0 && (args[pos-1] != ' ' && args[pos-1] != '\t'))
			{
				pos += key.size();
				continue;
			}
		size_t after = pos + key.size();
		if (after < args.size() && args[after] == '=')
		{
			size_t start = after + 1;
			size_t end = start;
			while (end < args.size() && args[end] != ' ' && args[end] != '\t') end++;
			return args.substr(start, end - start);
		}
		// Space-separated form, as the launcher sends it:
		//   ogremagix.exe --user <name> --pass <pw> --auto
		{
			size_t start = after;
			while (start < args.size() && (args[start] == ' ' || args[start] == '\t')) start++;
			if (start >= args.size()) return "";
			if (args.compare(start, 2, "--") == 0) return ""; // next flag, no value
			size_t end = start;
			while (end < args.size() && args[end] != ' ' && args[end] != '\t') end++;
			return args.substr(start, end - start);
		}
			pos += key.size();
		}
	}

	static void parse(const char* cmdLine)
	{
		reset();
		if (!cmdLine) return;
		const std::string args(cmdLine);
		enabled = args.find("--auto") != std::string::npos;
		autoChar = args.find("--autochar") != std::string::npos;
		user = argValue(args, "--user");
		pass = argValue(args, "--pass");
		if (!enabled) autoChar = false;
	}
};

#endif
