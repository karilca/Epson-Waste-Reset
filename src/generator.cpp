#include "ewr/generator.h"
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <urlmon.h>
#else
#include <curl/curl.h>
#endif

using json = nlohmann::json;

namespace ewr {

    bool UniversalGenerator::SyncDatabaseOTA()
    {
        const char* url = "https://raw.githubusercontent.com/RxNaison/Epson-Waste-Reset/main/database.json";
        const char* dest = "database.json";

#ifdef _WIN32
        HRESULT res = URLDownloadToFileA(NULL, url, dest, 0, NULL);
        return (res == S_OK);
#else
        CURL* curl = curl_easy_init();
        if (curl)
        {
            FILE* fp = fopen(dest, "wb");
            curl_easy_setopt(curl, CURLOPT_URL, url);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            CURLcode res = curl_easy_perform(curl);
            curl_easy_cleanup(curl);
            fclose(fp);
            return (res == CURLE_OK);
        }
        return false;
#endif
    }

    bool UniversalGenerator::LoadDatabase(const std::string& filepath)
    {
        std::ifstream file(filepath);
        if (!file.is_open())
            return false;

        try
        {
            json j;
            file >> j;

            for (auto& [key, val] : j.items())
            {
                DbPrinterModel model;
                model.name = key;
                model.rkey = val.value("rkey", 0);
                model.wkey = val.value("wkey", "");

                if (val.contains("addresses") && val["addresses"].is_array())
                {
                    for (auto& addr : val["addresses"])
                        model.addresses.push_back(addr.get<uint16_t>());
                }

                if (val.contains("reset") && val["reset"].is_array())
                {
                    for (auto& rst : val["reset"])
                        model.reset_values.push_back(rst.get<uint8_t>());
                }

                while (model.reset_values.size() < model.addresses.size())
                    model.reset_values.push_back(0x00);

                database[key] = std::move(model);
            }
            return true;
        }
        catch (const json::exception& e)
        {
            std::cerr << "[!] JSON Parse Error: " << e.what() << std::endl;
            return false;
        }
    }

    bool UniversalGenerator::IsEmpty() const
    {
        return database.empty();
    }

    std::vector<DbPrinterModel> UniversalGenerator::GetAvailableModels() const
    {
        std::vector<DbPrinterModel> models;
        models.reserve(database.size());

        for (const auto& pair : database)
            models.push_back(pair.second);

        return models;
    }

    std::vector<unsigned char> UniversalGenerator::GenerateWritePacket(uint16_t rkey, uint16_t address, uint8_t value, const std::string& wkey) const
    {
        uint8_t c = 0x42; // '|B' command
        uint8_t not_c = ~c & 0xFF;
        uint8_t shift_c = ((c >> 1) & 0x7F) | ((c << 7) & 0x80);

        std::vector<unsigned char> inner;
        inner.push_back(rkey & 0xFF);         // rkey Low
        inner.push_back((rkey >> 8) & 0xFF);  // rkey High
        inner.push_back(c);
        inner.push_back(not_c);
        inner.push_back(shift_c);
        inner.push_back(address & 0xFF);      // addr Low
        inner.push_back((address >> 8) & 0xFF); // addr High
        inner.push_back(value);               // reset value (e.g., 0x00)
        inner.insert(inner.end(), wkey.begin(), wkey.end());

        std::vector<unsigned char> epson_cmd;
        epson_cmd.push_back(0x7C); // '|'
        epson_cmd.push_back(0x7C); // '|'
        uint16_t len = inner.size();
        epson_cmd.push_back(len & 0xFF);      // inner_len Low
        epson_cmd.push_back((len >> 8) & 0xFF); // inner_len High
        epson_cmd.insert(epson_cmd.end(), inner.begin(), inner.end());

        // Wrap in IEEE 1284.4 D4 Header (EPSON-CTRL: 0x02, 0x02)
        std::vector<unsigned char> d4;
        d4.push_back(0x02); // psid
        d4.push_back(0x02); // ssid
        uint16_t d4_len = epson_cmd.size() + 6;
        d4.push_back((d4_len >> 8) & 0xFF);
        d4.push_back(d4_len & 0xFF);
        d4.push_back(0x00); // credit: MUST be 0 here to prevent overflow
        d4.push_back(0x00); // control
        d4.insert(d4.end(), epson_cmd.begin(), epson_cmd.end());

        return d4;
    }

