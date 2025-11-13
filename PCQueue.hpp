














#pragma once

#include <atomic>
#include <cstddef>
#include <utility>
#include <cassert>  // For static_assert

template <typename T, size_t Capacity>
class SPSCQueue {
    static_assert(Capacity > 0 && (Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");

public:
    SPSCQueue() = default;
    ~SPSCQueue() {
        // Drain remaining elements to call destructors if T is non-trivial
        T value;
        while (dequeue(value)) {}
    }

    // Enqueue: Returns true on success, false if full
    bool enqueue(const T& value) {
        size_t wp = write_pos_.load(std::memory_order_relaxed);
        size_t next_wp = (wp + 1) & (Capacity - 1);
        size_t rp = read_pos_.load(std::memory_order_acquire);
        if (next_wp == rp) {
            return false;  // Queue is full
        }
        buffer_[wp] = value;
        write_pos_.store(next_wp, std::memory_order_release);
        return true;
    }

    // Movable overload for efficiency
    bool enqueue(T&& value) {
        size_t wp = write_pos_.load(std::memory_order_relaxed);
        size_t next_wp = (wp + 1) & (Capacity - 1);
        size_t rp = read_pos_.load(std::memory_order_acquire);
        if (next_wp == rp) {
            return false;  // Queue is full
        }
        buffer_[wp] = std::move(value);
        write_pos_.store(next_wp, std::memory_order_release);
        return true;
    }

    // Dequeue: Returns true on success (value filled), false if empty
    bool dequeue(T& value) {
        size_t rp = read_pos_.load(std::memory_order_relaxed);
        size_t wp = write_pos_.load(std::memory_order_acquire);
        if (rp == wp) {
            return false;  // Queue is empty
        }
        value = std::move(buffer_[rp]);
        buffer_[rp].~T();  // Explicit destructor if non-trivial
        size_t next_rp = (rp + 1) & (Capacity - 1);
        read_pos_.store(next_rp, std::memory_order_release);
        return true;
    }

    // Check if empty (from consumer side)
    bool empty() const {
        size_t rp = read_pos_.load(std::memory_order_relaxed);
        size_t wp = write_pos_.load(std::memory_order_acquire);
        return rp == wp;
    }

    // Check if full (from producer side)
    bool full() const {
        size_t wp = write_pos_.load(std::memory_order_relaxed);
        size_t next_wp = (wp + 1) & (Capacity - 1);
        size_t rp = read_pos_.load(std::memory_order_acquire);
        return next_wp == rp;
    }

    // Approximate size (may be stale due to concurrency, but safe for SPSC)
    size_t size() const {
        size_t wp = write_pos_.load(std::memory_order_acquire);
        size_t rp = read_pos_.load(std::memory_order_acquire);
        return (wp - rp + Capacity) & (Capacity - 1);
    }

private:
    alignas(64) std::atomic<size_t> write_pos_{0};
    alignas(64) std::atomic<size_t> read_pos_{0};
    alignas(64) T buffer_[Capacity];
};



template <typename T, size_t Capacity>
class SPMCQueue {
    static_assert(Capacity > 0 && (Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");

public:
    SPMCQueue() = default;
    ~SPMCQueue() {
        // Drain remaining elements (single-threaded cleanup assumed)
        T value;
        while (dequeue(value)) {}
    }

    // Enqueue: Returns true on success, false if full (single producer, no CAS needed)
    bool enqueue(const T& value) {
        size_t wp = write_pos_.load(std::memory_order_relaxed);
        size_t next_wp = (wp + 1) & (Capacity - 1);
        size_t rp = read_pos_.load(std::memory_order_acquire);
        if (next_wp == rp) {
            return false;  // Queue is full
        }
        buffer_[wp] = value;
        write_pos_.store(next_wp, std::memory_order_release);
        return true;
    }

    // Movable overload for efficiency
    bool enqueue(T&& value) {
        size_t wp = write_pos_.load(std::memory_order_relaxed);
        size_t next_wp = (wp + 1) & (Capacity - 1);
        size_t rp = read_pos_.load(std::memory_order_acquire);
        if (next_wp == rp) {
            return false;  // Queue is full
        }
        buffer_[wp] = std::move(value);
        write_pos_.store(next_wp, std::memory_order_release);
        return true;
    }

    // Dequeue: Returns true on success (value filled), false if empty. Uses CAS for multi-consumer safety
    bool dequeue(T& value) {
        size_t rp = read_pos_.load(std::memory_order_relaxed);
        while (true) {
            size_t wp = write_pos_.load(std::memory_order_acquire);
            if (rp == wp) {
                return false;  // Queue is empty
            }
            size_t next_rp = (rp + 1) & (Capacity - 1);
            if (read_pos_.compare_exchange_weak(rp, next_rp, std::memory_order_release, std::memory_order_relaxed)) {
                value = std::move(buffer_[rp]);
                buffer_[rp].~T();  // Explicit destructor if non-trivial
                return true;
            }
            // CAS failed (another consumer advanced it): retry
        }
    }

    // Check if empty (approximate, for guidance only—may be stale)
    bool empty() const {
        size_t rp = read_pos_.load(std::memory_order_relaxed);
        size_t wp = write_pos_.load(std::memory_order_acquire);
        return rp == wp;
    }

    // Check if full (approximate, for producer guidance)
    bool full() const {
        size_t wp = write_pos_.load(std::memory_order_relaxed);
        size_t next_wp = (wp + 1) & (Capacity - 1);
        size_t rp = read_pos_.load(std::memory_order_acquire);
        return next_wp == rp;
    }

    // Approximate size (may be stale due to concurrency)
    size_t size() const {
        size_t wp = write_pos_.load(std::memory_order_acquire);
        size_t rp = read_pos_.load(std::memory_order_acquire);
        return (wp - rp + Capacity) & (Capacity - 1);
    }

private:
    alignas(64) std::atomic<size_t> write_pos_{0};  // Producer-owned
    alignas(64) std::atomic<size_t> read_pos_{0};   // Shared among consumers (CAS-protected)
    alignas(64) T buffer_[Capacity];
};







/*
#include <iostream>
#include <thread>

// Example with int, capacity 4 (power of 2)
SPSCQueue<int, 256> queue;

void producer() {
    for (int i = 0; i < 10; ++i) {
        while (!queue.enqueue(i)) {
            // Spin or yield; in trading, perhaps batch or drop
            std::this_thread::yield();
        }
        std::cout << "Produced: " << i << std::endl;
    }
}

void consumer() {
    int value;
    for (int i = 0; i < 10; ++i) {
        while (!queue.dequeue(value)) {
            std::this_thread::yield();
        }
        std::cout << "Consumed: " << value << std::endl;
    }
}

int main() {
    std::thread prod(producer);
    std::thread cons(consumer);
    prod.join();
    cons.join();
    return 0;
}*/