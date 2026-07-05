#pragma once
#include <string>
#include <mutex>

enum class NetworkScenario {
    SUCCESS,
    OFFLINE,
    TIMEOUT,
    BAD_JSON
};

class MockNetworkState {
public:
    static MockNetworkState& Get();
    void SetScenario(NetworkScenario scenario);
    NetworkScenario GetScenario() const;

    int PerformMockCurl(const std::string& dest_path);
    long PerformMockDownload(const std::string& dest_path);

private:
    MockNetworkState();
    NetworkScenario scenario_;
    mutable std::mutex mutex_;
};
