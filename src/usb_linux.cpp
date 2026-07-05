#include "ewr/usb.h"
#include "ewr/generator.h"
#include <libusb-1.0/libusb.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <chrono>

namespace ewr {

    unsigned char EP_IN = 0;
    unsigned char EP_OUT = 0;
    int TARGET_INTERFACE = -1;

    std::string HexDump(const unsigned char* data, size_t size)
    {
        if (size == 0)
            return "    (Empty)\n";

        std::ostringstream oss;
        for (size_t i = 0; i < size; ++i)
        {
            oss << std::hex << std::setw(2) << std::setfill('0') << (int)data[i] << " ";
            if ((i + 1) % 16 == 0 || i == size - 1)
            {
                if (i == size - 1 && (i + 1) % 16 != 0)
                {
                    for (size_t p = 0; p < 16 - ((i + 1) % 16); ++p)
                        oss << "   ";
                }

                oss << " | ";
                size_t start = (i / 16) * 16;
                for (size_t j = start; j <= i; ++j)
                    oss << (char)((data[j] >= 32 && data[j] <= 126) ? data[j] : '.');

                oss << "\n";
            }
        }
        return oss.str();
    }

    EwrDeviceHandle AutoConnectEpsonPrinter()
    {
        if (libusb_init(nullptr) < 0)
        {
            std::cerr << "Failed to initialize libusb." << std::endl;
            return nullptr;
        }

        libusb_device** devs;
        ssize_t cnt = libusb_get_device_list(nullptr, &devs);
        if (cnt < 0) 
            return nullptr;

        libusb_device_handle* handle = nullptr;
        std::ofstream logFile("ewr_trace.log", std::ios::out | std::ios::trunc);

        for (ssize_t i = 0; i < cnt; i++)
        {
            libusb_device_descriptor desc;

            if (libusb_get_device_descriptor(devs[i], &desc) < 0)
                continue;

            if (desc.idVendor == 0x04b8) // Epson VID
            {
                if (libusb_open(devs[i], &handle) == 0)
                {
                    libusb_config_descriptor* config;
                    libusb_get_active_config_descriptor(devs[i], &config);

                    int selected_interface = -1;
                    unsigned char selected_ep_in = 0;
                    unsigned char selected_ep_out = 0;
                    bool found_printer_class = false;

                    for (int iface_idx = 0; iface_idx < config->bNumInterfaces; iface_idx++) 
                    {
                        const libusb_interface_descriptor* interdesc = &config->interface[iface_idx].altsetting[0];
                        
                        if (interdesc->bInterfaceClass == LIBUSB_CLASS_PRINTER || interdesc->bInterfaceClass == LIBUSB_CLASS_VENDOR_SPEC) 
                        {
                            unsigned char ep_in = 0;
                            unsigned char ep_out = 0;

                            for (int e = 0; e < interdesc->bNumEndpoints; e++) 
                            {
                                const libusb_endpoint_descriptor* epdesc = &interdesc->endpoint[e];
                                if ((epdesc->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_BULK) 
                                {
                                    if (epdesc->bEndpointAddress & LIBUSB_ENDPOINT_IN)
                                        ep_in = epdesc->bEndpointAddress;
                                    else
                                        ep_out = epdesc->bEndpointAddress;
                                }
                            }

                            if (ep_in != 0 && ep_out != 0)
                            {
                                if (interdesc->bInterfaceClass == LIBUSB_CLASS_PRINTER)
                                {
                                    selected_interface = iface_idx;
                                    selected_ep_in = ep_in;
                                    selected_ep_out = ep_out;
                                    found_printer_class = true;
                                    break;
                                }
                                else if (!found_printer_class && selected_interface == -1) 
                                {
                                    selected_interface = iface_idx;
                                    selected_ep_in = ep_in;
                                    selected_ep_out = ep_out;
                                }
                            }
                        }
                    }

                    if (selected_interface != -1) 
                    {
                        TARGET_INTERFACE = selected_interface;
                        EP_IN = selected_ep_in;
                        EP_OUT = selected_ep_out;
                    }

                    libusb_free_config_descriptor(config);

                    if (TARGET_INTERFACE != -1 && EP_IN != 0 && EP_OUT != 0)
                    {
                        std::cout << "[SUCCESS] Auto-detected Epson Printer (PID: " << std::hex << std::setfill('0') << std::setw(4) << desc.idProduct << ")" << std::dec << std::endl;
                        
                        if (logFile.is_open())
                        {
                            logFile << "EWR Hardware Trace Log (Linux)\n";
                            logFile << "==================================================\n";
                            logFile << "Target VID: 0x04B8 | PID: 0x" << std::hex << desc.idProduct << std::dec << "\n";
                            logFile << "Target Interface: " << TARGET_INTERFACE << " (Printer Class)\n";
                            logFile << "Endpoint OUT: 0x" << std::hex << (int)EP_OUT << " | Endpoint IN: 0x" << (int)EP_IN << std::dec << "\n";
                            logFile << "==================================================\n\n";
                        }

                        if (libusb_kernel_driver_active(handle, TARGET_INTERFACE) == 1)
                        {
                            std::cout << "Detaching kernel driver (CUPS) for exclusive access..." << std::endl;
                            libusb_detach_kernel_driver(handle, TARGET_INTERFACE);
                        }

                        if (libusb_claim_interface(handle, TARGET_INTERFACE) < 0) 
                        {
                            std::cerr << "[!] Failed to claim USB interface." << std::endl;
                            libusb_close(handle);
                            handle = nullptr;
                        }
                        else 
                        {
                            break;
                        }
                    }
                    else 
                    {
                        libusb_close(handle);
                        handle = nullptr;
                    }
                }
            }
        }

        libusb_free_device_list(devs, 1);

        if (logFile.is_open())
            logFile.close();

        if (!handle)
            libusb_exit(nullptr);

        return static_cast<EwrDeviceHandle>(handle);
    }

    void DisconnectPrinter(EwrDeviceHandle hPrinter)
    {
        if (!hPrinter)
            return;

        libusb_device_handle* handle = static_cast<libusb_device_handle*>(hPrinter);

        if (TARGET_INTERFACE != -1) 
        {
            libusb_release_interface(handle, TARGET_INTERFACE);
            libusb_attach_kernel_driver(handle, TARGET_INTERFACE);
        }

        libusb_close(handle);
        libusb_exit(nullptr);
    }

    bool ExecutePayloadSequence(EwrDeviceHandle hPrinter, const std::vector<std::vector<unsigned char>>& sequence, const std::atomic<bool>* shutdown_requested, std::atomic<float>* progress)
    {
        std::cout << "\nExecuting universal Linux hardware state machine..." << std::endl;
        std::cout << "[i] Saving hardware trace to ewr_trace.log for diagnostics." << std::endl;
        
        libusb_device_handle* handle = static_cast<libusb_device_handle*>(hPrinter);
        std::ofstream logFile("ewr_trace.log", std::ios::app);

        int actual_length;
        unsigned char readBuffer[256];
        size_t ackCount = 0;

        for (size_t i = 0; i < sequence.size(); ++i) 
        {
            if (shutdown_requested && shutdown_requested->load())
            {
                std::cout << "[INFO] Reset sequence execution interrupted by user cancellation." << std::endl;
                if (logFile.is_open())
                    logFile << "[INFO] Execution interrupted by cancellation.\n\n";
                break;
            }

            if (logFile.is_open()) 
            {
                logFile << "[OUT] Packet " << i + 1 << " (" << sequence[i].size() << " bytes):\n";
                logFile << HexDump(sequence[i].data(), sequence[i].size()) << "\n";
            }

            int write_status = libusb_bulk_transfer(handle, EP_OUT, (unsigned char*)sequence[i].data(), sequence[i].size(), &actual_length, 2000);

            if (write_status != 0)
            {
                std::cerr << "Failed to send packet " << i + 1 << " (libusb error: " << write_status << ")" << std::endl;

                if (logFile.is_open())
                    logFile << "[!] WRITE FAILED. libusb error: " << write_status << "\n\n";

                if (logFile.is_open())
                    logFile.close();

                return false;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            std::vector<unsigned char> ackData;
            while (true)
            {
                int read_status = libusb_bulk_transfer(handle, EP_IN, readBuffer, sizeof(readBuffer), &actual_length, 250);

                if (read_status == LIBUSB_ERROR_TIMEOUT || actual_length == 0)
                    break;

                if (read_status == 0)
                    ackData.insert(ackData.end(), readBuffer, readBuffer + actual_length);
                else
                    break;
            }

            if (logFile.is_open())
            {
                logFile << "[IN]  ACK (" << ackData.size() << " bytes):\n";
                logFile << HexDump(ackData.data(), ackData.size()) << "\n";
            }

            if (!ackData.empty())
            {
                ackCount++;
                std::cout << "-> Packet " << i + 1 << " / " << sequence.size() << " | Triggered ACK: Cleared " << ackData.size() << " bytes." << std::endl;
            }
            else 
            {
                std::cout << "-> Packet " << i + 1 << " / " << sequence.size() << " | Sent. (No ACK)" << std::endl;
            }

            if (progress)
            {
                *progress = 0.5f + 0.4f * (static_cast<float>(i + 1) / sequence.size());
            }
        }
        
        if (logFile.is_open())
            logFile.close();

        if (shutdown_requested && shutdown_requested->load())
        {
            return false;
        }

        if (ackCount == 0)
        {
            std::cerr << "\n[ERROR] The printer did not acknowledge any packets. The reset sequence was rejected or ignored." << std::endl;
            std::cerr << "[!] Diagnostic tips:" << std::endl;
            std::cerr << "    1. Unplug the printer's USB cable, wait 5 seconds, and plug it back in." << std::endl;
            std::cerr << "    2. Restart the printer and try again." << std::endl;
            std::cerr << "    3. Ensure no other printing software (like CUPS or Epson Status Monitor) is active." << std::endl;
            return false;
        }

        return true;
    }

    bool IsEpsonPrinterConnected(uint16_t& out_pid)
    {
        if (libusb_init(nullptr) < 0)
            return false;

        libusb_device** devs;
        ssize_t cnt = libusb_get_device_list(nullptr, &devs);
        if (cnt < 0)
            return false;

        bool found = false;
        for (ssize_t i = 0; i < cnt; i++)
        {
            libusb_device_descriptor desc;
            if (libusb_get_device_descriptor(devs[i], &desc) < 0)
                continue;

            if (desc.idVendor == 0x04b8) // Epson VID
            {
                out_pid = desc.idProduct;
                found = true;
                break;
            }
        }

        libusb_free_device_list(devs, 1);
        libusb_exit(nullptr);
        return found;
    }

    bool ReadEEPROMAddress(EwrDeviceHandle hPrinter, uint16_t rkey, uint16_t address, uint8_t& out_value)
    {
        if (!hPrinter)
            return false;

        libusb_device_handle* handle = static_cast<libusb_device_handle*>(hPrinter);
        UniversalGenerator gen;
        std::vector<unsigned char> packet = gen.GenerateReadPacket(rkey, address);

        int actual_length;
        int write_status = libusb_bulk_transfer(handle, EP_OUT, packet.data(), packet.size(), &actual_length, 2000);
        if (write_status != 0)
        {
            std::cerr << "[ERROR] ReadEEPROMAddress failed to write read request packet (libusb error: " << write_status << ")" << std::endl;
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        unsigned char readBuffer[256];
        std::vector<unsigned char> responseData;
        while (true)
        {
            int read_status = libusb_bulk_transfer(handle, EP_IN, readBuffer, sizeof(readBuffer), &actual_length, 250);
            if (read_status == LIBUSB_ERROR_TIMEOUT || actual_length == 0)
                break;

            if (read_status == 0)
                responseData.insert(responseData.end(), readBuffer, readBuffer + actual_length);
            else
                break;
        }

        if (responseData.empty())
        {
            std::cerr << "[ERROR] ReadEEPROMAddress received no response from printer." << std::endl;
            return false;
        }

        // 1. Text-based protocol parser (e.g. "@BDC PS\r\nEE:XXXXYY;")
        std::string respStr(responseData.begin(), responseData.end());
        size_t bdc_pos = respStr.find("@BDC");
        size_t ee_pos = respStr.find("EE:");
        if (bdc_pos != std::string::npos || ee_pos != std::string::npos)
        {
            std::stringstream ss;
            ss << std::hex << std::setw(4) << std::setfill('0') << std::uppercase << address;
            std::string addrHex = ss.str();
            
            std::string target = "EE:" + addrHex;
            size_t targetPos = respStr.find(target);
            if (targetPos != std::string::npos)
            {
                size_t valPos = targetPos + target.length();
                if (valPos + 2 <= respStr.length())
                {
                    std::string valHex = respStr.substr(valPos, 2);
                    try {
                        out_value = static_cast<uint8_t>(std::stoul(valHex, nullptr, 16));
                        return true;
                    } catch (...) {
                        // Fallback to binary parsers if conversion fails
                    }
                }
            }
        }

        uint8_t addr_low = address & 0xFF;
        uint8_t addr_high = (address >> 8) & 0xFF;
        for (size_t i = 0; i + 3 < responseData.size(); ++i)
        {
            if (responseData[i] == 0x41 && responseData[i+1] == addr_low && responseData[i+2] == addr_high)
            {
                out_value = responseData[i+3];
                return true;
            }
        }

        for (size_t i = 0; i + 2 < responseData.size(); ++i)
        {
            if (responseData[i] == addr_low && responseData[i+1] == addr_high)
            {
                if (i > 0 && i + 2 < responseData.size())
                {
                    out_value = responseData[i+2];
                    return true;
                }
            }
        }

        // Fallback for Smart Protocol (e.g. L3256) where response does not echo the address.
        // It starts at payload index 10 and contains command 0x41 followed directly by value.
        for (size_t i = 10; i + 1 < responseData.size(); ++i)
        {
            if (responseData[i] == 0x41)
            {
                out_value = responseData[i+1];
                return true;
            }
        }

        std::cerr << "[ERROR] ReadEEPROMAddress could not parse read response (response size: " << responseData.size() << ")" << std::endl;
        std::cerr << "[ERROR] Response Hex:\n" << HexDump(responseData.data(), responseData.size()) << std::endl;
        return false;
    }

    bool SendRawPacket(EwrDeviceHandle hPrinter, const unsigned char* data, size_t size)
    {
        if (!hPrinter || !data || size == 0)
            return false;

        libusb_device_handle* handle = static_cast<libusb_device_handle*>(hPrinter);
        int actual_length;

        int write_status = libusb_bulk_transfer(handle, EP_OUT, (unsigned char*)data, size, &actual_length, 2000);
        if (write_status != 0)
        {
            std::cerr << "[ERROR] SendRawPacket failed (libusb error: " << write_status << ")" << std::endl;
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Read and discard ACK response
        unsigned char readBuffer[256];
        while (true)
        {
            int read_status = libusb_bulk_transfer(handle, EP_IN, readBuffer, sizeof(readBuffer), &actual_length, 250);
            if (read_status == LIBUSB_ERROR_TIMEOUT || actual_length == 0)
                break;
            if (read_status != 0)
                break;
        }

        return true;
    }
}
