#include "HttpClient.h"
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")


HttpClient& HttpClient::GetInstance() {
    static HttpClient instance;
    return instance;
}

HttpClient::HttpClient() : m_serverUrl(L"localhost"), m_port(8080), m_useHttps(true) {
    // Можно читать URL из реестра или конфига
}

HttpClient::~HttpClient() = default;

void HttpClient::SetServerUrl(const std::wstring& url) {
    m_serverUrl = url;
}

HttpResponse HttpClient::Post(const std::wstring& endpoint, const std::string& body, const std::string& authHeader) {
    return Request(L"POST", endpoint, body, authHeader);
}

HttpResponse HttpClient::Get(const std::wstring& endpoint, const std::string& authHeader) {
    return Request(L"GET", endpoint, "", authHeader);
}

HttpResponse HttpClient::Request(const std::wstring& method, const std::wstring& endpoint,
                                 const std::string& body, const std::string& authHeader) {
    HttpResponse resp{0, "", L""};

    // Функция для логирования ошибок
    // auto LogError = [&](const std::wstring& msg, DWORD err = GetLastError()) {
    //     std::string errMsg = std::to_string(err);
    //     resp.error = msg + L" (code " + std::to_wstring(err) + L")";
    // };

    // 1. WinHttpOpen
    HINTERNET hSession = WinHttpOpen(L"ZIoVPO Service/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        return resp;
    }

    // 2. WinHttpConnect
    HINTERNET hConnect = WinHttpConnect(hSession, m_serverUrl.c_str(), m_port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return resp;
    }

    // 3. WinHttpOpenRequest
    DWORD flags = m_useHttps ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, method.c_str(), endpoint.c_str(),
                                            NULL, NULL, NULL, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return resp;
    }

    // Временно отключаем проверку сертификатов для localhost (только для отладки!)
    if (m_useHttps) {
        DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                         SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                         SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));
    }

    // 4. Установка заголовков
    std::wstring headers = L"Content-Type: application/json\r\n";
    if (!authHeader.empty()) {
        std::wstring wauth(authHeader.begin(), authHeader.end());
        headers += L"Authorization: " + wauth + L"\r\n";
    }

    // 5. WinHttpSendRequest
    BOOL bSend = WinHttpSendRequest(hRequest, headers.c_str(), headers.size(),
                                    (LPVOID)body.c_str(), (DWORD)body.size(), (DWORD)body.size(), 0);
    if (!bSend) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return resp;
    }

    // 6. WinHttpReceiveResponse
    BOOL bRecv = WinHttpReceiveResponse(hRequest, nullptr);
    if (!bRecv) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return resp;
    }

    // 7. Получение статус кода
    DWORD statusCode = 0;
    DWORD size = sizeof(statusCode);
    BOOL bQuery = WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                      NULL, &statusCode, &size, NULL);

    resp.statusCode = statusCode;

    // 8. Чтение тела ответа
    DWORD dwSize = 0;
    std::string responseBody;
    do {
        dwSize = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) {
            break;
        }
        if (dwSize == 0) break;
        std::vector<char> buffer(dwSize + 1);
        DWORD dwDownloaded = 0;
        if (WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded)) {
            responseBody.append(buffer.data(), dwDownloaded);
        } else {
            break;
        }
    } while (dwSize > 0);
    resp.body = responseBody;

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return resp;
}