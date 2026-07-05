#include "mock_usb.h"

MockUsbState::MockUsbState() : scenario_(UsbScenario::SUCCESS), uid_(0), isAdmin_(true), ack_available_(false) {}

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
    ack_available_ = false;
}

void MockUsbState::AddWritePacket(const std::vector<unsigned char>& packet) {
    std::lock_guard<std::mutex> lock(mutex_);
    write_packets_.push_back(packet);
    ack_available_ = true;
}

const std::vector<std::vector<unsigned char>>& MockUsbState::GetWritePackets() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return write_packets_;
}

bool MockUsbState::ConsumeAck() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (ack_available_) {
        ack_available_ = false;
        return true;
    }
    return false;
}

