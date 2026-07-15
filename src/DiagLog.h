// DiagLog.h - debug helper. Writes to a file so we can see logs even without console.
#pragma once
#include <windows.h>
#include <cstdio>

namespace rm {

inline void DiagLog(const char* fmt, ...) {
    static HANDLE h_ = INVALID_HANDLE_VALUE;
    if (h_ == INVALID_HANDLE_VALUE) {
        char path[MAX_PATH];
        GetModuleFileNameA(nullptr, path, MAX_PATH);
        char* p = path + strlen(path);
        while (p > path && *p != '\\') --p;
        strcpy(p, "\\routermon.log");
        h_ = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                         nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    }
    if (h_ == INVALID_HANDLE_VALUE) return;
    char buf[2048];
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (n > (int)sizeof(buf) - 1) n = (int)sizeof(buf) - 1;
    DWORD written;
    WriteFile(h_, buf, (DWORD)n, &written, nullptr);
    WriteFile(h_, "\r\n", 2, &written, nullptr);
}

// Sanitized dump: prints bytes that are not \0 as ASCII escapes, useful for binary JSON responses.
inline void DiagLogBytes(const char* label, const void* data, size_t len) {
    static HANDLE h_ = INVALID_HANDLE_VALUE;
    if (h_ == INVALID_HANDLE_VALUE) {
        char path[MAX_PATH];
        GetModuleFileNameA(nullptr, path, MAX_PATH);
        char* p = path + strlen(path);
        while (p > path && *p != '\\') --p;
        strcpy(p, "\\routermon.log");
        h_ = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                         nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    }
    if (h_ == INVALID_HANDLE_VALUE) return;

    // Header
    char hdr[128];
    int nh = _snprintf(hdr, sizeof(hdr), "%s (len=%zu):\n", label, len);
    DWORD written;
    WriteFile(h_, hdr, (DWORD)nh, &written, nullptr);

    // Hex + ASCII dump
    const unsigned char* p = (const unsigned char*)data;
    char line[128];
    for (size_t i = 0; i < len; i += 16) {
        int pos = 0;
        pos += _snprintf(line + pos, sizeof(line) - pos, "%04zx  ", i);
        for (size_t j = 0; j < 16; ++j) {
            if (i + j < len) pos += _snprintf(line + pos, sizeof(line) - pos, "%02x ", p[i+j]);
            else pos += _snprintf(line + pos, sizeof(line) - pos, "   ");
            if (j == 7) line[pos++] = ' ';
        }
        pos += _snprintf(line + pos, sizeof(line) - pos, " |");
        for (size_t j = 0; j < 16 && i + j < len; ++j) {
            unsigned char c = p[i+j];
            line[pos++] = (c >= 32 && c < 127) ? (char)c : '.';
        }
        pos += _snprintf(line + pos, sizeof(line) - pos, "|\n");
        WriteFile(h_, line, (DWORD)pos, &written, nullptr);
    }
}

} // namespace rm