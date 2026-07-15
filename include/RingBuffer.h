// RingBuffer.h
// Thread-safe bounded ring buffer for storing recent samples.
// Used to keep last N seconds of per-device speed samples.
#pragma once

#include <vector>
#include <mutex>
#include <cstddef>

namespace rm {

template <typename T>
class RingBuffer {
public:
    explicit RingBuffer(size_t capacity = 300)
        : capacity_(capacity == 0 ? 1 : capacity), buf_(capacity_) {}

    // Move-only (mutex is non-movable; we rebuild it in the moved-into object).
    RingBuffer(RingBuffer&& other) noexcept
        : capacity_(other.capacity_), buf_(std::move(other.buf_)),
          head_(other.head_), count_(other.count_) {
        other.head_ = 0;
        other.count_ = 0;
    }
    RingBuffer& operator=(RingBuffer&& other) noexcept {
        if (this != &other) {
            capacity_ = other.capacity_;
            buf_ = std::move(other.buf_);
            head_ = other.head_;
            count_ = other.count_;
            other.head_ = 0;
            other.count_ = 0;
        }
        return *this;
    }
    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    void Push(const T& v) {
        std::lock_guard<std::mutex> lk(mu_);
        buf_[head_] = v;
        head_ = (head_ + 1) % capacity_;
        if (count_ < capacity_) ++count_;
    }

    void Push(T&& v) {
        std::lock_guard<std::mutex> lk(mu_);
        buf_[head_] = std::move(v);
        head_ = (head_ + 1) % capacity_;
        if (count_ < capacity_) ++count_;
    }

    size_t Size() const {
        std::lock_guard<std::mutex> lk(mu_);
        return count_;
    }
    size_t Capacity() const { return capacity_; }

    // Returns copy of stored samples in chronological order (oldest -> newest).
    std::vector<T> Snapshot() const {
        std::lock_guard<std::mutex> lk(mu_);
        std::vector<T> out;
        out.reserve(count_);
        if (count_ < capacity_) {
            // 0..head_-1 are filled in order
            for (size_t i = 0; i < count_; ++i) out.push_back(buf_[i]);
        } else {
            // head_ is oldest, capacity-1 is newest, then wrap to head_-1.
            for (size_t i = 0; i < capacity_; ++i) {
                out.push_back(buf_[(head_ + i) % capacity_]);
            }
        }
        return out;
    }

    void Clear() {
        std::lock_guard<std::mutex> lk(mu_);
        head_ = 0;
        count_ = 0;
    }

private:
    mutable std::mutex mu_;
    size_t capacity_;
    std::vector<T> buf_;
    size_t head_ = 0;
    size_t count_ = 0;
};

} // namespace rm