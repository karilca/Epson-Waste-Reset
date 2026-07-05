#include "mock_usb.h"
#include <libusb-1.0/libusb.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <iostream>

extern "C" uid_t geteuid(void) {
    return (uid_t)MockUsbState::Get().GetUid();
}

static libusb_device* dummy_dev = (libusb_device*)0x1234;
static libusb_device_handle* dummy_handle = (libusb_device_handle*)0x5678;

extern "C" {

int libusb_init(libusb_context** ctx) {
    return 0;
}

void libusb_exit(libusb_context* ctx) {
}

ssize_t libusb_get_device_list(libusb_context* ctx, libusb_device*** list) {
    if (MockUsbState::Get().GetScenario() == UsbScenario::NO_DEVICE) {
        *list = nullptr;
        return 0;
    }
    
    libusb_device** devs = (libusb_device**)malloc(2 * sizeof(libusb_device*));
    devs[0] = dummy_dev;
    devs[1] = nullptr;
    *list = devs;
    return 1;
}

void libusb_free_device_list(libusb_device** list, int unref_devices) {
    if (list) {
        free(list);
    }
}

int libusb_get_device_descriptor(libusb_device* dev, libusb_device_descriptor* desc) {
    if (dev == dummy_dev) {
        memset(desc, 0, sizeof(*desc));
        desc->idVendor = 0x04b8; // Epson
        desc->idProduct = 0x1111; // Dummy PID
        return 0;
    }
    return -1;
}

int libusb_open(libusb_device* dev, libusb_device_handle** handle) {
    if (MockUsbState::Get().GetScenario() == UsbScenario::DISCONNECTED) {
        return LIBUSB_ERROR_NO_DEVICE;
    }
    if (dev == dummy_dev) {
        *handle = dummy_handle;
        return 0;
    }
    return LIBUSB_ERROR_NO_DEVICE;
}

void libusb_close(libusb_device_handle* handle) {
}

int libusb_get_active_config_descriptor(libusb_device* dev, libusb_config_descriptor** config) {
    if (dev == dummy_dev) {
        libusb_config_descriptor* cfg = (libusb_config_descriptor*)malloc(sizeof(libusb_config_descriptor));
        memset(cfg, 0, sizeof(*cfg));
        cfg->bNumInterfaces = 1;
        
        libusb_interface* iface = (libusb_interface*)malloc(sizeof(libusb_interface));
        memset(iface, 0, sizeof(*iface));
        cfg->interface = iface;
        
        libusb_interface_descriptor* alt = (libusb_interface_descriptor*)malloc(sizeof(libusb_interface_descriptor));
        memset(alt, 0, sizeof(*alt));
        alt->bInterfaceClass = LIBUSB_CLASS_PRINTER;
        alt->bNumEndpoints = 2;
        iface->altsetting = alt;
        
        libusb_endpoint_descriptor* ep = (libusb_endpoint_descriptor*)malloc(2 * sizeof(libusb_endpoint_descriptor));
        memset(ep, 0, 2 * sizeof(*ep));
        ep[0].bmAttributes = LIBUSB_TRANSFER_TYPE_BULK;
        ep[0].bEndpointAddress = 0x81; // EP_IN
        ep[1].bmAttributes = LIBUSB_TRANSFER_TYPE_BULK;
        ep[1].bEndpointAddress = 0x01; // EP_OUT
        alt->endpoint = ep;
        
        *config = cfg;
        return 0;
    }
    return -1;
}

void libusb_free_config_descriptor(libusb_config_descriptor* config) {
    if (config) {
        if (config->interface) {
            if (config->interface->altsetting) {
                if (config->interface->altsetting->endpoint) {
                    free((void*)config->interface->altsetting->endpoint);
                }
                free((void*)config->interface->altsetting);
            }
            free((void*)config->interface);
        }
        free(config);
    }
}

int libusb_claim_interface(libusb_device_handle* handle, int interface_number) {
    return 0;
}

int libusb_release_interface(libusb_device_handle* handle, int interface_number) {
    return 0;
}

int libusb_kernel_driver_active(libusb_device_handle* handle, int interface_number) {
    return 0;
}

int libusb_detach_kernel_driver(libusb_device_handle* handle, int interface_number) {
    return 0;
}

int libusb_attach_kernel_driver(libusb_device_handle* handle, int interface_number) {
    return 0;
}

int libusb_bulk_transfer(libusb_device_handle* handle, unsigned char endpoint, unsigned char* data, int length, int* transferred, unsigned int timeout) {
    if (handle != dummy_handle) {
        return LIBUSB_ERROR_NO_DEVICE;
    }
    
    if (endpoint == 0x01) { // EP_OUT
        std::vector<unsigned char> packet(data, data + length);
        MockUsbState::Get().AddWritePacket(packet);
        *transferred = length;
        return 0;
    } else if (endpoint == 0x81) { // EP_IN
        if (MockUsbState::Get().GetScenario() == UsbScenario::HANDSHAKE_FAILURE) {
            *transferred = 0;
            return LIBUSB_ERROR_TIMEOUT;
        }
        if (length >= 6) {
            if (MockUsbState::Get().ConsumeAck()) {
                data[0] = 0x02;
                data[1] = 0x02;
                data[2] = 0x00;
                data[3] = 0x06;
                data[4] = 0x00;
                data[5] = 0x00;
                *transferred = 6;
                return 0;
            }
        }
        *transferred = 0;
        return 0;
    }
    return -1;
}

} // extern "C"
