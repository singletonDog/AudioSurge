cmake_minimum_required(VERSION 3.15)
project(AudioFlux)

set(CMAKE_CXX_STANDARD 17)

# 默认使用 Release 构建，便于体积/性能优化
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# GUI 版本（WIN32 = 使用 wWinMain 入口）
add_executable(AudioFlux WIN32
    src/gui_main.cpp
    src/audio/audio_devices.cpp
    src/audio/audio_engine.cpp
    src/system/system_volume.cpp
    src/audio/wasapi_capture.cpp
    src/audio/wasapi_output.cpp
    src/common/app_state.cpp
    src/app.rc
)

# 让资源在图标文件变化时自动重编（.rc 默认不跟踪 ICON 引用的 .ico）
set_source_files_properties(src/app.rc PROPERTIES
    OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/assets/audioflux.ico
)

target_include_directories(AudioFlux PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/audio
    ${CMAKE_SOURCE_DIR}/src/system
    ${CMAKE_SOURCE_DIR}/src/common
)

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

    # 体积优化：按段拆分 + 链接期回收未用段 + strip 符号
    target_compile_options(AudioFlux PRIVATE
        $<$<CONFIG:Release>:-Os -ffunction-sections -fdata-sections>
    )
    target_link_options(AudioFlux PRIVATE
        $<$<CONFIG:Release>:-Wl,--gc-sections -s>
    )
endif()
