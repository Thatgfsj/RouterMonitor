// HttpClient.h
// Thin WinHTTP wrapper with automatic Cookie management.
// Designed for simple POST-JSON calls to a single router endpoint.
#pragma once

#include <string>
#include <windows.h>
#include <winhttp.h>

namespace rm {

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    // POST application/json to the given absolute URL (e.g. http://192.168.10.1/cgi-bin/middleware.cgi).
    // On success returns true and fills *response with the response body.
    // Cookies received in Set-Cookie response headers are stored and sent on subsequent calls.
    // Sets *http_status on success.
    bool PostJson(const std::wstring& url,
                  const std::string& body,
                  std::string* response,
                  int* http_status = nullptr);

    // Clear all stored cookies.
    void ClearCookies();

    // Returns current Cookie header value (may be empty).
    const std::string& Cookie() const { return cookie_; }

    // Configure timeouts (milliseconds).
    void SetTimeouts(DWORD connect_ms, DWORD receive_ms) {
        connect_timeout_ms_ = connect_ms;
        receive_timeout_ms_ = receive_ms;
    }

private:
    bool EnsureSession();
    void CloseSession();

    HINTERNET session_ = nullptr;
    DWORD connect_timeout_ms_ = 5000;
    DWORD receive_timeout_ms_ = 10000;

    std::string cookie_;       // Current Cookie header value
    std::string last_host_;    // For diagnostics
};

} // namespace rm