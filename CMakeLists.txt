cmake_minimum_required(VERSION 3.16)
set(CMAKE_POLICY_DEFAULT_CMP0048 NEW)
project(moco VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

include(FetchContent)

# Force cJSON to be built as static for direct linkage into moco.
set(CJSON_OVERRIDE_BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
set(CJSON_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ENABLE_CJSON_TEST OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
        tomlc17
        GIT_REPOSITORY https://ghfast.top/https://github.com/cktan/tomlc17.git
        GIT_TAG main
)
FetchContent_Declare(
        cjson
        GIT_REPOSITORY https://ghfast.top/https://github.com/DaveGamble/cJSON.git
        GIT_TAG master
)

FetchContent_MakeAvailable(tomlc17 cjson)

# tomlc17 upstream has no CMake target; create one from fetched source.
if (NOT TARGET tomlc17)
    add_library(tomlc17 STATIC
            "${tomlc17_SOURCE_DIR}/src/tomlc17.c"
    )
    target_include_directories(tomlc17 PUBLIC
            "${tomlc17_SOURCE_DIR}/src"
    )
endif ()

string(TIMESTAMP CURRENT_DATE "%Y-%m-%d")
configure_file(
        "${PROJECT_SOURCE_DIR}/config.h.in"
        "${PROJECT_BINARY_DIR}/config.h"
)

if (NOT CMAKE_C_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR "This project requires GCC (GNU C compiler).")
endif ()

add_compile_definitions(_GNU_SOURCE)


add_executable(moco
        src/main.c
        src/interface.c
        src/command/init.c
        src/command/login/oauth2.c
        src/command/login/curl.c
        src/command/login/login.c
        src/command/install/install.c
        src/command/update/update.c
        src/command/search/search.c
        src/utility/file/json.c
        src/utility/store/store.c
        src/utility/download/download.c
        src/env.c
        src/utility/file/toml.c
        src/m_exit.c
        src/utility/mtool.c
        src/utility/download/m_epoll.c
        src/utility/download/m_epoll.h
        src/command/start/start.c
        src/command/start/start.h
)

target_link_libraries(moco PRIVATE
        CURL::libcurl
        cjson
        tomlc17
        crypto
        Threads::Threads
)

target_include_directories(moco PRIVATE
        "${PROJECT_SOURCE_DIR}/src"     # 源码目录
        "${PROJECT_BINARY_DIR}"          # 包含生成的 config.h 的目录
)