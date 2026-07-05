#pragma once
#include "ewr/payload.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include <vector>
#include <cstdint>
#include <atomic>

namespace ewr {
    typedef void* EwrDeviceHandle;

    EwrDeviceHandle AutoConnectEpsonPrinter();
    bool ExecutePayloadSequence(EwrDeviceHandle hPrinter, const std::vector<std::vector<unsigned char>>& sequence, const std::atomic<bool>* shutdown_requested = nullptr, std::atomic<float>* progress = nullptr);
    void DisconnectPrinter(EwrDeviceHandle hPrinter);

    bool IsEpsonPrinterConnected(uint16_t& out_pid);
    bool ReadEEPROMAddress(EwrDeviceHandle hPrinter, uint16_t rkey, uint16_t address, uint8_t& out_value);
}
