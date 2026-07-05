#include "mock_usb.h"

#if defined(_WIN32) || defined(WIN32)
#include <windows.h>
#include <setupapi.h>
#include <initguid.h>
#include <iostream>
#include <cstring>

// Mock implementation of Windows-specific privilege check
extern "C" BOOL WINAPI IsUserAnAdmin(VOID) {
    return MockUsbState::Get().IsAdmin() ? TRUE : FALSE;
}

#define MOCK_FILE_HANDLE ((HANDLE)0xBADF00D)

extern "C" {

HANDLE WINAPI MockCreateFile(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    if (MockUsbState::Get().GetScenario() == UsbScenario::DISCONNECTED) {
        SetLastError(ERROR_FILE_NOT_FOUND);
        return INVALID_HANDLE_VALUE;
    }
    
    std::string path(lpFileName);
    if (path.find("vid_04b8") != std::string::npos || path == "\\\\.\\EWR_MOCK_PRINTER") {
        return MOCK_FILE_HANDLE;
    }
    
    return CreateFileA(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

BOOL WINAPI MockWriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped) {
    if (hFile == MOCK_FILE_HANDLE) {
        const unsigned char* bytes = static_cast<const unsigned char*>(lpBuffer);
        std::vector<unsigned char> packet(bytes, bytes + nNumberOfBytesToWrite);
        MockUsbState::Get().AddWritePacket(packet);
        if (lpNumberOfBytesWritten) {
            *lpNumberOfBytesWritten = nNumberOfBytesToWrite;
        }
        if (lpOverlapped && lpOverlapped->hEvent) {
            SetEvent(lpOverlapped->hEvent);
        }
        return TRUE;
    }
    return WriteFile(hFile, lpBuffer, nNumberOfBytesToWrite, lpNumberOfBytesWritten, lpOverlapped);
}

BOOL WINAPI MockReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped) {
    if (hFile == MOCK_FILE_HANDLE) {
        if (MockUsbState::Get().GetScenario() == UsbScenario::HANDSHAKE_FAILURE) {
            if (lpNumberOfBytesRead) *lpNumberOfBytesRead = 0;
            return FALSE;
        }
        if (nNumberOfBytesToRead >= 6) {
            if (MockUsbState::Get().ConsumeAck()) {
                unsigned char* bytes = static_cast<unsigned char*>(lpBuffer);
                bytes[0] = 0x02;
                bytes[1] = 0x02;
                bytes[2] = 0x00;
                bytes[3] = 0x06;
                bytes[4] = 0x00;
                bytes[5] = 0x00;
                if (lpNumberOfBytesRead) *lpNumberOfBytesRead = 6;
                if (lpOverlapped && lpOverlapped->hEvent) {
                    SetEvent(lpOverlapped->hEvent);
                }
                return TRUE;
            }
        }
        if (lpNumberOfBytesRead) *lpNumberOfBytesRead = 0;
        return TRUE;
    }
    return ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
}

BOOL WINAPI MockCloseHandle(HANDLE hObject) {
    if (hObject == MOCK_FILE_HANDLE) {
        return TRUE;
    }
    return CloseHandle(hObject);
}

BOOL WINAPI MockGetOverlappedResult(HANDLE hFile, LPOVERLAPPED lpOverlapped, LPDWORD lpNumberOfBytesTransferred, BOOL bWait) {
    if (hFile == MOCK_FILE_HANDLE) {
        if (lpNumberOfBytesTransferred) {
            *lpNumberOfBytesTransferred = 6;
        }
        return TRUE;
    }
    return GetOverlappedResult(hFile, lpOverlapped, lpNumberOfBytesTransferred, bWait);
}

DWORD WINAPI MockWaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds) {
    if (MockUsbState::Get().GetScenario() == UsbScenario::HANDSHAKE_FAILURE) {
        return WAIT_TIMEOUT;
    }
    return WAIT_OBJECT_0;
}

BOOL WINAPI MockCancelIo(HANDLE hFile) {
    return TRUE;
}

HDEVINFO WINAPI SetupDiGetClassDevsA(const GUID* ClassGuid, PCSTR Enumerator, HWND hwndParent, DWORD Flags) {
    if (MockUsbState::Get().GetScenario() == UsbScenario::NO_DEVICE) {
        return INVALID_HANDLE_VALUE;
    }
    return reinterpret_cast<HDEVINFO>(0xCAFE);
}

BOOL WINAPI SetupDiEnumDeviceInterfaces(HDEVINFO DeviceInfoSet, PSP_DEVINFO_DATA DeviceInfoData, const GUID* InterfaceClassGuid, DWORD MemberIndex, PSP_DEVICE_INTERFACE_DATA DeviceInterfaceData) {
    if (DeviceInfoSet == reinterpret_cast<HDEVINFO>(0xCAFE) && MemberIndex == 0) {
        if (DeviceInterfaceData) {
            DeviceInterfaceData->cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
        }
        return TRUE;
    }
    SetLastError(ERROR_NO_MORE_ITEMS);
    return FALSE;
}

BOOL WINAPI SetupDiGetDeviceInterfaceDetailA(HDEVINFO DeviceInfoSet, PSP_DEVICE_INTERFACE_DATA DeviceInterfaceData, PSP_DEVICE_INTERFACE_DETAIL_DATA_A DeviceInterfaceDetailData, DWORD DeviceInterfaceDetailDataSize, PDWORD RequiredSize, PSP_DEVINFO_DATA DeviceInfoData) {
    if (RequiredSize) {
        *RequiredSize = 100;
    }
    if (DeviceInterfaceDetailDataSize == 0) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }
    if (DeviceInterfaceDetailData) {
        std::strcpy(DeviceInterfaceDetailData->DevicePath, "\\\\.\\EWR_MOCK_PRINTER");
    }
    return TRUE;
}

BOOL WINAPI SetupDiDestroyDeviceInfoList(HDEVINFO DeviceInfoSet) {
    return TRUE;
}

} // extern "C"
#else
// Empty translation unit helper for non-Windows platforms
void mock_usb_windows_dummy() {}
#endif
