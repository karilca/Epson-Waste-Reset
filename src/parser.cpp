#include "ewr/parser.h"
#include <filesystem>
#include <fstream>
#include <regex>
#include <iostream>

namespace fs = std::filesystem;

namespace ewr {

    std::vector<PrinterModel> ScanModelsFolder(const std::string& folderPath)
    {
        std::vector<PrinterModel> availableModels;
        std::regex filenameRegex(R"((.+)\.(txt|c)$)");

        if (!fs::exists(folderPath))
        {
            fs::create_directory(folderPath);
            return availableModels;
        }

        for (const auto& entry : fs::directory_iterator(folderPath))
        {
            if (entry.is_regular_file())
            {
                std::string filename = entry.path().filename().string();
                std::smatch match;

                if (std::regex_match(filename, match, filenameRegex))
                {
                    PrinterModel model;
                    model.name = match[1].str();
                    model.filepath = entry.path().string();
                    availableModels.push_back(model);
                }
            }
        }
        return availableModels;
    }

    std::vector<std::vector<unsigned char>> ParseWiresharkDump(const std::string& filepath)
    {
        std::vector<std::vector<unsigned char>> sequence;
        std::ifstream file(filepath);

        if (!file.is_open())
        {
            std::cerr << "Error: Could not open payload file." << std::endl;
            return sequence;
        }

        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        std::vector<std::vector<unsigned char>> all_packets;

        size_t pos = 0;
        while (true)
        {
            size_t startBrace = content.find('{', pos);
            if (startBrace == std::string::npos) break;
            size_t endBrace = content.find('}', startBrace);
            if (endBrace == std::string::npos) break;

            std::vector<unsigned char> current_packet;
            for (size_t i = startBrace + 1; i < endBrace; ++i)
            {
                if (i + 2 < endBrace && content[i] == '0' && (content[i + 1] == 'x' || content[i + 1] == 'X'))
                {
                    size_t j = i + 2;
                    while (j < endBrace && std::isxdigit(static_cast<unsigned char>(content[j])))
                    {
                        j++;
                    }
                    if (j > i + 2)
                    {
                        std::string hexStr = content.substr(i + 2, j - (i + 2));
                        unsigned char hexByte = static_cast<unsigned char>(std::stoul(hexStr, nullptr, 16));
                        current_packet.push_back(hexByte);
                        i = j - 1;
                    }
                }
            }

            if (current_packet.size() >= 27 && current_packet[0] == 0x1B && current_packet[1] == 0x00) 
            {
                current_packet.erase(current_packet.begin(), current_packet.begin() + 27);
            }

            if (!current_packet.empty())
            {
                all_packets.push_back(current_packet);
            }

            pos = endBrace + 1;
        }

        return all_packets;
    }
}