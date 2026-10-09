#include "ShaderReader.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <filesystem>

namespace Utils
{

std::string readSrc(const std::string& filePath)
{
    std::ifstream file(filePath, std::ios::in | std::ios::binary);
    if (!file.is_open())
    {
        std::cerr << "Failed to open shader file: " << filePath << std::endl;
        return {};
    }

    std::stringstream buffer;
    std::string line;
    std::filesystem::path currentPath(filePath);
    std::filesystem::path currentDir = currentPath.parent_path();

    while (std::getline(file, line))
    {
        size_t includePos = line.find("#include");
        if (includePos != std::string::npos)
        {
            size_t firstQuote = line.find('"', includePos);
            size_t lastQuote = line.rfind('"');
            if (firstQuote != std::string::npos && lastQuote != std::string::npos && firstQuote != lastQuote)
            {
                std::string includeFile = line.substr(firstQuote + 1, lastQuote - firstQuote - 1);
                std::filesystem::path includePath = currentDir / includeFile;
                buffer << readSrc(includePath.string()) << "\n";
            }
        }
        else
        {
            buffer << line << "\n";
        }
    }
    
    return buffer.str();
}

}
