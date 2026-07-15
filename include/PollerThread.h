// PollerThread.h
// Background polling thread. Owns RouterApi + per-device ring buffers.
// Posts snapshots to UI via PostMessage(hwnd, WM_APP+1, 0, snapshot_ptr).
// Snapshots are wrapped in shared_ptr so UI thread can use them safely.
#pragma once

#include "RouterApi.h"
#include "RingBuffer.h"

#include <atomic>
#include <windows.h>      // CreateThread / WaitForSingleObject — avoids libwinpthread DLL dep.
#include <memory>
#include <string>
#include <unordered_map>
#include <functional>

namespace rm {

// One sample point for a single device.
struct DeviceSample {
    double tx_kbps = 0.0;
    double rx_kbps = 0.0;
    int64_t unix_ms = 0;
};

struct DeviceState {
    Device info;             // latest metadata
    RingBuffer<DeviceSample> history;

    DeviceState() = default;
    DeviceState(DeviceState&&) noexcept = default;
    DeviceState& operator=(DeviceState&&) noexcept = default;
    // Copy is implicit-deleted because RingBuffer is non-copyable.
    DeviceState(const DeviceState&) = delete;
    DeviceState& operator=(const DeviceState&) = delete;
};

struct Snapshot {
    std::unordered_map<std::string, DeviceState> devices;  // key = mac
    double total_tx_kbps = 0.0;
    double total_rx_kbps = 0.0;
    int64_t unix_ms = 0;
    bool login_ok = false;
    std::string last_error;
    std::string my_ip;       // detected local IP (empty if unknown)
};

class PollerThread {
public:
    PollerThread();
    ~PollerThread();

    // Configure before Start().
    void Configure(std::string host, std::string user, std::string pass, int interval_ms, size_t history_capacity);

    void Start();
    void Stop();

    // Set callback (called on the worker thread). The callback is responsible
    // for delivering the snapshot to the UI thread (e.g. via PostMessage).
    void SetOnSnapshot(std::function<void(std::shared_ptr<const Snapshot>)> cb);

    bool IsRunning() const { return running_.load(); }
    bool IsLoginOk() const { return login_ok_.load(); }

private:
    void Worker();

    std::unique_ptr<RouterApi> api_;
    std::string host_, user_, pass_;
    int interval_ms_ = 2000;
    size_t history_capacity_ = 300;

    std::atomic<bool> running_{false};
    std::atomic<bool> login_ok_{false};
    HANDLE thread_ = nullptr;
    std::function<void(std::shared_ptr<const Snapshot>)> on_snapshot_;
};

} // namespace rm