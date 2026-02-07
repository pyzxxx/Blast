#include "Importer.h"
#include "Foundation/FileSystem.h"

#include <iostream>

int main(int argc, char* argv[])
{
    FS::Path::RegisterProtocol("asset", std::string(PROJECT_DIR) + "/Assets/");

    if (argc < 2)
    {
        std::cerr << "Error: Invalid arguments\n";
        return -1;
    }

    std::string command = argv[1];
    if (command == "import")
    {
        if (argc < 3)
        {
            std::cerr << "Error: Invalid arguments\n";
            return -1;
        }

        ImportAsset(argv[2]);
    }
    else
    {
        std::cerr << "Error: Unknown command '" << command << "'\n";
        return -1;
    }

    return 0;
}