// items_converter - private dev tool for KITF project
// Converts Items.cfg (plaintext) <-> Items.dat (XOR7 encrypted).
//
// This mirrors EXACTLY the game's cipher so the produced file is byte-compatible
// with what the client reads. It matches:
//   - MagixUtils::XOR7  (src/client/include/MagixUtils.h)
//   - XORKEY = 7*(i%7+1), obfuscated macro from config/GameConfig.h
//   - XOR7FileGen encrypt semantics (plaintext-byte checksum)
//
// Usage:
//   items_converter encrypt <input.cfg> <output.dat>
//   items_converter decrypt <input.dat> <output.cfg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>

// XORKEY obfuscated form from config/GameConfig.h:  (((i%7)*6+7)+(i%7))  ==  7*(i%7+1)
static unsigned char xorKeyByte(int i)
{
	return (unsigned char)(((i % 7) * 6 + 7) + (i % 7));
}

// Mirrors MagixUtils::XOR7. If checksum != 0:
//   preChecksum=true  -> sums INPUT (plaintext) bytes
//   preChecksum=false -> sums OUTPUT (post-XOR, i.e. reconstructed plaintext) bytes
static std::string xor7(const std::string& in, unsigned long* checksum = 0, bool preChecksum = false)
{
	std::string out;
	for (int i = 0; i < (int)in.size(); ++i)
	{
		unsigned char tC = (unsigned char)in[i];
		if (checksum && preChecksum) *checksum += (unsigned long)tC;
		tC ^= xorKeyByte(i);
		if (tC > 126 || tC < 32) tC = (unsigned char)in[i];
		if (checksum && !preChecksum) *checksum += (unsigned long)tC;
		out += (char)tC;
	}
	return out;
}

static bool readLines(const std::string& path, std::vector<std::string>& lines)
{
	std::ifstream f(path.c_str());
	if (!f.is_open()) return false;
	std::string line;
	while (std::getline(f, line)) lines.push_back(line);
	return true;
}

// cfg -> dat : encrypt each line (sum plaintext for checksum), append encrypted checksum line.
static bool encryptFile(const std::string& in, const std::string& out)
{
	std::vector<std::string> lines;
	if (!readLines(in, lines)) return false;

	unsigned long cs = 0;
	std::ofstream o(out.c_str(), std::ios::binary);
	if (!o.is_open()) return false;
	for (size_t i = 0; i < lines.size(); ++i)
		o << xor7(lines[i], &cs, true) << '\n';
	o << xor7(std::to_string(cs)) << '\n';
	o.close();
	return true;
}

// dat -> cfg : decrypt all but the last (checksum) line, verify checksum.
// preChecksum=false sums reconstructed plaintext, matching the encrypted plaintext sum.
static bool decryptFile(const std::string& in, const std::string& out)
{
	std::vector<std::string> lines;
	if (!readLines(in, lines) || lines.empty()) return false;

	const std::string csLine = xor7(lines.back());                 // last line = encrypted checksum
	const unsigned long expect = strtoul(csLine.c_str(), 0, 10);

	unsigned long cs = 0;
	std::ofstream o(out.c_str());
	if (!o.is_open()) return false;
	for (size_t i = 0; i < lines.size() - 1; ++i)
		o << xor7(lines[i], &cs, false) << '\n';
	o.close();

	if (cs != expect) { std::remove(out.c_str()); return false; }
	return true;
}

static void usage()
{
	std::cerr << "Usage: items_converter <encrypt|decrypt> <input> <output>\n";
}

int main(int argc, char** argv)
{
	if (argc < 4)
	{
		usage();
		return 1;
	}
	const std::string mode = argv[1];
	bool ok;
	if (mode == "encrypt") ok = encryptFile(argv[2], argv[3]);
	else if (mode == "decrypt") ok = decryptFile(argv[2], argv[3]);
	else { usage(); return 1; }

	if (!ok) { std::cerr << "Failed\n"; return 1; }
	std::cout << (mode == "encrypt" ? "Encrypted" : "Decrypted") << " OK: " << argv[3] << "\n";
	return 0;
}
