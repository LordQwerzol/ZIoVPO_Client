#pragma once
#include "pch.h"
#include <chrono>

class AuthManager {
public:
    static AuthManager& GetInstance();

    bool Login(const std::wstring& username, const std::wstring& password, std::wstring& error);
    void Logout();
    bool IsAuthenticated() const;
    std::wstring GetCurrentUsername() const;
    std::string GetAccessToken() const;

    void StartRefreshThread();
    void StopRefreshThread();

private:
    AuthManager();
    ~AuthManager();

    bool PerformLoginRequest(const std::wstring& username, const std::wstring& password,
                             std::string& outAccessToken, std::string& outRefreshToken, std::wstring& error);
    bool PerformRefreshRequest(const std::string& refreshToken,
                               std::string& outNewAccessToken, std::string& outNewRefreshToken, std::wstring& error);
    bool RefreshTokens();
    std::chrono::system_clock::time_point GetTokenExpiryFromJwt(const std::string& token);

    static DWORD WINAPI RefreshThreadProc(LPVOID lpParam);
    void RefreshLoop();

    std::string m_accessToken;
    std::string m_refreshToken;
    std::wstring m_username;
    std::chrono::system_clock::time_point m_tokenExpiry;
    mutable std::mutex m_mutex;

    HANDLE m_hRefreshThread;
    HANDLE m_hStopEvent;
    bool m_refreshRunning;
};