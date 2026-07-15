// Config.h
// Minimal INI-style config reader/writer (no dependencies).
// Stored at %APPDATA%\RouterMonitor\config.ini
// Note: router credentials are NOT here; see Credentials.h (credentials.json).
#pragma once

#include <string>
#include <unordered_map>

namespace rm {

class Config {
public:
    bool Load(const std::wstring& path = L"");
    bool Save(const std::wstring& path = L"") const;

    // Polling
    int poll_interval_ms = 2000;

    // UI
    bool hud_enabled = true;
    bool start_minimized = false;
    int history_capacity = 300;          // samples; 300 * 2s = 10 min

    // Auto-save on dtor? default true
    bool auto_save_on_exit = true;

    // Resolve full config path (%APPDATA%\RouterMonitor\config.ini).
    static std::wstring DefaultPath();

private:
    std::wstring current_path_;

    std::string Get(const std::string& section, const std::string& key, const std::string& def) const;
    int GetInt(const std::string& section, const std::string& key, int def) const;
    bool GetBool(const std::string& section, const std::string& key, bool def) const;
    void SetInt(int& dst, const std::string& section, const std::string& key);
    void SetBool(bool& dst, const std::string& section, const std::string& key);
};

// Base64 encode/decode (standard RFC 4648). Still used by Credentials.cpp for
// legacy accounts.ini migration.
std::string Base64Encode(const std::string& in);
std::string Base64Decode(const std::string& in);

} // namespace rm