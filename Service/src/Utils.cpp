#include "Utils.h"
#include <windows.h>
#include <iphlpapi.h>
#include <vector>
#include <algorithm>
#pragma comment(lib, "iphlpapi.lib")

std::string GetMacAddress() {
    PIP_ADAPTER_INFO pAdapterInfo = (IP_ADAPTER_INFO*)malloc(sizeof(IP_ADAPTER_INFO));
    ULONG ulOutBufLen = sizeof(IP_ADAPTER_INFO);
    DWORD dwRet = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen);
    if (dwRet == ERROR_BUFFER_OVERFLOW) {
        free(pAdapterInfo);
        pAdapterInfo = (IP_ADAPTER_INFO*)malloc(ulOutBufLen);
        dwRet = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen);
    }
    if (dwRet == NO_ERROR) {
        PIP_ADAPTER_INFO pAdapter = pAdapterInfo;
        while (pAdapter) {
            if (pAdapter->Type == MIB_IF_TYPE_ETHERNET) {
                if (pAdapter->AddressLength == 6) {
                    char mac[18] = {0};
                    sprintf_s(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
                              pAdapter->Address[0], pAdapter->Address[1], pAdapter->Address[2],
                              pAdapter->Address[3], pAdapter->Address[4], pAdapter->Address[5]);
                    free(pAdapterInfo);
                    return std::string(mac);
                }
            }
            pAdapter = pAdapter->Next;
        }
    }
    free(pAdapterInfo);
    return "00:00:00:00:00:00";
}

static std::string base64_decode(const std::string& encoded) {
    static const std::string b64chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string decoded;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[b64chars[i]] = i;

    int val = 0, valb = -8;
    for (unsigned char c : encoded) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            decoded.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return decoded;
}

std::string Base64UrlDecode(const std::string& input) {
    std::string base64 = input;
    std::replace(base64.begin(), base64.end(), '-', '+');
    std::replace(base64.begin(), base64.end(), '_', '/');
    switch (base64.size() % 4) {
        case 2: base64 += "=="; break;
        case 3: base64 += "="; break;
    }
    return base64_decode(base64);
}