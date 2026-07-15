// HttpClient.cpp
#include "HttpClient.h"

#include <vector>
#include <cstdio>

#pragma comment(lib, "winhttp.lib")

namespace rm {

HttpClient::HttpClient() = default;

HttpClient::~HttpClient() {
    CloseSession();
}

void HttpClient::CloseSession() {
    if (session_) {
        WinHttpCloseHandle(session_);
        session_ = nullptr;
    }
}

bool HttpClient::EnsureSession() {
    if (session_) return true;
    session_ = WinHttpOpen(L"RouterMonitor/0.1",
                            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                            WINHTTP_NO_PROXY_NAME,
                            WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session_) {
        std::fprintf(stderr, "[HttpClient] WinHttpOpen failed: %lu\n", GetLastError());
        return false;
    }
    return true;
}

void HttpClient::ClearCookies() {
    cookie_.clear();
}

// Split "name=value" pair by '='.
static bool SplitCookiePair(const std::string& pair, std::string* name, std::string* value) {
    auto eq = pair.find('=');
    if (eq == std::string::npos) return false;
    *name = pair.substr(0, eq);
    *value = pair.substr(eq + 1);
    return !name->empty();
}

// Parse the "Cookie" header from the next request, merging with existing cookies.
// Keeps last occurrence of each name (RFC 6265-ish).
static std::string MergeCookieHeader(const std::string& existing, const std::string& new_set_cookie) {
    // Parse existing into map
    std::vector<std::pair<std::string, std::string>> cookies;
    auto parse_list = [&](const std::string& str) {
        size_t i = 0;
        while (i < str.size()) {
            while (i < str.size() && (str[i] == ' ' || str[i] == ';')) ++i;
            size_t start = i;
            while (i < str.size() && str[i] != ';') ++i;
            std::string pair = str.substr(start, i - start);
            while (!pair.empty() && pair.back() == ' ') pair.pop_back();
            std::string n, v;
            if (SplitCookiePair(pair, &n, &v)) {
                cookies.emplace_back(n, v);
            }
        }
    };
    if (!existing.empty()) parse_list(existing);

    // New cookie: take the first 'name=value' before any ';' attributes.
    std::string n, v;
    size_t semi = new_set_cookie.find(';');
    std::string first = (semi == std::string::npos) ? new_set_cookie : new_set_cookie.substr(0, semi);
    if (SplitCookiePair(first, &n, &v)) {
        bool replaced = false;
        for (auto& kv : cookies) {
            if (kv.first == n) {
                kv.second = v;
                replaced = true;
                break;
            }
        }
        if (!replaced) cookies.emplace_back(n, v);
    }

    std::string out;
    for (size_t i = 0; i < cookies.size(); ++i) {
        if (i) out += "; ";
        out += cookies[i].first + "=" + cookies[i].second;
    }
    return out;
}

bool HttpClient::PostJson(const std::wstring& url,
                          const std::string& body,
                          std::string* response,
                          int* http_status) {
    if (response) response->clear();
    if (http_status) *http_status = 0;

    if (!EnsureSession()) return false;

    // Parse URL.
    URL_COMPONENTS uc = {};
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256] = {};
    wchar_t path[2048] = {};
    uc.lpszHostName = host;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = 2048;
    if (!WinHttpCrackUrl(url.c_str(), (DWORD)url.size(), 0, &uc)) {
        std::fprintf(stderr, "[HttpClient] WinHttpCrackUrl failed: %lu\n", GetLastError());
        return false;
    }
    // Convert wide host to UTF-8 for diagnostics
    char host_utf8[256] = {};
    WideCharToMultiByte(CP_UTF8, 0, host, uc.dwHostNameLength,
                        host_utf8, sizeof(host_utf8) - 1, nullptr, nullptr);
    last_host_ = host_utf8;

    HINTERNET connect = WinHttpConnect(session_, host, uc.nPort, 0);
    if (!connect) {
        std::fprintf(stderr, "[HttpClient] WinHttpConnect failed: %lu\n", GetLastError());
        return false;
    }

    DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(connect, L"POST", path,
                                          nullptr, WINHTTP_NO_REFERER,
                                          WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!request) {
        std::fprintf(stderr, "[HttpClient] WinHttpOpenRequest failed: %lu\n", GetLastError());
        WinHttpCloseHandle(connect);
        return false;
    }

    // Timeouts
    WinHttpSetTimeouts(request,
                       connect_timeout_ms_, connect_timeout_ms_,
                       receive_timeout_ms_, receive_timeout_ms_);

    // Headers
    std::wstring headers = L"Content-Type: application/json\r\n";
    headers += L"Accept: application/json, text/plain, */*\r\n";
    if (!cookie_.empty()) {
        std::wstring cookie_hdr = L"Cookie: ";
        cookie_hdr += std::wstring(cookie_.begin(), cookie_.end());
        cookie_hdr += L"\r\n";
        headers += cookie_hdr;
    }

    BOOL ok = WinHttpSendRequest(request,
                                 headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
                                 (DWORD)headers.size(),
                                 (LPVOID)body.data(), (DWORD)body.size(),
                                 (DWORD)body.size(), 0);
    if (!ok) {
        std::fprintf(stderr, "[HttpClient] WinHttpSendRequest failed: %lu\n", GetLastError());
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        return false;
    }
    ok = WinHttpReceiveResponse(request, nullptr);
    if (!ok) {
        std::fprintf(stderr, "[HttpClient] WinHttpReceiveResponse failed: %lu\n", GetLastError());
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        return false;
    }

    // Status code
    DWORD status = 0;
    DWORD status_size = sizeof(status);
    WinHttpQueryHeaders(request,
                        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size, WINHTTP_NO_HEADER_INDEX);
    if (http_status) *http_status = (int)status;

    // Capture Set-Cookie response headers and merge.
    DWORD header_len = 0;
    WinHttpQueryHeaders(request, WINHTTP_QUERY_SET_COOKIE,
                        WINHTTP_HEADER_NAME_BY_INDEX, WINHTTP_NO_OUTPUT_BUFFER,
                        &header_len, WINHTTP_NO_HEADER_INDEX);
    if (header_len > 0) {
        std::vector<wchar_t> buf(header_len / sizeof(wchar_t) + 1);
        if (WinHttpQueryHeaders(request, WINHTTP_QUERY_SET_COOKIE,
                                WINHTTP_HEADER_NAME_BY_INDEX, buf.data(),
                                &header_len, WINHTTP_NO_HEADER_INDEX)) {
            int utf8_len = WideCharToMultiByte(CP_UTF8, 0, buf.data(),
                                               (int)(header_len / sizeof(wchar_t)),
                                               nullptr, 0, nullptr, nullptr);
            if (utf8_len > 0) {
                std::string sc(utf8_len, '\0');
                WideCharToMultiByte(CP_UTF8, 0, buf.data(),
                                    (int)(header_len / sizeof(wchar_t)),
                                    sc.data(), utf8_len, nullptr, nullptr);
                cookie_ = MergeCookieHeader(cookie_, sc);
            }
        }
    }

    // Read body
    if (response) {
        std::string out;
        for (;;) {
            DWORD avail = 0;
            if (!WinHttpQueryDataAvailable(request, &avail)) break;
            if (avail == 0) break;
            std::vector<char> chunk(avail);
            DWORD read = 0;
            if (!WinHttpReadData(request, chunk.data(), avail, &read)) break;
            if (read == 0) break;
            out.append(chunk.data(), read);
        }
        *response = std::move(out);
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    return true;
}

} // namespace rm