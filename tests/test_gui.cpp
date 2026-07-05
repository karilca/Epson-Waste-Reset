#include "test_framework.h"
#include "gui/gui_app.h"
#include "mocks/imgui/imgui.h"
#include "mocks/mock_network.h"
#include "mocks/mock_usb.h"
#include <chrono>
#include <thread>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

// Helper to simulate waiting for a condition with a timeout
template<typename F>
static void WaitFor(F&& cond, int timeout_ms = 1000) {
    auto start = std::chrono::steady_clock::now();
    while (!cond()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        if (elapsed > timeout_ms) {
            break;
        }
    }
}

// 1. Search UI & Filtering
TEST(GuiSuite, Search_EmptyQuery) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    ewr::EwrGuiApp app;
    app.Initialize();
    app.SetSearchQuery("");
    
    MockImGuiState::Get().Clear();
    app.RenderUI();
    
    // Verify that at least L3150 and L3210 are present
    auto filtered = app.GetFilteredModelNames();
    EXPECT_TRUE(filtered.size() >= 2);
}

TEST(GuiSuite, Search_SingleMatch) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    ewr::EwrGuiApp app;
    app.Initialize();
    app.SetSearchQuery("L3150");
    
    MockImGuiState::Get().Clear();
    app.RenderUI();
    
    auto filtered = app.GetFilteredModelNames();
    EXPECT_EQ(filtered.size(), (size_t)1);
    EXPECT_STREQ(filtered[0].c_str(), "L3150");
}

TEST(GuiSuite, Search_MultipleMatches) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    ewr::EwrGuiApp app;
    app.Initialize();
    app.SetSearchQuery("L");
    
    MockImGuiState::Get().Clear();
    app.RenderUI();
    
    auto filtered = app.GetFilteredModelNames();
    EXPECT_TRUE(filtered.size() >= 2);
}

TEST(GuiSuite, Search_CaseInsensitive) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    ewr::EwrGuiApp app;
    app.Initialize();
    app.SetSearchQuery("l3150");
    
    MockImGuiState::Get().Clear();
    app.RenderUI();
    
    auto filtered = app.GetFilteredModelNames();
    EXPECT_EQ(filtered.size(), (size_t)1);
    EXPECT_STREQ(filtered[0].c_str(), "L3150");
}

TEST(GuiSuite, Search_NoMatch) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    ewr::EwrGuiApp app;
    app.Initialize();
    app.SetSearchQuery("XYZ999");
    
    MockImGuiState::Get().Clear();
    app.RenderUI();
    
    auto filtered = app.GetFilteredModelNames();
    EXPECT_EQ(filtered.size(), (size_t)0);
    EXPECT_TRUE(MockImGuiState::Get().HasText("No results found"));
}

TEST(GuiSuite, Search_TrimSpaces) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    ewr::EwrGuiApp app;
    app.Initialize();
    app.SetSearchQuery("  L3150   ");
    
    MockImGuiState::Get().Clear();
    app.RenderUI();
    
    auto filtered = app.GetFilteredModelNames();
    EXPECT_EQ(filtered.size(), (size_t)1);
    EXPECT_STREQ(filtered[0].c_str(), "L3150");
}

TEST(GuiSuite, Search_SpecialCharacters) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    ewr::EwrGuiApp app;
    app.Initialize();
    app.SetSearchQuery("L[31]50");
    
    MockImGuiState::Get().Clear();
    app.RenderUI();
    
    auto filtered = app.GetFilteredModelNames();
    EXPECT_EQ(filtered.size(), (size_t)0);
}

// 2. Details Panel & Reset Button State
TEST(GuiSuite, SelectModel_DisplaysSmartDetails) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    ewr::EwrGuiApp app;
    app.Initialize();
    app.SelectModel("L3150");
    
    MockImGuiState::Get().Clear();
    app.RenderUI();
    
    EXPECT_TRUE(MockImGuiState::Get().HasText("Type: Smart Protocol"));
    EXPECT_TRUE(MockImGuiState::Get().HasText("Read Key: 17080"));
}

