#pragma once
#include <vector>
#include <mutex>
#include <atomic>
#include <cstring>

/**
 * 多读者环形缓冲区
 * 
 * 一个写者（WASAPI 捕获）写入数据，多个读者（WASAPI 渲染）各自维护独立读取位置。
 * 写入完成后才更新 write_pos_（release 语义），确保读者不会读到半写数据。
 * 读取非阻塞，适合实时音频线程。读者落后太多时自动跳到最新位置。
 */
class SharedAudioBuffer {
public:
    explicit SharedAudioBuffer(size_t capacity)
        : buffer_(capacity), capacity_(capacity) {}

    // 写入数据（捕获线程调用），先写完再更新 write_pos_
    void write(const float* data, size_t count) {
        std::lock_guard<std::mutex> lock(write_mutex_);
        size_t wp = write_pos_.load(std::memory_order_relaxed);
        for (size_t i = 0; i < count; ++i)
            buffer_[(wp + i) % capacity_] = data[i];
        write_pos_.store((wp + count) % capacity_, std::memory_order_release);
    }

    // 非阻塞读取（渲染线程调用），每个读者维护自己的 read_pos
    size_t read(float* data, size_t count, size_t& read_pos) {
        size_t wp = write_pos_.load(std::memory_order_acquire);
        size_t available = (wp >= read_pos) ? (wp - read_pos) : (capacity_ - read_pos + wp);

        // 落后太多则跳到最新位置
        if (available > capacity_ / 2) {
            read_pos = (wp + capacity_ - count) % capacity_;
            available = count;
        }

        size_t to_read = (std::min)(count, available);
        size_t first = (std::min)(to_read, capacity_ - read_pos);
        memcpy(data, &buffer_[read_pos], first * sizeof(float));
        if (first < to_read)
            memcpy(data + first, &buffer_[0], (to_read - first) * sizeof(float));
        if (to_read < count)
            memset(data + to_read, 0, (count - to_read) * sizeof(float));

        read_pos = (read_pos + to_read) % capacity_;
        return to_read;
    }

    size_t getWritePos() const { return write_pos_.load(std::memory_order_relaxed); }

private:
    std::vector<float> buffer_;
    size_t capacity_;
    std::mutex write_mutex_;
    std::atomic<size_t> write_pos_{0};
};