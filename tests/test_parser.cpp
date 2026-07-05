#include "test_framework.h"
#include "ewr/parser.h"
#include <filesystem>
#include <fstream>
#include <cstdio>

namespace fs = std::filesystem;

TEST(ParserSuite, ScanModelsFolder_Empty) {
    std::string dir = "temp_empty_models";
    fs::create_directory(dir);
    
    auto models = ewr::ScanModelsFolder(dir);
    EXPECT_EQ(models.size(), (size_t)0);
    
    fs::remove(dir);
}

TEST(ParserSuite, ScanModelsFolder_FiltersFiles) {
    std::string dir = "temp_filter_models";
    fs::create_directory(dir);
    
    // Create valid files
    std::ofstream(dir + "/model1.txt").close();
    std::ofstream(dir + "/model2.c").close();
    // Create invalid files
    std::ofstream(dir + "/model3.pdf").close();
    std::ofstream(dir + "/README.md").close();
    
    auto models = ewr::ScanModelsFolder(dir);
    EXPECT_EQ(models.size(), (size_t)2);
    
    // Clean up
    fs::remove_all(dir);
}

TEST(ParserSuite, ScanModelsFolder_CreatesFolder) {
    std::string dir = "temp_create_models";
    if (fs::exists(dir)) {
        fs::remove_all(dir);
    }
    
    auto models = ewr::ScanModelsFolder(dir);
    EXPECT_TRUE(fs::exists(dir));
    EXPECT_EQ(models.size(), (size_t)0);
    
    fs::remove(dir);
}

TEST(ParserSuite, ParseWiresharkDump_ValidTxt) {
    std::string path = "test_dump.txt";
    std::ofstream f(path);
    f << "char payload[] = {\n"
      << "  0x01, 0x02, 0x03, 0x04\n"
      << "};\n";
    f.close();
    
    auto seq = ewr::ParseWiresharkDump(path);
    EXPECT_EQ(seq.size(), (size_t)1);
    EXPECT_EQ(seq[0].size(), (size_t)4);
    EXPECT_EQ(seq[0][0], (unsigned char)0x01);
    EXPECT_EQ(seq[0][3], (unsigned char)0x04);
    
    std::remove(path.c_str());
}

TEST(ParserSuite, ParseWiresharkDump_ValidC) {
    std::string path = "test_dump.c";
    std::ofstream f(path);
    f << "static const unsigned char dump1[] = { 0xAA, 0xBB };\n"
      << "static const unsigned char dump2[] = { 0xCC, 0xDD };\n";
    f.close();
    
    auto seq = ewr::ParseWiresharkDump(path);
    EXPECT_EQ(seq.size(), (size_t)2);
    EXPECT_EQ(seq[0].size(), (size_t)2);
    EXPECT_EQ(seq[1].size(), (size_t)2);
    EXPECT_EQ(seq[0][0], (unsigned char)0xAA);
    EXPECT_EQ(seq[1][1], (unsigned char)0xDD);
    
    std::remove(path.c_str());
}

TEST(ParserSuite, ParseWiresharkDump_StripHeader) {
    std::string path = "test_strip.txt";
    std::ofstream f(path);
    f << "{\n";
    // 27 bytes header + 3 bytes payload = 30 bytes total
    f << "  0x1B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\n"
      << "  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\n"
      << "  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\n"
      << "  0x00, 0x00, 0x00, 0x99, 0x88, 0x77\n";
    f << "};\n";
    f.close();
    
    auto seq = ewr::ParseWiresharkDump(path);
    EXPECT_EQ(seq.size(), (size_t)1);
    // Header should be stripped, so only 3 bytes remain
    EXPECT_EQ(seq[0].size(), (size_t)3);
    EXPECT_EQ(seq[0][0], (unsigned char)0x99);
    EXPECT_EQ(seq[0][2], (unsigned char)0x77);
    
    std::remove(path.c_str());
}

TEST(ParserSuite, ParseWiresharkDump_EmptyOrInvalid) {
    std::string path = "test_invalid.txt";
    std::ofstream f(path);
    f << "this is some random text with no hex arrays\n";
    f.close();
    
    auto seq = ewr::ParseWiresharkDump(path);
    EXPECT_TRUE(seq.empty());
    
    std::remove(path.c_str());
}

TEST(ParserSuite, ParseWiresharkDump_FileNotFound) {
    auto seq = ewr::ParseWiresharkDump("does_not_exist.txt");
    EXPECT_TRUE(seq.empty());
}

TEST(ParserSuite, Replay_MalformedWiresharkFiles) {
    std::string path = "test_malformed.txt";
    std::ofstream f(path);
    f << "char payload[] = {\n"
      << "  0x01, 0xXX, 0x03\n"
      << "};\n";
    f.close();
    
    auto seq = ewr::ParseWiresharkDump(path);
    // Standard parser should handle invalid hex characters gracefully, either skipping or stopping
    std::remove(path.c_str());
}

TEST(ParserSuite, Replay_HugeWiresharkFile) {
    std::string path = "test_huge.txt";
    std::ofstream f(path);
    f << "char payload[] = {\n";
    for (int i = 0; i < 50000; ++i) {
        f << "0x01";
        if (i < 49999) f << ", ";
        if (i % 20 == 0) f << "\n";
    }
    f << "};\n";
    f.close();
    
    auto seq = ewr::ParseWiresharkDump(path);
    EXPECT_EQ(seq.size(), (size_t)1);
    EXPECT_EQ(seq[0].size(), (size_t)50000);
    
    std::remove(path.c_str());
}