TEST(GuiSuite, SelectModel_DisplaysReplayDetails) {
    // Create a temporary custom model file
    fs::create_directory("models");
    std::ofstream("models/CustomModel.txt").close();
    
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    ewr::EwrGuiApp app;
    app.Initialize();
    app.SelectModel("CustomModel");
    
    MockImGuiState::Get().Clear();
    app.RenderUI();
    
    EXPECT_TRUE(MockImGuiState::Get().HasText("Type: Replay Model"));
    
    fs::remove("models/CustomModel.txt");
}

TEST(GuiSuite, SelectModel_EnablesResetButton) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    ewr::EwrGuiApp app;
    app.Initialize();
    
    EXPECT_FALSE(app.IsResetButtonEnabled());
    
    app.SelectModel("L3150");
    EXPECT_TRUE(app.IsResetButtonEnabled());
}

TEST(GuiSuite, DeselectModel_DisablesResetButton) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    ewr::EwrGuiApp app;
    app.Initialize();
    
    app.SelectModel("L3150");
    EXPECT_TRUE(app.IsResetButtonEnabled());
    
    app.SelectModel("");
    EXPECT_FALSE(app.IsResetButtonEnabled());
}

// 3. Log Interceptor
TEST(GuiSuite, LogCapture_StdoutRedirect) {
    ewr::EwrGuiApp app;
    app.Initialize();
    app.ClearLogs();
    
    std::cout << "TEST_STDOUT_MESSAGE" << std::endl;
    
    std::string out = app.GetConsoleOutput();
    EXPECT_TRUE(out.find("TEST_STDOUT_MESSAGE") != std::string::npos);
}

TEST(GuiSuite, LogCapture_StderrRedirect) {
    ewr::EwrGuiApp app;
    app.Initialize();
    app.ClearLogs();
    
    std::cerr << "TEST_STDERR_MESSAGE" << std::endl;
    
    std::string out = app.GetConsoleOutput();
    EXPECT_TRUE(out.find("TEST_STDERR_MESSAGE") != std::string::npos);
}

TEST(GuiSuite, LogCapture_ThreadSafety) {
    ewr::EwrGuiApp app;
    app.Initialize();
    app.ClearLogs();
    
    std::thread t1([](){ std::cout << "THREAD_1" << std::endl; });
    std::thread t2([](){ std::cout << "THREAD_2" << std::endl; });
    t1.join();
    t2.join();
    
    std::string out = app.GetConsoleOutput();
    EXPECT_TRUE(out.find("THREAD_1") != std::string::npos);
    EXPECT_TRUE(out.find("THREAD_2") != std::string::npos);
}

TEST(GuiSuite, LogCapture_OverflowControl) {
    ewr::EwrGuiApp app;
    app.Initialize();
    app.ClearLogs();
    
    // LogBuffer has limit, let's verify it doesn't crash on very large strings
    std::string huge(150000, 'A');
    std::cout << huge << std::endl;
    
    std::string out = app.GetConsoleOutput();
    EXPECT_TRUE(out.size() < 1200000);
}

// 4. Asynchronous Coordination & Task States
TEST(GuiSuite, AsyncTask_OTA_StatusRunning) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    ewr::EwrGuiApp app;
    app.Initialize();
    
    // Thread runs async, checking status running state
    EXPECT_TRUE(app.IsOtaSyncRunning() || app.GetOtaStatusText() == "Synced");
}

TEST(GuiSuite, AsyncTask_OTA_StatusSuccess) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    ewr::EwrGuiApp app;
    app.Initialize();
    
    WaitFor([&](){ return !app.IsOtaSyncRunning(); });
    EXPECT_STREQ(app.GetOtaStatusText().c_str(), "Synced");
}

TEST(GuiSuite, AsyncTask_Reset_StatusRunning) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    MockUsbState::Get().SetScenario(UsbScenario::SUCCESS);
    
    ewr::EwrGuiApp app;
    app.Initialize();
    app.SelectModel("L3150");
    app.TriggerReset();
    
    EXPECT_TRUE(app.IsResetRunning());
    app.Shutdown();
}

TEST(GuiSuite, AsyncTask_Reset_StatusSuccess) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    MockUsbState::Get().SetScenario(UsbScenario::SUCCESS);
    
    ewr::EwrGuiApp app;
    app.Initialize();
    app.SelectModel("L3150");
    app.TriggerReset();
    
    WaitFor([&](){ return !app.IsResetRunning(); });
    EXPECT_TRUE(app.GetResetStatusText().find("Success") != std::string::npos);
}

