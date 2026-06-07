cmake_minimum_required(VERSION 3.15)
project(audio)

set(CMAKE_CXX_STANDARD 17)

# 纯 WASAPI 实现
add_executable(audio 
    main.cpp
    wasapi_capture.cpp
    wasapi_output.cpp
)

# 链接 Windows 系统库
target_link_libraries(audio 
    ole32      # COM 组件
    uuid       # GUID
    ksuser     # Kernel Streaming GUID
)

# Windows 10+ 版本定义（支持 AUTOCONVERTPCM）
target_compile_definitions(audio PRIVATE _WIN32_WINNT=0x0A00)