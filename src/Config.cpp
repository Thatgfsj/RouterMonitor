// Config.cpp
#include "Config.h"

#include <windows.h>
#include <shlobj.h>
#include <cstdlib>

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <vector>

#pragma comment(lib, "shell32.lib")

namespace rm {

std::wstring Config::DefaultPath() {
    // config.ini lives in %APPDATA%\RouterMonitor\. Credentials are stored
    // separately in credentials.json (see Credentials.h).
    wchar_t appdata[MAX_PATH] = {};
    HRESULT hr = SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appdata);
    if (FAILED(hr) || appdata[0] == 0) {
        wchar_t* env = _wgetenv(L"APPDATA");
        if (!env) return L"config.ini";
        wcsncpy_s(appdata, env, _TRUNCATE);
    }
    std::wstring dir = appdata;
    dir += L"\\RouterMonitor";
    CreateDirectoryW(dir.c_str(), nullptr);
    dir += L"\\config.ini";
    return dir;
}

std::string Config::Get(const std::string& section, const std::string& key, const std::string& def) const {
    std::ifstream f(std::string(current_path_.begin(), current_path_.end()));
    if (!f) return def;
    std::string line, cur_sec;
    while (std::getline(f, line)) {
        // Trim
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) line.pop_back();
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line.front() == '[' && line.back() == ']') {
            cur_sec = line.substr(1, line.size() - 2);
            continue;
        }
        if (cur_sec == section) {
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string k = line.substr(0, eq);
            std::string v = line.substr(eq + 1);
            // Trim k
            while (!k.empty() && k.back() == ' ') k.pop_back();
            while (!k.empty() && k.front() == ' ') k.erase(k.begin());
            if (k == key) return v;
        }
    }
    return def;
}

int Config::GetInt(const std::string& section, const std::string& key, int def) const {
    return std::atoi(Get(section, key, std::to_string(def)).c_str());
}

bool Config::GetBool(const std::string& section, const std::string& key, bool def) const {
    std::string v = Get(section, key, def ? "1" : "0");
    std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c){ return std::tolower(c); });
    return v == "1" || v == "true" || v == "yes" || v == "on";
}

void Config::SetInt(int& dst, const std::string& section, const std::string& key) {
    dst = GetInt(section, key, dst);
}
void Config::SetBool(bool& dst, const std::string& section, const std::string& key) {
    dst = GetBool(section, key, dst);
}

bool Config::Load(const std::wstring& path) {
    current_path_ = path.empty() ? DefaultPath() : path;
    SetInt(poll_interval_ms, "polling", "interval_ms");
    if (poll_interval_ms < 500) poll_interval_ms = 500;
    if (poll_interval_ms > 60000) poll_interval_ms = 60000;

    SetBool(hud_enabled, "ui", "hud_enabled");
    SetBool(start_minimized, "ui", "start_minimized");
    SetInt(history_capacity, "ui", "history_capacity");
    if (history_capacity < 30) history_capacity = 30;
    if (history_capacity > 3600) history_capacity = 3600;
    return true;
}

bool Config::Save(const std::wstring& path) const {
    std::wstring p = path.empty() ? current_path_ : path;
    if (p.empty()) p = DefaultPath();

    std::ofstream f(std::string(p.begin(), p.end()), std::ios::trunc);
    if (!f) return false;
    f << "; RouterMonitor config\n\n";
    f << "[polling]\n";
    f << "interval_ms=" << poll_interval_ms << "\n\n";
    f << "[ui]\n";
    f << "hud_enabled=" << (hud_enabled ? 1 : 0) << "\n";
    f << "start_minimized=" << (start_minimized ? 1 : 0) << "\n";
    f << "history_capacity=" << history_capacity << "\n";
    return true;
}

// ------- Base64 -------

static const char kBase64Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string Base64Encode(const std::string& in) {
    std::string out;
    out.reserve(((in.size() + 2) / 3) * 4);
    int val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) | c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(kBase64Alphabet[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(kBase64Alphabet[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

std::string Base64Decode(const std::string& in) {
    std::string out;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; ++i) T[(unsigned char)kBase64Alphabet[i]] = i;
    int val = 0, valb = -8;
    for (unsigned char c : in) {
        if (c == '=' || c == '\r' || c == '\n' || c == ' ') continue;
        int d = T[c];
        if (d < 0) continue;
        val = (val << 6) | d;
        valb += 6;
        if (valb >= 0) {
            out.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

} // namespace rm