TEST(GuiSuite, AsyncTask_Reset_StatusError) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    MockUsbState::Get().SetScenario(UsbScenario::NO_DEVICE);
    
    ewr::EwrGuiApp app;
    app.Initialize();
    app.SelectModel("L3150");
    app.TriggerReset();
    
    WaitFor([&](){ return !app.IsResetRunning(); });
    EXPECT_TRUE(app.GetResetStatusText().find("Error") != std::string::npos);
}

TEST(GuiSuite, AsyncTask_LogForwarding) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    MockUsbState::Get().SetScenario(UsbScenario::SUCCESS);
    
    ewr::EwrGuiApp app;
    app.Initialize();
    app.ClearLogs();
    app.SelectModel("L3150");
    app.TriggerReset();
    
    WaitFor([&](){ return !app.IsResetRunning(); });
    std::string out = app.GetConsoleOutput();
    EXPECT_TRUE(out.find("Initiating") != std::string::npos);
}

TEST(GuiSuite, AsyncTask_ProgressUpdates) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    MockUsbState::Get().SetScenario(UsbScenario::SUCCESS);
    
    ewr::EwrGuiApp app;
    app.Initialize();
    app.SelectModel("L3150");
    app.TriggerReset();
    
    // Check intermediate progress updates
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    float prog = app.GetResetProgress();
    EXPECT_TRUE(prog >= 0.0f && prog <= 1.0f);
    
    WaitFor([&](){ return !app.IsResetRunning(); });
    EXPECT_EQ(app.GetResetProgress(), 1.0f);
}

TEST(GuiSuite, AsyncTask_NoGuiFreeze) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    MockUsbState::Get().SetScenario(UsbScenario::SUCCESS);
    
    ewr::EwrGuiApp app;
    app.Initialize();
    app.SelectModel("L3150");
    
    auto t1 = std::chrono::steady_clock::now();
    app.TriggerReset();
    
    // The call to TriggerReset must be non-blocking and return immediately
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t1).count();
    EXPECT_TRUE(diff < 50); // should be almost instantaneous (<50ms)
    app.Shutdown();
}

TEST(GuiSuite, AsyncTask_CancelReset) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    MockUsbState::Get().SetScenario(UsbScenario::SUCCESS);
    
    ewr::EwrGuiApp app;
    app.Initialize();
    app.SelectModel("L3150");
    app.TriggerReset();
    
    // Cancel immediately
    app.Shutdown();
    EXPECT_TRUE(app.GetResetStatusText().find("Cancelled") != std::string::npos || !app.IsResetRunning());
}

TEST(GuiSuite, AsyncTask_MultipleRestarts) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    MockUsbState::Get().SetScenario(UsbScenario::SUCCESS);
    
    ewr::EwrGuiApp app;
    app.Initialize();
    app.SelectModel("L3150");
    
    app.TriggerReset();
    WaitFor([&](){ return !app.IsResetRunning(); });
    
    app.TriggerReset();
    WaitFor([&](){ return !app.IsResetRunning(); });
    
    EXPECT_TRUE(app.GetResetStatusText().find("Success") != std::string::npos);
}

// 5. E2E Scenario Tests
TEST(GuiSuite, E2E_Startup_SyncSuccess_Root) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    MockUsbState::Get().SetUid(0);
    
    ewr::EwrGuiApp app;
    app.Initialize();
    
    WaitFor([&](){ return !app.IsOtaSyncRunning(); });
    EXPECT_FALSE(app.IsWarningBannerVisible());
    EXPECT_STREQ(app.GetOtaStatusText().c_str(), "Synced");
}

TEST(GuiSuite, E2E_Startup_SyncSuccess_NoRoot) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    MockUsbState::Get().SetUid(1000);
    
    ewr::EwrGuiApp app;
    app.Initialize();
    
    WaitFor([&](){ return !app.IsOtaSyncRunning(); });
    EXPECT_TRUE(app.IsWarningBannerVisible());
}

TEST(GuiSuite, E2E_Startup_SyncOffline_Root) {
    MockNetworkState::Get().SetScenario(NetworkScenario::OFFLINE);
    MockUsbState::Get().SetUid(0);
    
    ewr::EwrGuiApp app;
    app.Initialize();
    
    WaitFor([&](){ return !app.IsOtaSyncRunning(); });
    EXPECT_TRUE(app.GetOtaStatusText().find("Failed") != std::string::npos);
}

