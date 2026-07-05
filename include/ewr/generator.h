#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <optional>

namespace ewr {

    struct DbPrinterModel
    {
        std::string name;
        uint16_t rkey;
        std::string wkey;
        std::vector<uint16_t> addresses;
        std::vector<uint8_t> reset_values;
    };

    class UniversalGenerator
    {
    public:
        UniversalGenerator() = default;
        ~UniversalGenerator() = default;

        UniversalGenerator(const UniversalGenerator&) = delete;
        UniversalGenerator& operator=(const UniversalGenerator&) = delete;

        bool SyncDatabaseOTA();
        bool LoadDatabase(const std::string& filepath);

        bool IsEmpty() const;
        std::vector<DbPrinterModel> GetAvailableModels() const;
        std::vector<std::vector<unsigned char>> GenerateSequence(const DbPrinterModel& model) const;
        std::vector<unsigned char> GenerateReadPacket(uint16_t rkey, uint16_t address) const;
    private:
        std::vector<unsigned char> GenerateWritePacket(uint16_t rkey, uint16_t address, uint8_t value, const std::string& wkey) const;
    private:
        std::unordered_map<std::string, DbPrinterModel> database;
    };

} // namespace ewr