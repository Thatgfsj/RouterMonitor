// RouterApi.cpp
// ZTE RAX3000Ze (CMCC ZXW ZD.01) router API wrapper.
// Protocol (reverse-engineered from router's app.1729070691704.js):
//   1. POST {key:"devinfo", method:"getaccount", cmd:255}            -> user, addr, newpasswd
//   2. POST {key:"devinfo", method:"login_prepare", cmd:255,
//             param:{devipaddr:addr}}                              -> token
//   3. POST {key:"login", method:"login", param:{
//             user, passwd: sha256(token + password), level:3}}    -> sessionId
// All requests go to http://<host>/cgi-bin/middleware.cgi with Content-Type: application/json.
#include "RouterApi.h"
#include "JsonParser.h"
#include "Sha256.h"
#include "DiagLog.h"

#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <unordered_set>
#include <algorithm>

namespace rm {

RouterApi::RouterApi(std::string host) : host_(std::move(host)) {}

void RouterApi::SetCredentials(std::string user, std::string pass) {
    user_ = std::move(user);
    pass_ = std::move(pass);
}

bool RouterApi::SendMiddleware(const std::string& body, std::string* response,
                                int* status, std::string* err) {
    std::wstring url = L"http://" +
        std::wstring(host_.begin(), host_.end()) +
        L"/cgi-bin/middleware.cgi";

    if (verbose_) {
        DiagLog("[RouterApi] POST %ls", url.c_str());
        DiagLog("[RouterApi] Body: %s", body.c_str());
    }

    int st = 0;
    if (!http_.PostJson(url, body, response, &st)) {
        if (err) *err = "HTTP request failed";
        return false;
    }
    if (status) *status = st;
    if (verbose_) {
        DiagLog("[RouterApi] Status: %d", st);
        if (response && !response->empty()) {
            std::string body_view = response->substr(0, 2000);
            if (response->size() > 2000) body_view += "...(truncated)";
            DiagLog("[RouterApi] Response: %s", body_view.c_str());
        }
    }
    return true;
}

double RouterApi::MbpsStringToKBps(const std::string& s) {
    if (s.empty()) return 0.0;
    double mbps = std::strtod(s.c_str(), nullptr);
    return mbps * 1024.0 / 8.0;
}

bool RouterApi::Login(std::string* err) {
    logged_in_ = false;
    session_id_.clear();
    http_.ClearCookies();

    // Step 1: getaccount — fetch user / addr / newpasswd (so we know the local IP).
    std::string my_ip;
    {
        std::ostringstream body;
        body << "{\"key\":\"devinfo\",\"method\":\"getaccount\",\"cmd\":255}";
        std::string response;
        int status = 0;
        std::string dummy_err;
        DiagLog("[RouterApi] Step 1: getaccount");
        if (!SendMiddleware(body.str(), &response, &status, &dummy_err)) {
            DiagLog("[RouterApi] getaccount send failed: %s", dummy_err.c_str());
        } else {
            std::string parse_err;
            auto root = JsonParse(response, &parse_err);
            if (root) {
                my_ip = root->Str("addr");
                my_ip_ = my_ip;   // remember so callers can identify the local machine
                DiagLog("[RouterApi] getaccount: user=%s addr=%s",
                        root->Str("user").c_str(), my_ip.c_str());
            } else {
                DiagLog("[RouterApi] getaccount parse failed: %s", parse_err.c_str());
            }
        }
    }

    // Step 2: login_prepare — fetch the per-session token.
    std::string token;
    {
        std::ostringstream body;
        body << "{\"key\":\"devinfo\",\"method\":\"login_prepare\",\"cmd\":255"
             << ",\"param\":{\"devipaddr\":\"" << (my_ip.empty() ? host_ : my_ip) << "\"}}";
        std::string response;
        int status = 0;
        std::string dummy_err;
        DiagLog("[RouterApi] Step 2: login_prepare");
        if (!SendMiddleware(body.str(), &response, &status, &dummy_err)) {
            if (err) *err = "login_prepare failed: " + dummy_err;
            return false;
        }
        std::string parse_err;
        auto root = JsonParse(response, &parse_err);
        if (!root) {
            if (err) *err = "login_prepare JSON parse: " + parse_err;
            return false;
        }
        int result = (int)root->Num("result", -1);
        if (result != 0) {
            if (err) {
                std::ostringstream e;
                e << "login_prepare result=" << result;
                if (response.size() > 200) response.resize(200);
                e << " resp=" << response;
                *err = e.str();
            }
            return false;
        }
        token = root->Str("token");
        if (token.empty()) {
            if (err) *err = "login_prepare: no token in response";
            return false;
        }
        DiagLog("[RouterApi] token (first 16)=%.16s...", token.c_str());
    }

    // Step 3: actual login with SHA256(token + password).
    {
        std::string passwd_hash = Sha256Hex(token + pass_);
        std::ostringstream body;
        body << "{\"key\":\"login\",\"method\":\"login\",\"param\":{"
             << "\"user\":\"" << user_ << "\","
             << "\"passwd\":\"" << passwd_hash << "\","
             << "\"level\":3}}";
        std::string response;
        int status = 0;
        DiagLog("[RouterApi] Step 3: login (sha256 token+pass)");
        if (!SendMiddleware(body.str(), &response, &status, err)) return false;
        if (status != 200) {
            if (err) { std::ostringstream e; e << "HTTP " << status; *err = e.str(); }
            return false;
        }
        std::string parse_err;
        auto root = JsonParse(response, &parse_err);
        if (!root) {
            if (err) *err = "login JSON parse: " + parse_err;
            return false;
        }
        int result = (int)root->Num("result", -1);
        if (result != 0) {
            if (err) {
                std::ostringstream e;
                e << "login result=" << result;
                if (response.size() > 300) response.resize(300);
                e << " resp=" << response;
                *err = e.str();
            }
            return false;
        }
        session_id_ = root->Str("sessionId");
        if (session_id_.empty()) session_id_ = root->Str("stok");
        if (session_id_.empty()) session_id_ = root->Str("token");
        if (session_id_.empty()) {
            if (err) *err = "login: no sessionId in response";
            return false;
        }
        logged_in_ = true;
        DiagLog("[RouterApi] login OK, sessionId=%s...", session_id_.c_str());
        return true;
    }
}

std::vector<Device> RouterApi::GetDevices(std::string* err) {
    std::vector<Device> out;
    if (!logged_in_) {
        if (err) *err = "not logged in";
        return out;
    }

    // The actual endpoint (reverse-engineered from chunk-3a557042.js):
    // {key: "laninfo", method: "list"}
    // Response: {result, data: [{mac, devname, ipaddr, txrate, rxrate, uptime, ...}, ...]}
    std::ostringstream body;
    body << "{\"key\":\"laninfo\",\"method\":\"list\",\"sessionId\":\""
         << session_id_ << "\"}";
    std::string response;
    int status = 0;
    DiagLog("[RouterApi] GetDevices: key=laninfo method=list");
    if (!SendMiddleware(body.str(), &response, &status, err)) return out;
    if (status != 200) {
        if (err) { std::ostringstream e; e << "HTTP " << status; *err = e.str(); }
        return out;
    }
    std::string parse_err;
    auto root = JsonParse(response, &parse_err);
    if (!root) {
        if (err) *err = "device list JSON parse: " + parse_err;
        return out;
    }
    int result = (int)root->Num("result", 0);
    // Some endpoints (e.g. laninfo) don't return result at all; default 0 means "ok".
    if (result != 0) {
        if (err) {
            std::ostringstream e;
            e << "device list result=" << result;
            if (response.size() > 300) response.resize(300);
            e << " resp=" << response;
            *err = e.str();
        }
        return out;
    }

    const JsonValue* arr = nullptr;
    if (auto* d = root->Find("data"); d && d->IsArray()) arr = d;
    else if (root->IsArray()) arr = root.get();

    if (!arr) {
        if (err) *err = "no data array in device list";
        return out;
    }
    DiagLog("[RouterApi] GetDevices: total=%zu", arr->a.size());

    out.reserve(arr->a.size());
    // Dedup by MAC: keep the entry with highest speed (or any if both zero).
    std::unordered_set<std::string> seen_macs;
    for (size_t i = 0; i < arr->a.size(); ++i) {
        const JsonValue* v = arr->At(i);
        if (!v || !v->IsObject()) continue;
        // Filter: only keep currently-active devices (online + enabled).
        int active = (int)v->Num("active", 0);
        int enable = (int)v->Num("enable", 0);
        if (active == 0 && enable == 0) continue;
        // Skip if neither IP nor MAC known.
        std::string mac = v->Str("mac");
        if (mac.empty()) continue;
        // Dedup
        if (!seen_macs.insert(mac).second) continue;

        Device d;
        d.mac = mac;
        d.devname = v->Str("devname");
        // Try ifaRate.averrxrate/avertxrate first — that's the actual throughput average.
        std::string averrxrate, avertxrate;
        if (auto* r = v->Find("ifaRate"); r && r->IsObject()) {
            averrxrate = r->Str("averrxrate");
            avertxrate = r->Str("avertxrate");
        }
        if (d.devname.empty() || d.devname == "anonymous") {
            std::string hn = v->Str("hostname");
            if (!hn.empty() && hn != "anonymous") {
                d.devname = hn;
            } else {
                d.devname = "设备-" + mac.substr(0, std::min<size_t>(mac.size(), 6));
            }
        }
        d.hostname = v->Str("hostname");
        d.ipaddr = v->Str("ipaddr");
        d.radio = v->Str("radio");
        std::string lanport = v->Str("lanport");
        if (lanport.empty() && (d.radio.empty() || d.radio == "0")) {
            d.radio = "离线";
        } else if (!lanport.empty() && (d.radio.empty() || d.radio == "0")) {
            d.radio = "有线";
        } else if (d.radio.empty()) {
            d.radio = lanport.empty() ? "未知" : "有线";
        }
        if (!avertxrate.empty() && avertxrate != "0") {
            d.tx_kbps = MbpsStringToKBps(avertxrate);
        } else {
            d.tx_kbps = MbpsStringToKBps(v->Str("txrate"));
        }
        if (!averrxrate.empty() && averrxrate != "0") {
            d.rx_kbps = MbpsStringToKBps(averrxrate);
        } else {
            d.rx_kbps = MbpsStringToKBps(v->Str("rxrate"));
        }
        d.uptime_sec = (int64_t)v->Num("uptime", 0);
        out.push_back(std::move(d));
    }
    return out;
}

} // namespace rm