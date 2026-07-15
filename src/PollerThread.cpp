// PollerThread.cpp
#include "PollerThread.h"

#include <cstdio>

namespace rm {

PollerThread::PollerThread() = default;

PollerThread::~PollerThread() { Stop(); }

void PollerThread::Configure(std::string host, std::string user, std::string pass,
                             int interval_ms, size_t history_capacity) {
    host_ = std::move(host);
    user_ = std::move(user);
    pass_ = std::move(pass);
    interval_ms_ = interval_ms;
    history_capacity_ = history_capacity;
}

void PollerThread::SetOnSnapshot(std::function<void(std::shared_ptr<const Snapshot>)> cb) {
    on_snapshot_ = std::move(cb);
}

void PollerThread::Start() {
    if (running_.exchange(true)) return;
    api_ = std::make_unique<RouterApi>(host_);
    api_->SetCredentials(user_, pass_);
    api_->SetVerbose(true);
    thread_ = CreateThread(nullptr, 0,
        [](LPVOID param) -> DWORD {
            static_cast<PollerThread*>(param)->Worker();
            return 0;
        }, this, 0, nullptr);
}

void PollerThread::Stop() {
    if (!running_.exchange(false)) return;
    if (thread_) {
        WaitForSingleObject(thread_, INFINITE);
        CloseHandle(thread_);
        thread_ = nullptr;
    }
    api_.reset();
}

void PollerThread::Worker() {
    // Use Windows-native GetTickCount64 instead of std::chrono::steady_clock so we don't
    // pull in libwinpthread-1.dll (which provides clock_gettime on MinGW).
    auto get_ms = []() -> int64_t { return (int64_t)GetTickCount64(); };
    auto next_tick = get_ms();

    // Persistent state across ticks (keyed by MAC).
    std::unordered_map<std::string, DeviceState> state;
    bool login_attempted = false;

    while (running_.load()) {
        auto snap = std::make_shared<Snapshot>();
        // Windows-native epoch ms (FileTime → Unix epoch). Avoids std::chrono (libwinpthread).
        FILETIME ft; GetSystemTimeAsFileTime(&ft);
        ULARGE_INTEGER u; u.LowPart = ft.dwLowDateTime; u.HighPart = ft.dwHighDateTime;
        snap->unix_ms = (int64_t)((u.QuadPart - 116444736000000000ULL) / 10000);

        std::string err;

        if (!login_attempted || !api_->IsLoggedIn()) {
            if (api_->Login(&err)) {
                login_attempted = true;
                login_ok_.store(true);
            } else {
                login_ok_.store(false);
                snap->last_error = "login: " + err;
                std::fprintf(stderr, "[Poller] login failed: %s\n", err.c_str());
            }
        }

        if (api_->IsLoggedIn()) {
            auto devices = api_->GetDevices(&err);
            if (!err.empty()) {
                snap->last_error = "get devices: " + err;
                std::fprintf(stderr, "[Poller] get devices failed: %s\n", err.c_str());
                // If session expired (401), force re-login next iteration.
                if (err.find("HTTP 401") != std::string::npos ||
                    err.find("HTTP 403") != std::string::npos) {
                    api_->SetCredentials(user_, pass_);
                    login_attempted = false;
                }
            } else {
                snap->login_ok = true;
                snap->total_tx_kbps = 0;
                snap->total_rx_kbps = 0;
                snap->my_ip = api_->MyIp();
                for (const auto& d : devices) {
                    auto it = state.find(d.mac);
                    if (it == state.end()) {
                        // First time seeing this MAC; create new state.
                        DeviceState ns;
                        ns.info = d;
                        auto inserted = state.emplace(d.mac, std::move(ns));
                        it = inserted.first;
                    } else if (it->second.history.Capacity() != history_capacity_) {
                        // Capacity changed (config update); erase and re-insert.
                        state.erase(it);
                        DeviceState ns;
                        ns.info = d;
                        it = state.emplace(d.mac, std::move(ns)).first;
                    } else {
                        it->second.info = d;
                    }
                    it->second.history.Push(DeviceSample{d.tx_kbps, d.rx_kbps, snap->unix_ms});
                    // Move a copy of DeviceState into the snapshot. Since DeviceState is move-only,
                    // we copy the cheap fields and rebuild the RingBuffer with shared history view.
                    // Simpler approach: deep-copy by constructing a fresh DeviceState here.
                    DeviceState copy;
                    copy.info = it->second.info;
                    // Copy history samples into a new RingBuffer in the snapshot.
                    copy.history = RingBuffer<DeviceSample>(history_capacity_);
                    auto samples = it->second.history.Snapshot();
                    for (const auto& s : samples) copy.history.Push(s);
                    snap->devices.emplace(d.mac, std::move(copy));
                    snap->total_tx_kbps += d.tx_kbps;
                    snap->total_rx_kbps += d.rx_kbps;
                }
            }
        }

        if (on_snapshot_) on_snapshot_(snap);

        next_tick += interval_ms_;
        auto now = get_ms();
        if (next_tick > now) {
            // Sleep in small slices so Stop() can interrupt promptly.
            while (running_.load() && get_ms() < next_tick) {
                Sleep(50);
            }
        } else {
            // We're behind schedule; skip ahead without sleeping.
            next_tick = now;
        }
    }
}

} // namespace rm