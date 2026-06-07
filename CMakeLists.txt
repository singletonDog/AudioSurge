cmake_minimum_required(VERSION 3.15)
project(AudioMirror)

set(CMAKE_CXX_STANDARD 17)

add_executable(AudioMirror
    main.cpp
    wasapi_capture.cpp
    wasapi_output.cpp
)

target_link_libraries(AudioMirror
    ole32
    uuid
    ksuser
)

target_compile_definitions(AudioMirror PRIVATE _WIN32_WINNT=0x0A00)