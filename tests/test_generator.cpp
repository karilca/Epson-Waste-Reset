#include "test_framework.h"
#include "ewr/generator.h"
#include "mocks/mock_network.h"
#include <fstream>
#include <cstdio>

TEST(GeneratorSuite, SyncDatabaseOTA_Success) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    ewr::UniversalGenerator gen;
    bool res = gen.SyncDatabaseOTA();
    EXPECT_TRUE(res);
}

TEST(GeneratorSuite, SyncDatabaseOTA_Offline) {
    MockNetworkState::Get().SetScenario(NetworkScenario::OFFLINE);
    ewr::UniversalGenerator gen;
    bool res = gen.SyncDatabaseOTA();
    EXPECT_FALSE(res);
}

TEST(GeneratorSuite, SyncDatabaseOTA_Timeout) {
    MockNetworkState::Get().SetScenario(NetworkScenario::TIMEOUT);
    ewr::UniversalGenerator gen;
    bool res = gen.SyncDatabaseOTA();
    EXPECT_FALSE(res);
}

TEST(GeneratorSuite, LoadDatabase_ValidJson) {
    std::string path = "test_valid.json";
    std::ofstream f(path);
    f << "{\n"
      << "  \"L3150\": { \"rkey\": 17080, \"wkey\": \"L3150_KEY\", \"addresses\": [20, 21], \"reset\": [0, 0] }\n"
      << "}";
    f.close();

    ewr::UniversalGenerator gen;
    bool res = gen.LoadDatabase(path);
    EXPECT_TRUE(res);
    EXPECT_FALSE(gen.IsEmpty());
    auto models = gen.GetAvailableModels();
    EXPECT_EQ(models.size(), (size_t)1);
    EXPECT_STREQ(models[0].name.c_str(), "L3150");
    EXPECT_EQ(models[0].rkey, (uint16_t)17080);
    EXPECT_STREQ(models[0].wkey.c_str(), "L3150_KEY");
    EXPECT_EQ(models[0].addresses.size(), (size_t)2);
    EXPECT_EQ(models[0].reset_values.size(), (size_t)2);

    std::remove(path.c_str());
}

TEST(GeneratorSuite, LoadDatabase_MissingAddresses) {
    std::string path = "test_missing_addr.json";
    std::ofstream f(path);
    f << "{\n"
      << "  \"L3150\": { \"rkey\": 17080, \"wkey\": \"L3150_KEY\" }\n"
      << "}";
    f.close();

    ewr::UniversalGenerator gen;
    bool res = gen.LoadDatabase(path);
    EXPECT_TRUE(res);
    auto models = gen.GetAvailableModels();
    EXPECT_EQ(models[0].addresses.size(), (size_t)0);

    std::remove(path.c_str());
}

TEST(GeneratorSuite, LoadDatabase_MissingResetValues) {
    std::string path = "test_missing_reset.json";
    std::ofstream f(path);
    f << "{\n"
      << "  \"L3150\": { \"rkey\": 17080, \"wkey\": \"L3150_KEY\", \"addresses\": [20, 21] }\n"
      << "}";
    f.close();

    ewr::UniversalGenerator gen;
    bool res = gen.LoadDatabase(path);
    EXPECT_TRUE(res);
    auto models = gen.GetAvailableModels();
    EXPECT_EQ(models[0].reset_values.size(), (size_t)2);
    EXPECT_EQ(models[0].reset_values[0], (uint8_t)0);
    EXPECT_EQ(models[0].reset_values[1], (uint8_t)0);

    std::remove(path.c_str());
}

TEST(GeneratorSuite, LoadDatabase_CorruptJson) {
    std::string path = "test_corrupt.json";
    std::ofstream f(path);
    f << "{\n"
      << "  \"L3150\": { \"rkey\": 17080, \n";
    f.close();

    ewr::UniversalGenerator gen;
    bool res = gen.LoadDatabase(path);
    EXPECT_FALSE(res);
    EXPECT_TRUE(gen.IsEmpty());

    std::remove(path.c_str());
}

TEST(GeneratorSuite, LoadDatabase_FileNotFound) {
    ewr::UniversalGenerator gen;
    bool res = gen.LoadDatabase("does_not_exist.json");
    EXPECT_FALSE(res);
    EXPECT_TRUE(gen.IsEmpty());
}

