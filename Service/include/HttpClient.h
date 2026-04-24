#pragma once
#include <string>
#include <windows.h>
#include <vector>

struct HttpResponse {
    int statusCode;
    std::string body;
    std::wstring error;
};

class HttpClient {
public:
    static HttpClient& GetInstance();
    void SetServerUrl(const std::wstring& url);
    HttpResponse Post(const std::wstring& endpoint, const std::string& body, const std::string& authHeader = "");
    HttpResponse Get(const std::wstring& endpoint, const std::string& authHeader = "");

private:
    HttpClient();
    ~HttpClient();
    HttpResponse Request(const std::wstring& method, const std::wstring& endpoint,
                         const std::string& body, const std::string& authHeader);
    std::wstring m_serverUrl;
    int m_port;
    bool m_useHttps;
};