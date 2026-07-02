cmake_minimum_required(VERSION 3.15)
project(AudioFlux)

set(CMAKE_CXX_STANDARD 17)

# GUI 版本（WIN32 = 使用 wWinMain 入口）
add_executable(AudioFlux WIN32
    gui_main.cpp
    audio_devices.cpp
    audio_engine.cpp
    system_volume.cpp
    wasapi_capture.cpp
    wasapi_output.cpp
)

target_include_directories(AudioFlux PRIVATE ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(AudioFlux
    ole32
    oleaut32
    uuid
    ksuser
    user32
    gdi32
    dwmapi
)

target_compile_definitions(AudioFlux PRIVATE _WIN32_WINNT=0x0A00 UNICODE _UNICODE)

# MinGW 需要 -municode 支持 wWinMain 入口
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(AudioFlux PRIVATE -municode)
    target_link_options(AudioFlux PRIVATE -municode)
endif()