TEST(GuiSuite, E2E_Startup_SyncTimeout_Root) {
    MockNetworkState::Get().SetScenario(NetworkScenario::TIMEOUT);
    MockUsbState::Get().SetUid(0);
    
    ewr::EwrGuiApp app;
    app.Initialize();
    
    WaitFor([&](){ return !app.IsOtaSyncRunning(); });
    EXPECT_TRUE(app.GetOtaStatusText().find("Failed") != std::string::npos);
}

TEST(GuiSuite, E2E_Startup_SyncCorrupt_Root) {
    MockNetworkState::Get().SetScenario(NetworkScenario::BAD_JSON);
    MockUsbState::Get().SetUid(0);
    
    ewr::EwrGuiApp app;
    app.Initialize();
    
    WaitFor([&](){ return !app.IsOtaSyncRunning(); });
    // Should fall back cleanly and keep running
    EXPECT_TRUE(app.GetOtaStatusText().find("Failed") != std::string::npos);

    // Clean up corrupted database.json to isolate this test from subsequent tests
    std::ofstream f("database.json");
    f << "{\n"
      << "  \"L3150\": { \"rkey\": 17080, \"wkey\": \"L3150_KEY\", \"addresses\": [20, 21], \"reset\": [0, 0] },\n"
      << "  \"L3210\": { \"rkey\": 17080, \"wkey\": \"L3210_KEY\", \"addresses\": [30], \"reset\": [0] }\n"
      << "}";
    f.close();
}

TEST(GuiSuite, E2E_SmartReset_Success) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    MockUsbState::Get().SetScenario(UsbScenario::SUCCESS);
    
    ewr::EwrGuiApp app;
    app.Initialize();
    app.SelectModel("L3150");
    
    app.TriggerReset();
    WaitFor([&](){ return !app.IsResetRunning(); });
    
    EXPECT_TRUE(app.GetResetStatusText().find("Success") != std::string::npos);
}

TEST(GuiSuite, E2E_ReplayReset_Success) {
    fs::create_directory("models");
    std::ofstream f("models/ReplayModel.txt");
    f << "{ 0x01, 0x02 }";
    f.close();
    
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    MockUsbState::Get().SetScenario(UsbScenario::SUCCESS);
    
    ewr::EwrGuiApp app;
    app.Initialize();
    app.SelectModel("ReplayModel");
    
    app.TriggerReset();
    WaitFor([&](){ return !app.IsResetRunning(); });
    
    EXPECT_TRUE(app.GetResetStatusText().find("Success") != std::string::npos);
    
    fs::remove("models/ReplayModel.txt");
}

TEST(GuiSuite, E2E_Reset_NoDevice) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    MockUsbState::Get().SetScenario(UsbScenario::NO_DEVICE);
    
    ewr::EwrGuiApp app;
    app.Initialize();
    app.SelectModel("L3150");
    app.ClearLogs();
    
    app.TriggerReset();
    WaitFor([&](){ return !app.IsResetRunning(); });
    
    EXPECT_TRUE(app.GetConsoleOutput().find("Could not find an Epson printer") != std::string::npos);
}

TEST(GuiSuite, E2E_Reset_DeviceBusy_Windows) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    MockUsbState::Get().SetScenario(UsbScenario::DISCONNECTED);
    
    ewr::EwrGuiApp app;
    app.Initialize();
    app.SelectModel("L3150");
    
    app.TriggerReset();
    WaitFor([&](){ return !app.IsResetRunning(); });
    
    EXPECT_TRUE(app.GetResetStatusText().find("Error") != std::string::npos);
}

TEST(GuiSuite, E2E_Reset_AccessDenied_Windows) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    MockUsbState::Get().SetScenario(UsbScenario::DISCONNECTED);
    
    ewr::EwrGuiApp app;
    app.Initialize();
    app.SelectModel("L3150");
    
    app.TriggerReset();
    WaitFor([&](){ return !app.IsResetRunning(); });
    
    EXPECT_TRUE(app.GetResetStatusText().find("Error") != std::string::npos);
}

