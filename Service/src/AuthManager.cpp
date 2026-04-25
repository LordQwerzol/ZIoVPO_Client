#include "AuthManager.h"
#include "HttpClient.h"
#include "Utils.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

AuthManager& AuthManager::GetInstance() {
    static AuthManager instance;
    return instance;
}

AuthManager::AuthManager()
    : m_hRefreshThread(nullptr), m_hStopEvent(nullptr), m_refreshRunning(false)
{
    m_hStopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
}

AuthManager::~AuthManager() {
    StopRefreshThread();
    if (m_hStopEvent) CloseHandle(m_hStopEvent);
}

bool AuthManager::Login(const std::wstring& email, const std::wstring& password, std::wstring& error) {
    std::string accessToken, refreshToken;
    if (!PerformLoginRequest(email, password, accessToken, refreshToken, error))
        return false;

    auto expiry = GetTokenExpiryFromJwt(accessToken);
    if (expiry == std::chrono::system_clock::time_point()) {
        error = L"Failed to parse JWT";
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_accessToken = accessToken;
        m_refreshToken = refreshToken;
        m_username = email;
        m_tokenExpiry = expiry;
    }
    StartRefreshThread();
    return true;
}

void AuthManager::Logout() {
    StopRefreshThread();
    std::lock_guard<std::mutex> lock(m_mutex);
    m_accessToken.clear();
    m_refreshToken.clear();
    m_username.clear();
    m_tokenExpiry = std::chrono::system_clock::time_point();
}

bool AuthManager::IsAuthenticated() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return !m_accessToken.empty() && std::chrono::system_clock::now() < m_tokenExpiry;
}

std::wstring AuthManager::GetCurrentUsername() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_username;
}

std::string AuthManager::GetAccessToken() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_accessToken;
}

void AuthManager::StartRefreshThread() {
    if (m_refreshRunning) return;
    ResetEvent(m_hStopEvent);
    m_refreshRunning = true;
    m_hRefreshThread = CreateThread(nullptr, 0, RefreshThreadProc, this, 0, nullptr);
}

void AuthManager::StopRefreshThread() {
    if (!m_refreshRunning) return;
    SetEvent(m_hStopEvent);
    WaitForSingleObject(m_hRefreshThread, INFINITE);
    CloseHandle(m_hRefreshThread);
    m_refreshRunning = false;
}

DWORD WINAPI AuthManager::RefreshThreadProc(LPVOID lpParam) {
    AuthManager* pThis = static_cast<AuthManager*>(lpParam);
    pThis->RefreshLoop();
    return 0;
}

void AuthManager::RefreshLoop() {
    while (WaitForSingleObject(m_hStopEvent, 0) != WAIT_OBJECT_0) {
        std::chrono::seconds waitSec(60);
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto now = std::chrono::system_clock::now();
            if (m_tokenExpiry > now) {
                auto remain = m_tokenExpiry - now - std::chrono::minutes(5);
                if (remain < std::chrono::seconds(30)) remain = std::chrono::seconds(30);
                waitSec = std::chrono::duration_cast<std::chrono::seconds>(remain);
                if (waitSec.count() < 1) waitSec = std::chrono::seconds(1);
            }
        }
        DWORD ms = static_cast<DWORD>(waitSec.count() * 1000);
        if (WaitForSingleObject(m_hStopEvent, ms) == WAIT_OBJECT_0) break;
        RefreshTokens();
    }
}

bool AuthManager::RefreshTokens() {
    std::string refresh;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        refresh = m_refreshToken;
    }
    if (refresh.empty()) return false;

    std::wstring error;
    std::string newAccess, newRefresh;
    if (!PerformRefreshRequest(refresh, newAccess, newRefresh, error))
        return false;

    auto newExpiry = GetTokenExpiryFromJwt(newAccess);
    if (newExpiry == std::chrono::system_clock::time_point())
        return false;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_accessToken = newAccess;
        m_refreshToken = newRefresh;
        m_tokenExpiry = newExpiry;
    }
    return true;
}

bool AuthManager::PerformLoginRequest(const std::wstring& wemail, const std::wstring& wpassword,
                                      std::string& outAccess, std::string& outRefresh, std::wstring& error) {
    std::string email(wemail.begin(), wemail.end());
    std::string password(wpassword.begin(), wpassword.end());
    std::string mac = GetMacAddress();
    json req = {{"email", email}, {"password", password}, {"macAddress", mac}};
    std::string body = req.dump();

    HttpResponse resp = HttpClient::GetInstance().Post(L"/auth/login", body, "");
    if (resp.statusCode != 200) {
        error = L"HTTP " + std::to_wstring(resp.statusCode);
        try {
            auto err = json::parse(resp.body);
            if (err.contains("message"))
                error = L"Server: " + std::wstring(err["message"].get<std::string>().begin(),
                                                   err["message"].get<std::string>().end());
        } catch (...) {}
        return false;
    }

    auto data = json::parse(resp.body);
    if (!data.contains("accessToken") || !data.contains("refreshToken")) {
        error = L"Missing tokens in response";
        return false;
    }
    outAccess = data["accessToken"];
    outRefresh = data["refreshToken"];
    return true;
}

bool AuthManager::PerformRefreshRequest(const std::string& refreshToken,
                                        std::string& outNewAccess, std::string& outNewRefresh,
                                        std::wstring& error) {
    json req = {{"refreshToken", refreshToken}};
    std::string body = req.dump();

    HttpResponse resp = HttpClient::GetInstance().Post(L"/auth/refresh", body, "");
    if (resp.statusCode != 200) {
        error = L"Refresh HTTP " + std::to_wstring(resp.statusCode);
        return false;
    }

    auto data = json::parse(resp.body);
    if (!data.contains("accessToken") || !data.contains("refreshToken")) {
        error = L"Missing tokens in refresh response";
        return false;
    }
    outNewAccess = data["accessToken"];
    outNewRefresh = data["refreshToken"];
    return true;
}

std::chrono::system_clock::time_point AuthManager::GetTokenExpiryFromJwt(const std::string& token) {
    size_t first_dot = token.find('.');
    size_t second_dot = token.find('.', first_dot + 1);
    if (first_dot == std::string::npos || second_dot == std::string::npos)
        return std::chrono::system_clock::now() + std::chrono::hours(1);

    std::string payload_b64 = token.substr(first_dot + 1, second_dot - first_dot - 1);
    std::string payload_json = Base64UrlDecode(payload_b64);
    try {
        auto data = json::parse(payload_json);
        if (data.contains("exp")) {
            long long exp_seconds = data["exp"];
            return std::chrono::system_clock::from_time_t(static_cast<time_t>(exp_seconds));
        }
    } catch (...) {}
    return std::chrono::system_clock::now() + std::chrono::hours(1);
}