#include <iostream>
#include <Ogre.h>
#include <fstream>
#include <vector>

using namespace Ogre;
using namespace std;

// Simulate ROTKEY global
const String ROTKEY = "MyPrivateKey";

// ROT13 Cipher Function (reversible)
const String ROT13(const std::string& input) {
    String output = "";
    const String& key = ROTKEY;
    int keysize = (int)key.size();
    if (keysize == 0) return input;

    for (size_t i = 0; i < input.size(); ++i) {
        char tC = input[i];
        char keyChar = key[i % keysize];
        int shift = 0;
        if (keyChar >= 'a' && keyChar <= 'z') shift = keyChar - 'a';
        else if (keyChar >= 'A' && keyChar <= 'Z') shift = keyChar - 'A';

        if (tC >= 'a' && tC <= 'z') {
            tC = 'a' + (tC - 'a' + shift) % 26;
        } else if (tC >= 'A' && tC <= 'Z') {
            tC = 'A' + (tC - 'A' + shift) % 26;
        }

        if (tC < 32 || tC > 126) tC = input[i];

        output += tC;
    }

    return output;
}

// Cipher text file line-by-line
bool ROT13CipherGen(const String& inputIO, const std::string& outputIO) {
    std::ifstream inFile(inputIO.c_str());
    if (!inFile.is_open()) return false;

    std::vector<String> buffer;
    std::string line;

    while (std::getline(inFile, line)) {
        buffer.push_back(ROT13(line));
    }
    inFile.close();

    std::ofstream outFile(outputIO);
    if (!outFile.is_open()) return false;

    for (const auto& cipheredLine : buffer) {
        outFile << cipheredLine << "\n";
    }
    outFile.close();

    return true;
}

// Decipher ROT13 file back into memory
vector<String>::type ROT13Decipher(const String& inFile) {
    vector<String>::type buffer;
    DataStreamPtr stream = Root::getSingleton().openFileStream(inFile);

    if (stream.isNull()) {
        OGRE_EXCEPT(Exception::ERR_FILE_NOT_FOUND, 
            "Cannot locate file " + inFile + " for ROT13 decipher.", 
            "ROT13Decipher");
    }

    while (!stream->eof()) {
        String line = stream->getLine(false);
        if (line.empty() && stream->eof()) break;

        buffer.push_back(ROT13(line));
    }

    return buffer;
}

// --- Entry point ---
int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: ROT13Tool <inputFile.txt> <outputFile.txt>\n";
        return 1;
    }

    String inputFile = argv[1];
    String outputFile = argv[2];

    try {
        Root root(""); // Dummy init for file access

        // Cipher the file
        if (!ROT13CipherGen(inputFile, outputFile)) {
            std::cerr << "Failed to cipher file.\n";
            return 1;
        }

        std::cout << "Ciphering complete. Now deciphering:\n";

        // Decipher it back
        vector<String>::type result = ROT13Decipher(outputFile);

        // Output deciphered lines
        for (const auto& line : result) {
            std::cout << line << std::endl;
        }

    } catch (const Exception& e) {
        std::cerr << "OGRE Exception: " << e.getFullDescription() << "\n";
        return 1;
    }

    return 0;
}
