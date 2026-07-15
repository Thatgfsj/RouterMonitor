// Credentials.h
// Router login credentials stored in a separate JSON file (credentials.json),
// decoupled from config.ini so other settings can be edited without touching the
// password, and so users can manage credentials independently.
#pragma once

#include <string>

namespace rm {

struct Credentials {
    std::string host;   // router address, e.g. "192.168.10.1"
    std::string user;   // router admin username
    std::string pass;   // router admin password (plain)

    // True if all three fields are non-empty.
    bool Valid() const { return !host.empty() && !user.empty() && !pass.empty(); }

    // Resolve path to credentials.json: exe-dir first, then %APPDATA%\RouterMonitor.
    // Returns whichever path SHOULD be used (does not check existence).
    static std::wstring DefaultPath();

    // Try to load credentials. Order:
    //   1. <exe-dir>\credentials.json    (preferred — user-editable next to exe)
    //   2. <exe-dir>\accounts.ini        (legacy Config file, base64-encoded)
    //   3. %APPDATA%\RouterMonitor\credentials.json
    // Returns true if any source produced a valid (host,user,pass) triple.
    bool Load();

    // Save current fields to <exe-dir>\credentials.json if writable, else
    // %APPDATA%\RouterMonitor\credentials.json. Creates parent directory as needed.
    bool Save() const;
};

} // namespace rm