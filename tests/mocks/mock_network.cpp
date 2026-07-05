#include "mock_network.h"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <cstdarg>

#ifndef _WIN32
// Curl mock types and values
typedef void CURL;
typedef int CURLcode;
typedef int CURLoption;

#define CURLOPT_URL 10002
#define CURLOPT_WRITEDATA 10001
#define CURLOPT_FOLLOWLOCATION 52

#define CURLE_OK 0
#define CURLE_COULDNT_CONNECT 7
#define CURLE_OPERATION_TIMEDOUT 28

struct CurlMockHandle {
    FILE* fp = nullptr;
    std::string url;
};

static CurlMockHandle g_curl_handle;

extern "C" {
    CURL* curl_easy_init() {
        return reinterpret_cast<CURL*>(&g_curl_handle);
    }
    
    CURLcode curl_easy_setopt(CURL* curl, CURLoption option, ...) {
        va_list args;
        va_start(args, option);
        if (option == CURLOPT_WRITEDATA) {
            g_curl_handle.fp = va_arg(args, FILE*);
        } else if (option == CURLOPT_URL) {
            g_curl_handle.url = va_arg(args, const char*);
        }
        va_end(args);
        return CURLE_OK;
    }
    
    CURLcode curl_easy_perform(CURL* curl) {
        return static_cast<CURLcode>(MockNetworkState::Get().PerformMockCurl(""));
    }
    
    void curl_easy_cleanup(CURL* curl) {
        g_curl_handle.fp = nullptr;
        g_curl_handle.url.clear();
    }
}
#else
#include <windows.h>
#include <urlmon.h>

extern "C" HRESULT WINAPI URLDownloadToFileA(
    LPUNKNOWN pCaller,
    LPCSTR szURL,
    LPCSTR szFileName,
    DWORD dwReserved,
    LPBINDSTATUSCALLBACK lpfnCB
) {
    return static_cast<HRESULT>(MockNetworkState::Get().PerformMockDownload(szFileName));
}
#endif

MockNetworkState::MockNetworkState() : scenario_(NetworkScenario::SUCCESS) {}

MockNetworkState& MockNetworkState::Get() {
    static MockNetworkState instance;
    return instance;
}

void MockNetworkState::SetScenario(NetworkScenario scenario) {
    std::lock_guard<std::mutex> lock(mutex_);
    scenario_ = scenario;
}

NetworkScenario MockNetworkState::GetScenario() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return scenario_;
}

int MockNetworkState::PerformMockCurl(const std::string& dest_path) {
    std::lock_guard<std::mutex> lock(mutex_);
#ifndef _WIN32
    if (scenario_ == NetworkScenario::SUCCESS) {
        if (g_curl_handle.fp) {
            const char* json_data = "{\n"
                                    "  \"L3150\": { \"rkey\": 17080, \"wkey\": \"L3150_KEY\", \"addresses\": [20, 21], \"reset\": [0, 0] },\n"
                                    "  \"L3210\": { \"rkey\": 17080, \"wkey\": \"L3210_KEY\", \"addresses\": [30], \"reset\": [0] }\n"
                                    "}";
            std::fputs(json_data, g_curl_handle.fp);
        }
        return CURLE_OK;
    } else if (scenario_ == NetworkScenario::OFFLINE) {
        return CURLE_COULDNT_CONNECT;
    } else if (scenario_ == NetworkScenario::TIMEOUT) {
        return CURLE_OPERATION_TIMEDOUT;
    } else if (scenario_ == NetworkScenario::BAD_JSON) {
        if (g_curl_handle.fp) {
            const char* corrupt_data = "{\n  \"L3150\": { \"rkey\":";
            std::fputs(corrupt_data, g_curl_handle.fp);
        }
        return CURLE_OK;
    }
#endif
    return 0;
}

long MockNetworkState::PerformMockDownload(const std::string& dest_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (scenario_ == NetworkScenario::SUCCESS) {
        std::ofstream f(dest_path);
        if (f.is_open()) {
            f << "{\n"
              << "  \"L3150\": { \"rkey\": 17080, \"wkey\": \"L3150_KEY\", \"addresses\": [20, 21], \"reset\": [0, 0] },\n"
              << "  \"L3210\": { \"rkey\": 17080, \"wkey\": \"L3210_KEY\", \"addresses\": [30], \"reset\": [0] }\n"
              << "}";
            f.close();
        }
        return 0; // S_OK
    } else if (scenario_ == NetworkScenario::OFFLINE) {
        return 0x800C0005L; // INET_E_RESOURCE_NOT_FOUND
    } else if (scenario_ == NetworkScenario::TIMEOUT) {
        return 0x800C0008L; // INET_E_CONNECTION_TIMEOUT
    } else if (scenario_ == NetworkScenario::BAD_JSON) {
        std::ofstream f(dest_path);
        if (f.is_open()) {
            f << "{\n  \"L3150\": { \"rkey\":";
            f.close();
        }
        return 0; // S_OK
    }
    return 0x800C0005L;
}
