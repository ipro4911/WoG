#pragma once
// © Purring Bytes for KITF project
// this is public domain, this mention must stay.
//
// Note : XOR function are here for now but are gona be migrated in a subclass of KITF_UTILS
#ifndef __MagixUtils_h_
#define __MagixUtils_h_

#include "Ogre.h"

using namespace Ogre;

namespace KITF_Utils 
{
	class MagixUtils 
	{
	protected:
	public:
		MagixUtils()
		{

		}
		
		const String XOR7(const String& input, unsigned long* checksum = 0, bool preChecksum = false)
		{
			String output = "";
			for (int i = 0; i < input.length(); i++)
			{
				char tC = input[i];
				if (checksum && preChecksum)
				{
					*checksum += (unsigned long)tC;
				}

				tC ^= XORKEY;//3*(i%7)+24; // 7*(i%7+1)

				//Replace illegal characters with original character
				if (tC > char(126) || tC<char(32))
				{
					tC = input[i];
				}

				output += tC;

				if (checksum && !preChecksum)
				{
					*checksum += (unsigned long)tC;
				}
			}

			return output;
		}

		// let temporary but gona be replaced with a rot13 soon.
	// this probably derivate from some askme or stackoverflow and must therefore be
	// rewritten.
		vector<String>::type XORInternal(const String inFile, bool preChecksum = false)
		{
			String line = "", prevline;
			vector<String>::type tBuffer;
			unsigned long tChecksum = 0;
			DataStreamPtr stream = Root::getSingleton().openFileStream(inFile);

			if (!stream.isNull())
			{
				int i = 0;
				while (!stream->eof())
				{
					prevline = line;
					line = stream->getLine(false);
					if (stream->eof())
						break;
					prevline = XOR7(prevline, &tChecksum, preChecksum);

					if (i > 1)
						tBuffer.push_back(prevline);
					i++;
				}
				tBuffer.pop_back();
				const bool tChecksumRight = (tChecksum == StringConverter::parseLong(XOR7(prevline)));
				if (!tChecksumRight)
					throw(Exception(9, "Corrupted Data File", inFile + ", Please reencode or ."));
				tBuffer.pop_back();

				return tBuffer;
			}
			OGRE_EXCEPT(Exception::ERR_FILE_NOT_FOUND, "Cannot locate file " +
				inFile + " required for game.", "MagixExternalDefinitions.h");
		}


		bool XOR7FileGen(const String& infile, const String& outfile, bool decrypt, bool checksum = false)
		{
			vector<String>::type tBuffer;
			unsigned long tChecksum = 0;
			std::ifstream inFile;
			inFile.open(infile.c_str(), (decrypt ? std::ios_base::binary : std::ifstream::in));
			if (!inFile.good())return false;
			while (inFile.good() && !inFile.eof())
			{
				char tLine[512] = "";
				inFile.getline(tLine, 512);
				tBuffer.push_back(tLine);
			}
			tBuffer.pop_back();
			inFile.close();

			std::ofstream outFile(outfile.c_str(), (decrypt ? std::ofstream::out : std::ios_base::binary));
			for (int i = 0; i < (int)tBuffer.size() - (decrypt && checksum ? 1 : 0); i++)
			{
				const String tLine = XOR7(tBuffer[i].c_str(), (checksum ? &tChecksum : 0), !decrypt) + "\n";
				outFile.write(tLine.c_str(), (int)tLine.length());
			}
			if (!decrypt && checksum && tBuffer.size() > 0)
			{
				const String tLine = XOR7(StringConverter::toString(tChecksum)) + "\n";
				outFile.write(tLine.c_str(), (int)tLine.length());
			}
			outFile.close();

			if (decrypt && checksum)
			{
				const bool tChecksumRight = tBuffer.size() > 0 ? (tChecksum == StringConverter::parseLong(XOR7(tBuffer[tBuffer.size() - 1]).c_str())) : false;
				if (!tChecksumRight)_unlink(outfile.c_str());
				return tChecksumRight;
			}

			return true;
		}

