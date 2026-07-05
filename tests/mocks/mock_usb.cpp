#include "mock_usb.h"

MockUsbState::MockUsbState() : scenario_(UsbScenario::SUCCESS), uid_(0), isAdmin_(true) {}

MockUsbState& MockUsbState::Get() {
    static MockUsbState instance;
    return instance;
}

void MockUsbState::SetScenario(UsbScenario scenario) {
    std::lock_guard<std::mutex> lock(mutex_);
    scenario_ = scenario;
}

UsbScenario MockUsbState::GetScenario() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return scenario_;
}

void MockUsbState::SetUid(int uid) {
    std::lock_guard<std::mutex> lock(mutex_);
    uid_ = uid;
}

int MockUsbState::GetUid() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return uid_;
}

void MockUsbState::SetIsAdmin(bool isAdmin) {
    std::lock_guard<std::mutex> lock(mutex_);
    isAdmin_ = isAdmin;
}

bool MockUsbState::IsAdmin() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return isAdmin_;
}

void MockUsbState::ClearTrace() {
    std::lock_guard<std::mutex> lock(mutex_);
    write_packets_.clear();
}

void MockUsbState::AddWritePacket(const std::vector<unsigned char>& packet) {
    std::lock_guard<std::mutex> lock(mutex_);
    write_packets_.push_back(packet);
}

const std::vector<std::vector<unsigned char>>& MockUsbState::GetWritePackets() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return write_packets_;
}
