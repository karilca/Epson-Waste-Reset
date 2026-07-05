#pragma once
#include <sys/types.h>
#include <stdint.h>

typedef void libusb_context;
typedef void libusb_device;
typedef void libusb_device_handle;

struct libusb_device_descriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
};

struct libusb_endpoint_descriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bEndpointAddress;
    uint8_t  bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t  bInterval;
    uint8_t  bRefresh;
    uint8_t  bSynchAddress;
    const unsigned char *extra;
    int extra_length;
};

struct libusb_interface_descriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bInterfaceNumber;
    uint8_t  bAlternateSetting;
    uint8_t  bNumEndpoints;
    uint8_t  bInterfaceClass;
    uint8_t  bInterfaceSubClass;
    uint8_t  bInterfaceProtocol;
    uint8_t  iInterface;
    const struct libusb_endpoint_descriptor *endpoint;
    const unsigned char *extra;
    int extra_length;
};

struct libusb_interface {
    const struct libusb_interface_descriptor *altsetting;
    int num_altsetting;
};

struct libusb_config_descriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wTotalLength;
    uint8_t  bNumInterfaces;
    uint8_t  bConfigurationValue;
    uint8_t  iConfiguration;
    uint8_t  bmAttributes;
    uint8_t  MaxPower;
    const struct libusb_interface *interface;
    const unsigned char *extra;
    int extra_length;
};

#define LIBUSB_CLASS_PRINTER 7
#define LIBUSB_CLASS_VENDOR_SPEC 0xFF
#define LIBUSB_TRANSFER_TYPE_BULK 2
#define LIBUSB_TRANSFER_TYPE_MASK 0x03
#define LIBUSB_ENDPOINT_IN 0x80

#define LIBUSB_ERROR_IO -1
#define LIBUSB_ERROR_INVALID_PARAM -2
#define LIBUSB_ERROR_ACCESS -3
#define LIBUSB_ERROR_NO_DEVICE -4
#define LIBUSB_ERROR_NOT_FOUND -5
#define LIBUSB_ERROR_BUSY -6
#define LIBUSB_ERROR_TIMEOUT -7
#define LIBUSB_ERROR_OVERFLOW -8
#define LIBUSB_ERROR_PIPE -9
#define LIBUSB_ERROR_INTERRUPTED -10
#define LIBUSB_ERROR_NO_MEM -11
#define LIBUSB_ERROR_NOT_SUPPORTED -12
#define LIBUSB_ERROR_OTHER -99

extern "C" {
    int libusb_init(libusb_context** ctx);
    void libusb_exit(libusb_context* ctx);
    ssize_t libusb_get_device_list(libusb_context* ctx, libusb_device*** list);
    void libusb_free_device_list(libusb_device** list, int unref_devices);
    int libusb_get_device_descriptor(libusb_device* dev, struct libusb_device_descriptor* desc);
    int libusb_open(libusb_device* dev, libusb_device_handle** handle);
    void libusb_close(libusb_device_handle* handle);
    int libusb_get_active_config_descriptor(libusb_device* dev, struct libusb_config_descriptor** config);
    void libusb_free_config_descriptor(struct libusb_config_descriptor* config);
    int libusb_claim_interface(libusb_device_handle* handle, int interface_number);
    int libusb_release_interface(libusb_device_handle* handle, int interface_number);
    int libusb_kernel_driver_active(libusb_device_handle* handle, int interface_number);
    int libusb_detach_kernel_driver(libusb_device_handle* handle, int interface_number);
    int libusb_attach_kernel_driver(libusb_device_handle* handle, int interface_number);
    int libusb_bulk_transfer(libusb_device_handle* handle, unsigned char endpoint, unsigned char* data, int length, int* transferred, unsigned int timeout);
}
