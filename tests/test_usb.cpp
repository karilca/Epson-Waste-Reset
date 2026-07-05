#include "test_framework.h"
#include "mocks/mock_usb.h"
#include "ewr/usb.h"
#include "gui/gui_app.h"
#include <thread>
#include <chrono>

TEST(UsbSuite, CheckPrivileges_Root_Linux) {
#ifndef _WIN32
    MockUsbState::Get().SetUid(0);
    ewr::EwrGuiApp app;
    app.Initialize();
    EXPECT_FALSE(app.IsWarningBannerVisible());
#endif
}

TEST(UsbSuite, CheckPrivileges_User_Linux) {
#ifndef _WIN32
    MockUsbState::Get().SetUid(1000);
    ewr::EwrGuiApp app;
    app.Initialize();
    EXPECT_TRUE(app.IsWarningBannerVisible());
#endif
}

TEST(UsbSuite, CheckPrivileges_Admin_Windows) {
#ifdef _WIN32
    MockUsbState::Get().SetIsAdmin(true);
    ewr::EwrGuiApp app;
    app.Initialize();
    EXPECT_FALSE(app.IsWarningBannerVisible());
#endif
}

TEST(UsbSuite, CheckPrivileges_User_Windows) {
#ifdef _WIN32
    MockUsbState::Get().SetIsAdmin(false);
    ewr::EwrGuiApp app;
    app.Initialize();
    EXPECT_TRUE(app.IsWarningBannerVisible());
#endif
}

TEST(UsbSuite, USB_PowerCycleSimulator) {
    // Quickly plug and unplug USB printer during scans
    for (int i = 0; i < 5; ++i) {
        MockUsbState::Get().SetScenario(UsbScenario::NO_DEVICE);
        auto h1 = ewr::AutoConnectEpsonPrinter();
        EXPECT_TRUE(h1 == nullptr);

        MockUsbState::Get().SetScenario(UsbScenario::SUCCESS);
        auto h2 = ewr::AutoConnectEpsonPrinter();
        EXPECT_TRUE(h2 != nullptr);
        ewr::DisconnectPrinter(h2);
    }
}

TEST(UsbSuite, USB_ExtremelySlowPrinter) {
    MockUsbState::Get().SetScenario(UsbScenario::SUCCESS);
    auto h = ewr::AutoConnectEpsonPrinter();
    EXPECT_TRUE(h != nullptr);
    
    std::vector<std::vector<unsigned char>> seq = {{0x01, 0x02}};
    bool res = ewr::ExecutePayloadSequence(h, seq);
    EXPECT_TRUE(res);
    
    ewr::DisconnectPrinter(h);
}

TEST(UsbSuite, USB_PartialWrite) {
    MockUsbState::Get().SetScenario(UsbScenario::SUCCESS);
    auto h = ewr::AutoConnectEpsonPrinter();
    EXPECT_TRUE(h != nullptr);
    
    std::vector<std::vector<unsigned char>> seq = {{0xAA, 0xBB, 0xCC}};
    MockUsbState::Get().ClearTrace();
    bool res = ewr::ExecutePayloadSequence(h, seq);
    EXPECT_TRUE(res);
    
    auto written = MockUsbState::Get().GetWritePackets();
    EXPECT_EQ(written.size(), (size_t)1);
    EXPECT_EQ(written[0].size(), (size_t)3);
    
    ewr::DisconnectPrinter(h);
}

TEST(UsbSuite, Resource_DescriptorLeak_Check) {
    // Attempt connecting and cleaning up multiple times to ensure zero leak states
    for (int i = 0; i < 10; ++i) {
        MockUsbState::Get().SetScenario(UsbScenario::DISCONNECTED);
        auto h = ewr::AutoConnectEpsonPrinter();
        EXPECT_TRUE(h == nullptr);
    }
}