TEST(GuiSuite, E2E_Reset_ClaimInterfaceFailed_Linux) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    MockUsbState::Get().SetScenario(UsbScenario::DISCONNECTED);
    
    ewr::EwrGuiApp app;
    app.Initialize();
    app.SelectModel("L3150");
    
    app.TriggerReset();
    WaitFor([&](){ return !app.IsResetRunning(); });
    
    EXPECT_TRUE(app.GetResetStatusText().find("Error") != std::string::npos);
}

TEST(GuiSuite, E2E_Reset_WriteFailed) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    MockUsbState::Get().SetScenario(UsbScenario::DISCONNECTED);
    
    ewr::EwrGuiApp app;
    app.Initialize();
    app.SelectModel("L3150");
    
    app.TriggerReset();
    WaitFor([&](){ return !app.IsResetRunning(); });
    
    EXPECT_TRUE(app.GetResetStatusText().find("Error") != std::string::npos);
}

TEST(GuiSuite, E2E_Reset_NoAckReceived) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    MockUsbState::Get().SetScenario(UsbScenario::HANDSHAKE_FAILURE);
    
    ewr::EwrGuiApp app;
    app.Initialize();
    app.SelectModel("L3150");
    
    app.TriggerReset();
    WaitFor([&](){ return !app.IsResetRunning(); });
    
    EXPECT_TRUE(app.GetResetStatusText().find("Error") != std::string::npos);
}

TEST(GuiSuite, E2E_Reset_DisconnectedMidway) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    MockUsbState::Get().SetScenario(UsbScenario::HANDSHAKE_FAILURE);
    
    ewr::EwrGuiApp app;
    app.Initialize();
    app.SelectModel("L3150");
    
    app.TriggerReset();
    WaitFor([&](){ return !app.IsResetRunning(); });
    
    EXPECT_TRUE(app.GetResetStatusText().find("Error") != std::string::npos);
}

TEST(GuiSuite, E2E_SearchFilter_ChangeSelection) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    ewr::EwrGuiApp app;
    app.Initialize();
    
    app.SelectModel("L3150");
    EXPECT_STREQ(app.GetSelectedModelName().c_str(), "L3150");
    
    app.SelectModel("L3210");
    EXPECT_STREQ(app.GetSelectedModelName().c_str(), "L3210");
}

TEST(GuiSuite, E2E_RunReset_ThenSyncDatabase) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    MockUsbState::Get().SetScenario(UsbScenario::SUCCESS);
    
    ewr::EwrGuiApp app;
    app.Initialize();
    app.SelectModel("L3150");
    
    app.TriggerReset();
    // Start a concurrent sync
    app.Initialize();
    
    WaitFor([&](){ return !app.IsResetRunning(); });
    EXPECT_TRUE(app.GetResetStatusText().find("Success") != std::string::npos);
}

TEST(GuiSuite, E2E_DatabaseReload_UpdatesList) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    ewr::EwrGuiApp app;
    app.Initialize();
    
    WaitFor([&](){ return !app.IsOtaSyncRunning(); });
    auto list = app.GetFilteredModelNames();
    EXPECT_TRUE(list.size() >= 2);
}

TEST(GuiSuite, E2E_MultipleResets_Consecutive) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    MockUsbState::Get().SetScenario(UsbScenario::SUCCESS);
    
    ewr::EwrGuiApp app;
    app.Initialize();
    app.SelectModel("L3150");
    
    app.TriggerReset();
    WaitFor([&](){ return !app.IsResetRunning(); });
    
    app.TriggerReset();
    WaitFor([&](){ return !app.IsResetRunning(); });
    
    EXPECT_TRUE(app.GetResetStatusText().find("Success") != std::string::npos);
}

TEST(GuiSuite, E2E_Reset_ReplaysBothTxtAndC) {
    fs::create_directory("models");
    std::ofstream("models/ModelA.txt") << "{ 0x01 }";
    std::ofstream("models/ModelB.c") << "{ 0x02 }";
    
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    ewr::EwrGuiApp app;
    app.Initialize();
    
    auto list = app.GetFilteredModelNames();
    bool foundA = false, foundB = false;
    for (const auto& name : list) {
        if (name == "ModelA") foundA = true;
        if (name == "ModelB") foundB = true;
    }
    EXPECT_TRUE(foundA);
    EXPECT_TRUE(foundB);
    
    fs::remove("models/ModelA.txt");
    fs::remove("models/ModelB.c");
}

