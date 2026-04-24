#include "LicenseManager.h"
#include "HttpClient.h"
#include "AuthManager.h"
#include "Utils.h"
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <ctime>

using json = nlohmann::json;

LicenseManager& LicenseManager::GetInstance() {
    static LicenseManager instance;
    return instance;
}

LicenseManager::LicenseManager()
    : m_hRefreshThread(nullptr), m_hStopEvent(nullptr), m_refreshRunning(false)
{
    m_hStopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    m_currentTicket.status = L"NONE";
}

LicenseManager::~LicenseManager() {
    StopRefreshThread();
    if (m_hStopEvent) CloseHandle(m_hStopEvent);
}

bool LicenseManager::ActivateProduct(const std::wstring& code, std::wstring& error) {
    std::string raw;
    if (!PerformActivationRequest(code, raw, error))
        return false;

    LicenseTicket ticket;
    if (!ParseTicketResponse(raw, ticket, error))
        return false;

    if (!IsTicketValid(ticket)) {
        error = L"License is blocked or expired";
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_currentTicket = ticket;
    }
    StartRefreshThread();
    return true;
}

bool LicenseManager::CheckLicenseStatus(std::wstring& error) {
    std::string raw;
    if (!PerformCheckRequest(raw, error))
        return false;

    LicenseTicket ticket;
    if (!ParseTicketResponse(raw, ticket, error))
        return false;

    bool valid = IsTicketValid(ticket);
    if (!valid)
        error = L"License invalid (blocked/expired)";

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_currentTicket = ticket;
    }
    return valid;
}

void LicenseManager::ClearLicense() {
    StopRefreshThread();
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentTicket.status = L"NONE";
    m_currentTicket.expirationDate.clear();
    m_currentTicket.validUntil = std::chrono::system_clock::time_point();
    m_currentTicket.isBlocked = true;
}

LicenseTicket LicenseManager::GetCurrentLicense() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentTicket;
}

bool LicenseManager::HasValidLicense() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return IsTicketValid(m_currentTicket);
}

bool LicenseManager::PerformActivationRequest(const std::wstring& wcode, std::string& rawResp, std::wstring& error) {
    std::string code(wcode.begin(), wcode.end());
    std::string mac = GetMacAddress();
    json req = {{"activateCod", code}, {"macAddress", mac}};   
    std::string body = req.dump();

    std::string accessToken = AuthManager::GetInstance().GetAccessToken();
    if (accessToken.empty()) {
        error = L"Not authenticated";
        return false;
    }
    std::string authHeader = "Bearer " + accessToken;
    HttpResponse resp = HttpClient::GetInstance().Post(L"/license/activate", body, authHeader);
    if (resp.statusCode != 200) {
        error = L"Activation HTTP " + std::to_wstring(resp.statusCode);
        return false;
    }
    rawResp = resp.body;
    return true;
}

bool LicenseManager::PerformCheckRequest(std::string& rawResp, std::wstring& error) {
    std::string accessToken = AuthManager::GetInstance().GetAccessToken();
    if (accessToken.empty()) {
        error = L"Not authenticated";
        return false;
    }
    std::string mac = GetMacAddress();
    nlohmann::json req = {
        {"productName", "Antivirus Pro"},
        {"macAddress", mac}
    };
    std::string body = req.dump();
    std::string authHeader = "Bearer " + accessToken;
    HttpResponse resp = HttpClient::GetInstance().Post(L"/license/check", body, authHeader);
    if (resp.statusCode != 200) {
        error = L"Check HTTP " + std::to_wstring(resp.statusCode);
        return false;
    }
    rawResp = resp.body;
    return true;
}

bool LicenseManager::ParseTicketResponse(const std::string& jsonStr, LicenseTicket& ticket, std::wstring& error) {
    try {
        auto data = json::parse(jsonStr);
        if (!data.contains("ticket") || !data.contains("signature")) {
            error = L"Missing ticket or signature";
            return false;
        }
        auto t = data["ticket"];
        ticket.userEmail = t.value("userEmail", "");
        ticket.macAddress = t.value("macAddress", "");
        ticket.isBlocked = t.value("blocked", true);
        std::string expDate = t.value("expirationDate", "");

        // Преобразование даты "YYYY-MM-DD" в time_point
        std::tm tm = {};
        std::istringstream ss(expDate);
        ss >> std::get_time(&tm, "%Y-%m-%d");
        if (ss.fail()) {
            error = L"Invalid expirationDate format";
            return false;
        }
        std::time_t tt = std::mktime(&tm);
        ticket.validUntil = std::chrono::system_clock::from_time_t(tt) + std::chrono::hours(24); // конец дня
        ticket.expirationDate = std::wstring(expDate.begin(), expDate.end());

        ticket.status = ticket.isBlocked ? L"BLOCKED" : L"ACTIVE";
        ticket.rawTicket = jsonStr;
        return true;
    } catch (const std::exception& e) {
        error = L"JSON parse error: " + std::wstring(e.what(), e.what() + strlen(e.what()));
        return false;
    }
}

bool LicenseManager::IsTicketValid(const LicenseTicket& ticket) const {
    if (ticket.status != L"ACTIVE") return false;
    if (ticket.isBlocked) return false;
    return std::chrono::system_clock::now() < ticket.validUntil;
}

void LicenseManager::StartRefreshThread() {
    if (m_refreshRunning) return;
    ResetEvent(m_hStopEvent);
    m_refreshRunning = true;
    m_hRefreshThread = CreateThread(nullptr, 0, RefreshThreadProc, this, 0, nullptr);
}

void LicenseManager::StopRefreshThread() {
    if (!m_refreshRunning) return;
    SetEvent(m_hStopEvent);
    WaitForSingleObject(m_hRefreshThread, INFINITE);
    CloseHandle(m_hRefreshThread);
    m_refreshRunning = false;
}

DWORD WINAPI LicenseManager::RefreshThreadProc(LPVOID lpParam) {
    LicenseManager* pThis = static_cast<LicenseManager*>(lpParam);
    pThis->RefreshLoop();
    return 0;
}

void LicenseManager::RefreshLoop() {
    while (WaitForSingleObject(m_hStopEvent, 0) != WAIT_OBJECT_0) {
        std::chrono::seconds waitSec(3600);
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_currentTicket.status == L"ACTIVE") {
                auto remain = m_currentTicket.validUntil - std::chrono::system_clock::now();
                if (remain < std::chrono::seconds(60)) remain = std::chrono::seconds(60);
                waitSec = std::chrono::duration_cast<std::chrono::seconds>(remain / 10);
                if (waitSec < std::chrono::seconds(60)) waitSec = std::chrono::seconds(60);
            }
        }
        DWORD ms = static_cast<DWORD>(waitSec.count() * 1000);
        if (WaitForSingleObject(m_hStopEvent, ms) == WAIT_OBJECT_0) break;
        std::wstring error;
        CheckLicenseStatus(error);
    }
}