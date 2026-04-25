#pragma once
#include <string>
#include <mutex>
#include <chrono>
#include <windows.h>

struct LicenseTicket {
    std::wstring status;          // L"ACTIVE", L"BLOCKED"
    std::wstring expirationDate;  // YYYY-MM-DD
    std::string userEmail;
    std::string macAddress;
    bool isBlocked;
    std::chrono::system_clock::time_point validUntil;
    std::string rawTicket;
};

class LicenseManager {
public:
    static LicenseManager& GetInstance();

    bool ActivateProduct(const std::wstring& activationCode, std::wstring& error);
    bool CheckLicenseStatus(std::wstring& error);
    void ClearLicense();
    LicenseTicket GetCurrentLicense() const;
    bool HasValidLicense() const;

    void StartRefreshThread();
    void StopRefreshThread();

private:
    LicenseManager();
    ~LicenseManager();

    bool PerformActivationRequest(const std::wstring& code, std::string& rawResp, std::wstring& error);
    bool PerformCheckRequest(std::string& rawResp, std::wstring& error);
    bool ParseTicketResponse(const std::string& jsonStr, LicenseTicket& ticket, std::wstring& error);
    bool IsTicketValid(const LicenseTicket& ticket) const;

    static DWORD WINAPI RefreshThreadProc(LPVOID lpParam);
    void RefreshLoop();

    LicenseTicket m_currentTicket;
    mutable std::mutex m_mutex;
    HANDLE m_hRefreshThread;
    HANDLE m_hStopEvent;
    bool m_refreshRunning;
};