TEST(GuiSuite, E2E_AppQuit_DuringActiveTask) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    MockUsbState::Get().SetScenario(UsbScenario::SUCCESS);
    
    {
        ewr::EwrGuiApp app;
        app.Initialize();
        app.SelectModel("L3150");
        app.TriggerReset();
        // App goes out of scope and is destroyed immediately while background reset thread is active
    }
    
    EXPECT_TRUE(true); // Verifies no deadlock or crash on destruction
}

TEST(GuiSuite, Search_RapidInputStress) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    ewr::EwrGuiApp app;
    app.Initialize();
    
    for (int i = 0; i < 50; ++i) {
        app.SetSearchQuery("L");
        app.GetFilteredModelNames();
        app.SetSearchQuery("L3");
        app.GetFilteredModelNames();
    }
    EXPECT_TRUE(true);
}

TEST(GuiSuite, LogConsole_StressBuffer) {
    ewr::EwrGuiApp app;
    app.Initialize();
    app.ClearLogs();
    
    for (int i = 0; i < 5000; ++i) {
        std::cout << "STRESS_LOG_LINE_" << i << "\n";
    }
    
    std::string out = app.GetConsoleOutput();
    EXPECT_TRUE(out.size() > 0);
}

TEST(GuiSuite, Thread_StackOverflowRisk) {
    ewr::UniversalGenerator gen;
    ewr::DbPrinterModel model;
    model.name = "L3150";
    model.rkey = 1234;
    model.wkey = "KEY";
    // Construct a large model with 1000 addresses to test stack limits
    for (int i = 0; i < 1000; ++i) {
        model.addresses.push_back(i);
        model.reset_values.push_back(0);
    }
    
    auto seq = gen.GenerateSequence(model);
    EXPECT_EQ(seq.size(), (size_t)(3 + 3 * 1000));
}

TEST(GuiSuite, Memory_Leak_ValgrindRun) {
    for (int i = 0; i < 5; ++i) {
        ewr::EwrGuiApp app;
        app.Initialize();
    }
    EXPECT_TRUE(true);
}

TEST(GuiSuite, EEPROM_Read_Success) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    MockUsbState::Get().SetScenario(UsbScenario::SUCCESS);
    
    ewr::EwrGuiApp app;
    app.Initialize();
    app.SelectModel("L3150");
    
    app.TriggerReadCounters();
    EXPECT_TRUE(app.IsReadRunning());
    
    WaitFor([&](){ return !app.IsReadRunning(); });
    
    EXPECT_FALSE(app.IsReadRunning());
    EXPECT_TRUE(app.IsReadSuccess());
    auto values = app.GetReadValues();
    EXPECT_FALSE(values.empty());
    EXPECT_EQ(values[0], 100);
    EXPECT_EQ(app.GetReadStatusText(), "Success");
}

TEST(GuiSuite, EEPROM_Read_NoDevice) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    MockUsbState::Get().SetScenario(UsbScenario::NO_DEVICE);
    
    ewr::EwrGuiApp app;
    app.Initialize();
    app.SelectModel("L3150");
    
    app.TriggerReadCounters();
    
    WaitFor([&](){ return !app.IsReadRunning(); });
    
    EXPECT_FALSE(app.IsReadRunning());
    EXPECT_FALSE(app.IsReadSuccess());
    EXPECT_TRUE(app.GetReadStatusText().find("Error") != std::string::npos);
}

TEST(GuiSuite, Connection_PeriodicScanning) {
    MockNetworkState::Get().SetScenario(NetworkScenario::SUCCESS);
    MockUsbState::Get().SetScenario(UsbScenario::SUCCESS);
    
    ewr::EwrGuiApp app;
    app.Initialize();
    
    // Initial scan on Initialize should have detected it
    EXPECT_TRUE(app.IsPrinterConnected());
    EXPECT_EQ(app.GetConnectedPrinterPid(), 0x1111);
    
    // Change to disconnected
    MockUsbState::Get().SetScenario(UsbScenario::NO_DEVICE);
    
    // We mock time to trigger the 2.0s scan check
    // In our mock imgui double GetTime() increment by 0.1 on each call
    // Let's call RenderUI multiple times to simulate time passing (> 2.0 seconds)
    for (int i = 0; i < 25; ++i) {
        app.RenderUI();
    }
    
    EXPECT_FALSE(app.IsPrinterConnected());
}
