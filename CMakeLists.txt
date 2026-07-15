cmake_minimum_required(VERSION 3.15)
project(RouterMonitor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Windows-only application
if(NOT WIN32)
    message(FATAL_ERROR "RouterMonitor currently only supports Windows.")
endif()

# Static runtime -> single exe with no CRT redist dependency
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:Release>")

# Use UNICODE and strict warnings on MSVC
if(MSVC)
    add_compile_options(
        /W4 /permissive- /utf-8 /EHsc /D_UNICODE /DUNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX
    )
    # Aggressive size optimization for release
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(/Os /GL)
        add_link_options(/LTCG /OPT:REF /OPT:ICF)
    endif()
else()
    # MinGW fallback
    add_compile_options(-Wall -Wextra -mwindows -municode)
    # Static-link the GCC/C++/winpthread runtimes so the exe has zero non-system DLL deps.
    # winpthread ships both libwinpthread.a (static) and libwinpthread.dll.a (import);
    # -lwinpthread defaults to the .dll.a, so we point explicitly at the static archive.
    add_link_options(-mwindows -municode -static-libgcc -static-libstdc++ -l:libwinpthread.a)
    add_compile_definitions(UNICODE _UNICODE)
endif()

# Source files
set(ROUTERMONITOR_SOURCES
    src/HttpClient.cpp
    src/JsonParser.cpp
    src/Config.cpp
    src/Credentials.cpp
    src/Sha256.cpp
    src/RouterApi.cpp
    src/PollerThread.cpp
    src/ui/MainWindow.cpp
    src/ui/HudWindow.cpp
    src/ui/TrayIcon.cpp
    src/main.cpp
)

add_executable(RouterMonitor WIN32 ${ROUTERMONITOR_SOURCES})

target_include_directories(RouterMonitor PRIVATE include)

# Link Windows system libraries
target_link_libraries(RouterMonitor PRIVATE
    winhttp    # HTTP client
    bcrypt    # SHA256
    gdiplus    # GDI+ for nicer drawing
    comctl32   # Common controls
    shell32    # Shell_NotifyIcon, etc.
    user32     # Window management
    gdi32      # GDI
    advapi32   # Registry (for future use)
    msimg32    # AlphaBlend
)

# Output to a predictable location
set_target_properties(RouterMonitor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Post-build: copy config template next to exe (optional)
# add_custom_command(TARGET RouterMonitor POST_BUILD ...)