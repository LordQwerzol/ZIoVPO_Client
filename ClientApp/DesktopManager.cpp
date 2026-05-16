#include "DesktopManager.h"
#include <windows.h>
#include <wincred.h>
#include <wtsapi32.h>
#include <string>
#include <sstream>
#include <vector>

#pragma comment(lib, "Credui.lib")
#pragma comment(lib, "wtsapi32.lib")

bool DesktopManager::confirmation() {
    CREDUI_INFOW uiInfo = {};
    uiInfo.cbSize = sizeof(uiInfo);
    uiInfo.pszCaptionText = L"Подтверждение завершения";
    uiInfo.pszMessageText = L"Для завершения программы ZIoVPO введите ваш пароль Windows.";
    ULONG authPackage = 0;
    void* authBuffer = nullptr;
    ULONG authBufferSize = 0;
    BOOL save = FALSE;
    constexpr DWORD flags = CREDUIWIN_SECURE_PROMPT | CREDUIWIN_ENUMERATE_CURRENT_USER;
    DWORD status = CredUIPromptForWindowsCredentialsW(
        &uiInfo,
        0,
        &authPackage,
        nullptr,
        0,
        &authBuffer,
        &authBufferSize,
        &save,
        flags
    );
    if (status == ERROR_CANCELLED) {
        return false;
    }
    if (status != NO_ERROR) {
        return false;
    }

    DWORD userLen = 0, domainLen = 0, passLen = 0;
    CredUnPackAuthenticationBufferW(0, authBuffer, authBufferSize,
                                    nullptr, &userLen,
                                    nullptr, &domainLen,
                                    nullptr, &passLen);

    std::vector<wchar_t> userNameBuf(userLen + 1, L'\0');
    std::vector<wchar_t> domainBuf(domainLen + 1, L'\0');
    std::vector<wchar_t> passwordBuf(passLen + 1, L'\0');

    if (!CredUnPackAuthenticationBufferW(0, authBuffer, authBufferSize,
                                        userNameBuf.data(), &userLen,
                                        domainBuf.data(), &domainLen,
                                        passwordBuf.data(), &passLen)) {
        CoTaskMemFree(authBuffer);
        return false;
    }
    CoTaskMemFree(authBuffer);

    std::wstring rawUserName = userNameBuf.data();
    std::wstring domainFromCred = domainBuf.data();
    std::wstring password = passwordBuf.data();

    // --- Разбор rawUserName ---
    std::wstring userName, domain;
    size_t backslash = rawUserName.find(L'\\');
    size_t at = rawUserName.find(L'@');

    if (backslash != std::wstring::npos) {
        // Формат DOMAIN\User
        domain = rawUserName.substr(0, backslash);
        userName = rawUserName.substr(backslash + 1);
    } else if (at != std::wstring::npos) {
        // Формат user@domain
        userName = rawUserName.substr(0, at);
        domain = rawUserName.substr(at + 1);
    } else {
        // Обычное имя без домена
        userName = rawUserName;
        domain = domainFromCred; // может быть пусто
    }

    // Если домен всё ещё пуст, получаем из токена текущего процесса
    if (domain.empty()) {
        HANDLE hToken = nullptr;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
            DWORD size = 0;
            GetTokenInformation(hToken, TokenUser, nullptr, 0, &size);
            PTOKEN_USER userInfo = (PTOKEN_USER)malloc(size);
            if (userInfo && GetTokenInformation(hToken, TokenUser, userInfo, size, &size)) {
                WCHAR domainName[256];
                DWORD domainLen = 256;
                SID_NAME_USE sidType;
                WCHAR userName2[256];
                DWORD userNameLen = 256;
                if (LookupAccountSidW(nullptr, userInfo->User.Sid, userName2, &userNameLen, domainName, &domainLen, &sidType)) {
                    domain = domainName;
                }
            }
            free(userInfo);
            CloseHandle(hToken);
        }
    }

    // --- Попытки LogonUser ---
    HANDLE token = nullptr;
    BOOL logonOk = FALSE;

    // Попытка 1: LOGON32_LOGON_NETWORK
    logonOk = LogonUserW(
        userName.c_str(),
        domain.empty() ? nullptr : domain.c_str(),
        password.c_str(),
        LOGON32_LOGON_NETWORK,
        LOGON32_PROVIDER_DEFAULT,
        &token
    );

    if (!logonOk && GetLastError() == 1326) {
        // Попытка 2: INTERACTIVE
        logonOk = LogonUserW(
            userName.c_str(),
            domain.empty() ? nullptr : domain.c_str(),
            password.c_str(),
            LOGON32_LOGON_INTERACTIVE,
            LOGON32_PROVIDER_DEFAULT,
            &token
        );
    }

    if (!logonOk && GetLastError() == 1326 && domain.empty()) {
        // Попытка 3: локальный домен "."
        logonOk = LogonUserW(
            userName.c_str(),
            L".",
            password.c_str(),
            LOGON32_LOGON_INTERACTIVE,
            LOGON32_PROVIDER_DEFAULT,
            &token
        );
    }

    // Затираем пароль
    SecureZeroMemory(passwordBuf.data(), passwordBuf.size() * sizeof(wchar_t));
    SecureZeroMemory(&password[0], password.size() * sizeof(wchar_t));

    if (!logonOk) {
        DWORD err = GetLastError();
        return false;
    }
    CloseHandle(token);
    return true;
}