#pragma once
#include <vector>
#include <atomic>
#include <cstring>

/**
 * 多读者环形缓冲区（无锁）
 *
 * 一个写者（WASAPI 捕获）写入数据，多个读者（WASAPI 渲染）各自维护独立读取位置。
 * 完全无锁设计，适合实时音频线程。
 *
 * 写入：先写入数据，再用 release 语义更新 write_pos_，确保读者不会读到半写数据。
 * 读取：非阻塞，用 acquire 语义读取 write_pos_。读者落后太多时自动跳到最新位置。
 */
class SharedAudioBuffer {
public:
    explicit SharedAudioBuffer(size_t capacity)
        : buffer_(capacity), capacity_(capacity) {}

    // 写入数据（捕获线程调用），无锁
    void write(const float* data, size_t count) {
        // 防御：单次写入量不应超过容量，超出则只保留最新部分，避免越界
        if (count > capacity_) {
            data += (count - capacity_);
            count = capacity_;
        }
        size_t wp = write_pos_.load(std::memory_order_relaxed);
        // 分两段写入（处理环绕）
        size_t first = (std::min)(count, capacity_ - wp);
        memcpy(&buffer_[wp], data, first * sizeof(float));
        if (first < count)
            memcpy(&buffer_[0], data + first, (count - first) * sizeof(float));
        // release 语义：确保数据写入在 write_pos_ 更新之前完成
        write_pos_.store((wp + count) % capacity_, std::memory_order_release);
    }

    // 非阻塞读取（渲染线程调用），每个读者维护自己的 read_pos
    // jumped: 可选输出参数，如果读取位置被跳跃调整则为true
    size_t read(float* data, size_t count, size_t& read_pos, bool* jumped = nullptr) {
        // 防御：读取量不应超过容量，避免跳跃分支下溢
        if (count > capacity_) count = capacity_;
        // acquire 语义：确保读到最新写入的数据
        size_t wp = write_pos_.load(std::memory_order_acquire);
        size_t available = (wp >= read_pos) ? (wp - read_pos) : (capacity_ - read_pos + wp);

        // 落后太多则跳到最新位置（避免播放过时数据产生噪音）
        bool did_jump = false;
        if (available > capacity_ / 2) {
            read_pos = (wp + capacity_ - count) % capacity_;
            available = count;
            did_jump = true;
        }

        size_t to_read = (std::min)(count, available);
        size_t first = (std::min)(to_read, capacity_ - read_pos);
        memcpy(data, &buffer_[read_pos], first * sizeof(float));
        if (first < to_read)
            memcpy(data + first, &buffer_[0], (to_read - first) * sizeof(float));
        if (to_read < count)
            memset(data + to_read, 0, (count - to_read) * sizeof(float));

        read_pos = (read_pos + to_read) % capacity_;
        if (jumped) *jumped = did_jump;
        return to_read;
    }

    size_t getWritePos() const { return write_pos_.load(std::memory_order_relaxed); }

private:
    std::vector<float> buffer_;
    size_t capacity_;
    std::atomic<size_t> write_pos_{0};
};
