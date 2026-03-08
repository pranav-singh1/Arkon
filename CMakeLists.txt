cmake_minimum_required(VERSION 3.20)
project(arkon VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# macOS (Homebrew): tell CMake where to find OpenSSL
# Homebrew installs it here on both Intel and Apple Silicon
if(APPLE)
    # Apple Silicon
    include_directories(/opt/homebrew/include)
    if(EXISTS "/opt/homebrew/opt/openssl")
        set(OPENSSL_ROOT_DIR "/opt/homebrew/opt/openssl")
    # Intel Mac
    elseif(EXISTS "/usr/local/opt/openssl")
        set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl")
    endif()
endif()

find_package(Boost 1.74 CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)

# --- kalshi_feed executable ---
add_executable(kalshi_feed
    src/kalshi_feed.cpp
)

target_include_directories(kalshi_feed PRIVATE
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(kalshi_feed PRIVATE
    Boost::headers
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)

# Enable warnings
target_compile_options(kalshi_feed PRIVATE -Wall -Wextra -O2)