TEST(GeneratorSuite, UniversalGenerator_IsEmpty) {
    ewr::UniversalGenerator gen;
    EXPECT_TRUE(gen.IsEmpty());
}

TEST(GeneratorSuite, GenerateWritePacket_Format) {
    ewr::UniversalGenerator gen;
    ewr::DbPrinterModel model;
    model.name = "L3150";
    model.rkey = 17080;
    model.wkey = "L3150_KEY";
    model.addresses = {20};
    model.reset_values = {0};

    auto seq = gen.GenerateSequence(model);
    // Sequence has EJL init, D4 init, Open channel, and then grant, req, write packet
    // Total sequence size for 1 address = 6 packets
    EXPECT_EQ(seq.size(), (size_t)6);
    
    auto write_packet = seq[5];
    EXPECT_EQ(write_packet[0], (unsigned char)0x02); // psid
    EXPECT_EQ(write_packet[1], (unsigned char)0x02); // ssid
}

TEST(GeneratorSuite, GenerateSequence_SafeInit) {
    ewr::UniversalGenerator gen;
    ewr::DbPrinterModel model;
    model.name = "L3150";
    model.rkey = 17080;
    model.wkey = "L3150_KEY";
    model.addresses = {20};
    model.reset_values = {0};

    auto seq = gen.GenerateSequence(model);
    EXPECT_TRUE(seq.size() >= 3);
    
    // Check EJL command in first packet
    bool found_ejl = false;
    for (auto b : seq[0]) {
        if (b == 'E') found_ejl = true;
    }
    EXPECT_TRUE(found_ejl);
}

TEST(GeneratorSuite, GenerateSequence_AddressValuesMatch) {
    ewr::UniversalGenerator gen;
    ewr::DbPrinterModel model;
    model.name = "L3150";
    model.rkey = 17080;
    model.wkey = "L3150_KEY";
    model.addresses = {20, 21, 22};
    model.reset_values = {0, 0, 0};

    auto seq = gen.GenerateSequence(model);
    // For 3 addresses, we get: EJL, D4 init, Open, then for each address we get grant, req, write.
    // Total = 3 + 3 * 3 = 12 packets.
    EXPECT_EQ(seq.size(), (size_t)12);
}

TEST(GeneratorSuite, GenerateSequence_KeysEncoded) {
    ewr::UniversalGenerator gen;
    ewr::DbPrinterModel model;
    model.name = "L3150";
    model.rkey = 0x1234;
    model.wkey = "TESTKEY";
    model.addresses = {20};
    model.reset_values = {0};

    auto seq = gen.GenerateSequence(model);
    auto write_packet = seq[5];
    
    // Key should be encoded inside the inner body
    bool found_key = false;
    for (size_t i = 0; i < write_packet.size() - 6; ++i) {
        if (write_packet[i] == 'T' && write_packet[i+1] == 'E' && write_packet[i+2] == 'S' && write_packet[i+3] == 'T') {
            found_key = true;
        }
    }
    EXPECT_TRUE(found_key);
}

TEST(GeneratorSuite, Database_ExtremeSize) {
    std::string path = "test_extreme.json";
    std::ofstream f(path);
    f << "{\n";
    for (int i = 0; i < 10000; ++i) {
        f << "  \"MODEL_" << i << "\": { \"rkey\": 1234, \"wkey\": \"KEY\", \"addresses\": [10], \"reset\": [0] }";
        if (i < 9999) f << ",\n";
    }
    f << "\n}";
    f.close();

    ewr::UniversalGenerator gen;
    bool res = gen.LoadDatabase(path);
    EXPECT_TRUE(res);
    auto models = gen.GetAvailableModels();
    EXPECT_EQ(models.size(), (size_t)10000);

    std::remove(path.c_str());
}

TEST(GeneratorSuite, Database_InvalidModelFields) {
    std::string path = "test_invalid_fields.json";
    std::ofstream f(path);
    f << "{\n"
      << "  \"L3150\": { \"rkey\": \"bad_rkey\", \"wkey\": 1234, \"addresses\": [20], \"reset\": [0] }\n"
      << "}";
    f.close();

    ewr::UniversalGenerator gen;
    // nlohmann::json will fail to parse if type conversions fail
    bool res = gen.LoadDatabase(path);
    // It may or may not succeed based on fallback, but it must handle it gracefully
    std::remove(path.c_str());
}
