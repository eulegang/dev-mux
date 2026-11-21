cmake_minimum_required(VERSION 3.16)
project(dev-mux VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUV REQUIRED IMPORTED_TARGET libuv)
pkg_check_modules(LIBGIT2 REQUIRED IMPORTED_TARGET libgit2)
pkg_check_modules(LUA REQUIRED IMPORTED_TARGET lua)

add_executable(
	dev-mux

	src/main.c 
	src/cli.c

	src/protocol/protocol.c

	src/server/server.c
	src/server/runtime.c
	src/server/handle.c
	src/server/pty.c

	src/client/client.c
	src/client/config.c
	src/client/keycode.c
	src/client/terminal.c
	src/client/runtime.c
)

target_link_libraries(dev-mux PRIVATE PkgConfig::LIBUV PkgConfig::LIBGIT2 PkgConfig::LUA)

if(APPLE)
    target_compile_definitions(dev-mux PRIVATE _DARWIN_C_SOURCE)
else()
    target_compile_definitions(dev-mux PRIVATE _DEFAULT_SOURCE)
endif()
target_compile_options(dev-mux PRIVATE -Wall -Wextra -Wpedantic)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(dev-mux PRIVATE util)
endif()

include(CTest)
if(BUILD_TESTING)
    enable_language(CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    find_package(GTest REQUIRED)

    add_executable(protocol_test tests/protocol_test.cpp src/protocol/protocol.c)
    target_include_directories(protocol_test PRIVATE src)
    target_link_libraries(protocol_test PRIVATE GTest::gtest_main PkgConfig::LIBUV)
    add_test(NAME protocol_test COMMAND protocol_test)

    add_executable(config_test tests/config_test.cpp src/client/config.c src/client/keycode.c)
    target_link_libraries(config_test PRIVATE GTest::gtest_main PkgConfig::LUA PkgConfig::LIBUV)
    add_test(NAME config_test COMMAND config_test)

    add_executable(keycode_test tests/keycode_test.cpp src/client/keycode.c)
    target_include_directories(keycode_test PRIVATE src)
    target_link_libraries(keycode_test PRIVATE GTest::gtest_main)
    add_test(NAME keycode_test COMMAND keycode_test)
endif()
