// Credentials.cpp
#include "Credentials.h"
#include "JsonParser.h"
#include "DiagLog.h"

#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <sstream>

#pragma comment(lib, "shell32.lib")

namespace rm {

namespace {

// Path to the directory containing RouterMonitor.exe, with trailing backslash.
std::wstring ExeDir() {
    wchar_t exe_path[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, exe_path, MAX_PATH) > 0) {
        wchar_t* p = exe_path + wcslen(exe_path);
        while (p > exe_path && *p != L'\\') --p;
        if (p > exe_path) {
            *p = 0;
            return exe_path;
        }
    }
    return L"";
}

// %APPDATA%\RouterMonitor (created if missing), with trailing backslash.
std::wstring AppDataDir() {
    wchar_t appdata[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appdata)) ||
        appdata[0] == 0) {
        wchar_t* env = _wgetenv(L"APPDATA");
        if (env) wcsncpy_s(appdata, env, _TRUNCATE);
    }
    std::wstring dir = appdata;
    dir += L"\\RouterMonitor";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir;
}

// Minimal JSON string escaping for our small value set.
std::string JsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)(unsigned char)c);
                    out += buf;
                } else {
                    out.push_back(c);
                }
        }
    }
    return out;
}

bool FileExists(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

bool TryReadAll(const std::wstring& path, std::string* out) {
    std::ifstream f(std::string(path.begin(), path.end()));
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    *out = ss.str();
    return true;
}

// Try to parse a credentials JSON file at the given path. Returns true on success.
bool TryLoadJson(const std::wstring& path, Credentials* c) {
    std::string text;
    if (!TryReadAll(path, &text)) return false;
    std::string err;
    auto root = JsonParse(text, &err);
    if (!root || !root->IsObject()) {
        DiagLog("[Credentials] parse failed for %ls: %s", path.c_str(), err.c_str());
        return false;
    }
    c->host = root->Str("host");
    c->user = root->Str("user");
    c->pass = root->Str("pass");
    return c->Valid();
}

// Parse an INI-style [router] block (legacy accounts.ini), tolerating the
// existing base64-encoded password format used by Config.
bool TryLoadIni(const std::wstring& path, Credentials* c) {
    std::string text;
    if (!TryReadAll(path, &text)) return false;
    std::string cur_sec;
    std::string host, user, pass_b64;
    std::istringstream iss(text);
    std::string line;
    while (std::getline(iss, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) line.pop_back();
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line.front() == '[' && line.back() == ']') {
            cur_sec = line.substr(1, line.size() - 2);
            continue;
        }
        if (cur_sec != "router") continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq);
        std::string v = line.substr(eq + 1);
        while (!k.empty() && k.back() == ' ') k.pop_back();
        while (!k.empty() && k.front() == ' ') k.erase(k.begin());
        if (k == "host") host = v;
        else if (k == "user") user = v;
        else if (k == "pass_b64") pass_b64 = v;
    }
    if (host.empty() || user.empty() || pass_b64.empty()) return false;
    // Reuse Config's Base64Decode via a tiny inline copy (kept local to avoid coupling).
    static const char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int T[256]; for (int i = 0; i < 256; ++i) T[i] = -1;
    for (int i = 0; i < 64; ++i) T[(unsigned char)kB64[i]] = i;
    std::string pass;
    int val = 0, valb = -8;
    for (char c : pass_b64) {
        if (c == '=' || c == ' ' || c == '\r' || c == '\n') continue;
        int d = T[(unsigned char)c];
        if (d < 0) continue;
        val = (val << 6) | d;
        valb += 6;
        if (valb >= 0) { pass.push_back((char)((val >> valb) & 0xFF)); valb -= 8; }
    }
    c->host = host;
    c->user = user;
    c->pass = pass;
    return c->Valid();
}

bool TryWriteAll(const std::wstring& path, const std::string& text) {
    std::ofstream f(std::string(path.begin(), path.end()), std::ios::trunc);
    if (!f) return false;
    f << text;
    return f.good();
}

} // anonymous namespace

std::wstring Credentials::DefaultPath() {
    std::wstring exedir = ExeDir();
    if (!exedir.empty()) {
        std::wstring cand = exedir + L"\\credentials.json";
        if (FileExists(cand)) return cand;
    }
    return AppDataDir() + L"\\credentials.json";
}

bool Credentials::Load() {
    std::wstring exedir = ExeDir();
    // 1) Preferred: <exe-dir>\credentials.json
    if (!exedir.empty() && TryLoadJson(exedir + L"\\credentials.json", this)) {
        DiagLog("[Credentials] loaded from exe-dir/credentials.json host=%s user=%s",
                host.c_str(), user.c_str());
        return true;
    }
    // 2) Legacy: <exe-dir>\accounts.ini
    if (!exedir.empty() && TryLoadIni(exedir + L"\\accounts.ini", this)) {
        DiagLog("[Credentials] migrated legacy accounts.ini host=%s user=%s",
                host.c_str(), user.c_str());
        return true;
    }
    // 3) Fallback: %APPDATA%\RouterMonitor\credentials.json
    std::wstring appdir = AppDataDir();
    if (TryLoadJson(appdir + L"\\credentials.json", this)) {
        DiagLog("[Credentials] loaded from APPDATA/credentials.json host=%s user=%s",
                host.c_str(), user.c_str());
        return true;
    }
    return false;
}

bool Credentials::Save() const {
    std::wstring exedir = ExeDir();
    if (!exedir.empty()) {
        std::wstring target = exedir + L"\\credentials.json";
        std::ostringstream ss;
        ss << "{\n"
           << "  \"host\": \"" << JsonEscape(host) << "\",\n"
           << "  \"user\": \"" << JsonEscape(user) << "\",\n"
           << "  \"pass\": \"" << JsonEscape(pass) << "\"\n"
           << "}\n";
        if (TryWriteAll(target, ss.str())) {
            DiagLog("[Credentials] saved to %ls", target.c_str());
            return true;
        }
    }
    // Fallback to APPDATA
    std::wstring target = AppDataDir() + L"\\credentials.json";
    std::ostringstream ss;
    ss << "{\n"
       << "  \"host\": \"" << JsonEscape(host) << "\",\n"
       << "  \"user\": \"" << JsonEscape(user) << "\",\n"
       << "  \"pass\": \"" << JsonEscape(pass) << "\"\n"
       << "}\n";
    bool ok = TryWriteAll(target, ss.str());
    if (ok) DiagLog("[Credentials] saved to %ls", target.c_str());
    return ok;
}

} // namespace rm