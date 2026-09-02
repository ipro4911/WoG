#include <string>
#include <zzip/plugin.h>

// Your magic number. Change this to match the magic number in your un-obfuscator.
static int xor_value = 0x10;

// Override the read handler for zzip.
static zzip_ssize_t our_read(int fd, void* buf, zzip_size_t len)
{
    const zzip_ssize_t bytes = read(fd, buf, len);
    zzip_ssize_t i;
    char* pch = (char*)buf;
    for (i=0; i<bytes; ++i)
    {
        pch[i] ^= xor_value;
    }
    return bytes;
}

int main (int argc, char* argv[])
{

    std::string usage =
        " Obfuscate <fileName>\n"
        " - This program will (un)obfuscate a (.OBFUSZIP).zip file.\n"
        " Feed it a .zip file and it will obfuscate it.\n"
        " Feed it a .OBFUSZIP file and it will unobfuscate it.\n";

    if(argc != 2)
    {
        printf(usage.c_str());
        return 0;
    }

    if(strlen(argv[1]) > 128)
    {
        fprintf(stderr, "Please provide a filename shorter than 128 chars.\n");
        exit(1);
    }

    std::string fileName_ext = argv[1];
    std::string outFileName;

    // Get the extension and filename
    size_t delim = fileName_ext.find_last_of(".");
    std::string fileName = fileName_ext.substr(0 , delim);
    std::string extension = fileName_ext.substr(delim + 1);

    // Convert the extension to lower case.
    for(size_t position = 0; position < extension.size(); ++position)
    {
        extension[position] = tolower(extension[position]);
    }

    if(extension == "zip")
    {
        outFileName = fileName + ".OBFUSZIP";
    }
    else if(extension == "obfuszip")
    {
        outFileName = fileName + ".zip";
    }
    else
    {
        printf(usage.c_str());
        return 0;
    }

    /* Convert the file */
    FILE* fin = fopen(fileName_ext.c_str(), "rb");

    if(!fin)
    {
        fprintf(stderr, "Can't open input file \"%s\"\n", fileName_ext.c_str());
        exit(1);
    }

    FILE* fout = fopen(outFileName.c_str(), "wb");

    if(!fout)
    {
        fprintf(stderr, "Can't open output file \"%s\"\n", outFileName.c_str());
        exit(1);
    }

    int ch;

    while((ch = fgetc(fin)) != EOF)
    {
        ch ^= xor_value;
        fputc(ch, fout);
    }

    fclose(fout);
    fclose(fin);
    return 0;
}