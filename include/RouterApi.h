// RouterApi.h
// ZTE RAX3000Ze (CMCC ZXW ZD.01) router API wrapper.
// IMPORTANT: Several request body fields are placeholders (TODO). The library logs every
// request/response so you can compare against browser behavior and adjust the JSON shape.
#pragma once

#include "HttpClient.h"

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace rm {

struct Device {
    std::string mac;            // uppercase, no separator
    std::string devname;        // display name
    std::string hostname;
    std::string ipaddr;
    std::string radio;          // "2.4G" / "5G" / "有线"
    double tx_kbps = 0.0;       // upload speed (KB/s), derived from raw rate
    double rx_kbps = 0.0;       // download speed (KB/s), derived from raw rate
    int64_t uptime_sec = 0;
};

class RouterApi {
public:
    explicit RouterApi(std::string host = "192.168.10.1");

    // Set credentials (called before Login).
    void SetCredentials(std::string user, std::string pass);

    // Attempt login. Returns true on success (result == 0 and we have a sessionId).
    // On failure, *err is populated with a short diagnostic message.
    bool Login(std::string* err = nullptr);

    // Fetch device list. Caller is responsible for ensuring Login succeeded.
    // On failure, returns empty list and *err is populated.
    std::vector<Device> GetDevices(std::string* err = nullptr);

    bool IsLoggedIn() const { return logged_in_; }
    const std::string& Host() const { return host_; }
    const std::string& SessionId() const { return session_id_; }
    // Local IP detected from the login_prepare step (best-effort; empty if unknown).
    const std::string& MyIp() const { return my_ip_; }

    // For diagnostics / verbose mode
    void SetVerbose(bool v) { verbose_ = v; }

private:
    // Sends a JSON body to the middleware endpoint, returns response text.
    bool SendMiddleware(const std::string& body, std::string* response, int* status, std::string* err);

    // Convert raw "txrate"/"rxrate" string (e.g. "0.000" or "33.150") in Mbps to KB/s.
    static double MbpsStringToKBps(const std::string& s);

    HttpClient http_;
    std::string host_;
    std::string user_;
    std::string pass_;
    std::string session_id_;
    std::string my_ip_;   // populated by getaccount step in Login()
    bool logged_in_ = false;
    bool verbose_ = true;
};

} // namespace rm