#pragma once
#include <string>
#include <vector>
#include <mutex>

enum class UsbScenario {
    SUCCESS,
    NO_DEVICE,
    DISCONNECTED,
    HANDSHAKE_FAILURE
};

class MockUsbState {
public:
    static MockUsbState& Get();
    
    void SetScenario(UsbScenario scenario);
    UsbScenario GetScenario() const;
    
    void SetUid(int uid);
    int GetUid() const;
    
    void SetIsAdmin(bool isAdmin);
    bool IsAdmin() const;

    void ClearTrace();
    void AddWritePacket(const std::vector<unsigned char>& packet);
    const std::vector<std::vector<unsigned char>>& GetWritePackets() const;

    bool ConsumeAck();

private:
    MockUsbState();
    UsbScenario scenario_;
    int uid_;
    bool isAdmin_;
    bool ack_available_;
    std::vector<std::vector<unsigned char>> write_packets_;
    mutable std::mutex mutex_;
};

