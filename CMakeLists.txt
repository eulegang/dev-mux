cmake_minimum_required(VERSION 3.16)
project(twind VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(
	twind 

	src/main.c 
	src/terminal.c
	src/io.c
	src/protocol.c
	src/server.c
	src/client.c
)


if(APPLE)
    target_compile_definitions(twind PRIVATE _DARWIN_C_SOURCE)
else()
    target_compile_definitions(twind PRIVATE _DEFAULT_SOURCE)
endif()
target_compile_options(twind PRIVATE -Wall -Wextra -Wpedantic)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(twind PRIVATE util)
endif()
