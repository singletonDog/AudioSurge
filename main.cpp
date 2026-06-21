#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <windows.h>
#include "ring_buffer.h"
#include "wasapi_capture.h"
#include "wasapi_output.h"

static std::atomic<bool> running(true);

static void onSignal(int) {
    std::cout << "\n[信息] 正在停止..." << std::endl;
    running = false;
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    std::cout << "========================================" << std::endl;
    std::cout << "    AudioFlux - 多音箱同步输出" << std::endl;
    std::cout << "========================================" << std::endl;

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    // 共享音频缓冲区（约 5 秒）
    SharedAudioBuffer buffer(480000);

    // 初始化 WASAPI Loopback 捕获
    WasapiCapture capture(buffer);
    if (!capture.initialize()) {
        std::cerr << "[错误] 捕获初始化失败" << std::endl;
        return 1;
    }

    // 初始化 WASAPI 多设备输出
    WasapiOutput output(buffer);
    if (!output.initialize(capture.getSampleRate(), capture.getChannels())) {
        std::cerr << "[错误] 输出初始化失败" << std::endl;
        return 1;
    }

    // 启动捕获，短暂等待缓冲区填充
    capture.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 启动所有输出设备
    if (!output.startAll()) {
        std::cerr << "[错误] 启动输出失败" << std::endl;
        capture.stop();
        return 1;
    }

    std::cout << "\n系统已启动！播放任何音频，所有音箱将同时输出。" << std::endl;
    std::cout << "按 Ctrl+C 停止。" << std::endl;

    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    output.stopAll();
    capture.stop();

    std::cout << "[信息] 程序已退出" << std::endl;
    return 0;
}