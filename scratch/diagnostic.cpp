#include <iostream>
#include <iomanip>
#include "ewr/usb.h"
#include "ewr/generator.h"

int main() {
    std::cout << "Auto-connecting to printer..." << std::endl;
    ewr::EwrDeviceHandle hPrinter = ewr::AutoConnectEpsonPrinter();
    if (!hPrinter) {
        std::cerr << "Failed to connect to printer. Please run this as root/sudo if needed." << std::endl;
        return 1;
    }
    std::cout << "Successfully connected to printer." << std::endl;

    uint16_t address = 28;
    uint16_t rkey = 13898; // L3256 rkey

    std::cout << "Sending read packet for address " << address << " (rkey: " << rkey << ")..." << std::endl;
    uint8_t val = 0;
    bool success = ewr::ReadEEPROMAddress(hPrinter, rkey, address, val);
    if (success) {
        std::cout << "[SUCCESS] ReadEEPROMAddress succeeded! Value: " << (int)val << std::endl;
    } else {
        std::cout << "[FAILED] ReadEEPROMAddress failed." << std::endl;
    }

    ewr::DisconnectPrinter(hPrinter);
    return 0;
}