		///
		/// ROT 13 replacement (untested
		/// 
		// this take input and throws out a rot13 key, only chars in A-Z and a-z are parsed the rest stays normal
		const String ROT13(const std::string& input)
		{
			String output = "";

			String key = ROTKEY;
			int keysize = (int)key.size();

			// this should happen like never.		
			//if(keyLen == 0) return input; // no key means no change

			for (size_t i = 0; i < input.size(); ++i)
			{
				char tC = input[i];

				// Get key char at position modulo key length
				char keyChar = ROTKEY[i % keysize];

				// Compute shift amount as 0-based letter index if letter, else zero
				int shift = 0;
				if ((keyChar >= 'a' && keyChar <= 'z')) shift = keyChar - 'a';
				else if ((keyChar >= 'A' && keyChar <= 'Z')) shift = keyChar - 'A';

				// Apply shift only to letters
				if (tC >= 'a' && tC <= 'z') {
					tC = 'a' + (tC - 'a' + shift) % 26;
				}
				else if (tC >= 'A' && tC <= 'Z') {
					tC = 'A' + (tC - 'A' + shift) % 26;
				}
				// else keep tC unchanged for non-letters

				// Keep printable range only (32..126), else revert to original input char
				if (tC < 32 || tC > 126) {
					tC = input[i];
				}

				output += tC;
			}

			return output;
		}

		// this generate but do not decipher, for deciphering, have a tool external to game
		// warning : you are responsible for your cfg backups, do not distribute them.
		bool ROT13CipherGen(const String& inputIO, const std::string& outputIO) {
			std::ifstream inFile(inputIO.c_str());

			// input file not found or can't be opened, this need logging
			if (!inFile.is_open()) {
				return false;
			}

			std::vector<String> buffer;
			std::string line;

			// Read input file line by line
			while (std::getline(inFile, line)) {
				buffer.push_back(ROT13(line));
			}
			inFile.close();

			// Write ciphered lines to output file
			std::ofstream outFile(outputIO);
			if (!outFile.is_open()) {
				return false; // output file can't be opened for writing
			}

			for (const auto& cipheredLine : buffer) {
				outFile << cipheredLine << "\n";
			}
			outFile.close();

			return true; // success
		}
		// this is similar to how XORInternal works
		vector<String>::type ROT13Decipher(const String& inFile)
		{
			vector<String>::type buffer;

			DataStreamPtr stream = Root::getSingleton().openFileStream(inFile);

			if (stream.isNull())
			{
				OGRE_EXCEPT(Exception::ERR_FILE_NOT_FOUND,
					"Cannot locate file " + inFile + " for ROT13 decipher.",
					"ROT13Decipher");
			}

			while (!stream->eof())
			{
				String line = stream->getLine(false); // false = do not trim
				if (line.empty() && stream->eof()) // to avoid processing last empty line after EOF
					break;

				// Apply ROT13 to decipher line
				String decipheredLine = ROT13(line);

				buffer.push_back(decipheredLine);
			}

			return buffer;
		}
		///
		/// END of ROT 13 functions
		/// 

		///
		/// Some world related functions
		/// 
		bool loadWorldFile(const String& filename, String& terrain, Real& x, Real& z, Vector3& spawnSquare, Vector2& worldBounds)
		{
			long tSize = 0;
			char* tBuffer;
			String tData = "";

			std::ifstream inFile;
			inFile.open(filename.c_str(), std::ifstream::in);
			if (inFile.good())
			{
				inFile.seekg(0, std::ios::end);
				tSize = (long)inFile.tellg();
				inFile.seekg(0, std::ios::beg);
				tBuffer = new char[tSize];
				strcpy(tBuffer, "");
				inFile.getline(tBuffer, tSize, '#');
				inFile.close();
				tData = tBuffer;
				delete[] tBuffer;
			}

			const vector<String>::type tPart = StringUtil::split(tData, "[#");
			for (int i = 0; i<int(tPart.size()); i++)
			{
				const vector<String>::type tLine = StringUtil::split(tPart[i], "\n", 6);
				if (tLine.size() > 0)
				{
					if (StringUtil::startsWith(tLine[0], "Initialize", false) && tLine.size() >= 5)
					{
						terrain = tLine[1];
						x = StringConverter::parseReal(tLine[2]);
						z = StringConverter::parseReal(tLine[3]);
						spawnSquare = StringConverter::parseVector3(tLine[4]);
						if (tLine.size() > 5)worldBounds = StringConverter::parseVector2(tLine[5]);
						return true;
					}
				}
			}
			return false;
		}


	};
}
#endif;