    std::vector<unsigned char> UniversalGenerator::GenerateReadPacket(uint16_t rkey, uint16_t address) const
    {
        uint8_t c = 0x41; // '|A' command
        uint8_t not_c = ~c & 0xFF;
        uint8_t shift_c = ((c >> 1) & 0x7F) | ((c << 7) & 0x80);

        std::vector<unsigned char> inner;
        inner.push_back(rkey & 0xFF);         // rkey Low
        inner.push_back((rkey >> 8) & 0xFF);  // rkey High
        inner.push_back(c);
        inner.push_back(not_c);
        inner.push_back(shift_c);
        inner.push_back(address & 0xFF);      // addr Low
        inner.push_back((address >> 8) & 0xFF); // addr High

        std::vector<unsigned char> epson_cmd;
        epson_cmd.push_back(0x7C); // '|'
        epson_cmd.push_back(0x7C); // '|'
        uint16_t len = inner.size();
        epson_cmd.push_back(len & 0xFF);      // inner_len Low
        epson_cmd.push_back((len >> 8) & 0xFF); // inner_len High
        epson_cmd.insert(epson_cmd.end(), inner.begin(), inner.end());

        // Wrap in IEEE 1284.4 D4 Header (EPSON-CTRL: 0x02, 0x02)
        std::vector<unsigned char> d4;
        d4.push_back(0x02); // psid
        d4.push_back(0x02); // ssid
        uint16_t d4_len = epson_cmd.size() + 6;
        d4.push_back((d4_len >> 8) & 0xFF);
        d4.push_back(d4_len & 0xFF);
        d4.push_back(0x00); // credit: MUST be 0 here to prevent overflow
        d4.push_back(0x00); // control
        d4.insert(d4.end(), epson_cmd.begin(), epson_cmd.end());

        return d4;
    }

    std::vector<std::vector<unsigned char>> UniversalGenerator::GenerateSequence(const DbPrinterModel& model) const
    {
        std::vector<std::vector<unsigned char>> sequence;

        // Enter IEEE 1284.4 Packet Mode (3 Null bytes RESTORED)
        const unsigned char ejl_init[] = {
            0x00, 0x00, 0x00, 0x1B, 0x01, '@', 'E', 'J', 'L', ' ', '1', '2', '8', '4', '.', '4', '\n',
            '@', 'E', 'J', 'L', '\n',
            '@', 'E', 'J', 'L', '\n'
        };
        sequence.push_back(std::vector<unsigned char>(std::begin(ejl_init), std::end(ejl_init)));

        // D4 Init (Targeting PSID=0x00, SSID=0x00)
        const unsigned char d4_init[] = {
            0x00, 0x00, 0x00, 0x08, 0x01, 0x00, 0x00, 0x10
        };
        sequence.push_back(std::vector<unsigned char>(std::begin(d4_init), std::end(d4_init)));

        // D4 OpenChannel (Targeting EPSON-CTRL 0x02, 0x02)
        const unsigned char d4_open[] = {
            0x00, 0x00, 0x00, 0x11, 0x01, 0x00, 0x01,
            0x02, 0x02, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00
        };
        sequence.push_back(std::vector<unsigned char>(std::begin(d4_open), std::end(d4_open)));

        // IEEE 1284.4 Protocol Maintenance Packets
        const unsigned char d4_credit_grant[] = {
            0x00, 0x00, 0x00, 0x0B, 0x01, 0x00, 0x03, 0x02, 0x02, 0x00, 0x01
        };
        const unsigned char d4_credit_req[] = {
            0x00, 0x00, 0x00, 0x0D, 0x01, 0x00, 0x04, 0x02, 0x02, 0xFF, 0xFF, 0x00, 0x01
        };

        for (size_t i = 0; i < model.addresses.size(); ++i)
        {
            sequence.push_back(std::vector<unsigned char>(std::begin(d4_credit_grant), std::end(d4_credit_grant)));
            sequence.push_back(std::vector<unsigned char>(std::begin(d4_credit_req), std::end(d4_credit_req)));
            sequence.push_back(GenerateWritePacket(model.rkey, model.addresses[i], model.reset_values[i], model.wkey));
        }

        return sequence;
    }

} // namespace ewr