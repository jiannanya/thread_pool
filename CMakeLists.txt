cmake_minimum_required(VERSION 3.20)
project(main)

set(CMAKE_CXX_STANDARD 17)
# clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(./src ./test)


add_executable(${PROJECT_NAME} main.cc)

file(GLOB sources CONFIGURE_DEPENDS src/*.cc src/*.h test/*.cc test/*.h)
#message("sources: "${sources})

target_sources(main PUBLIC ${sources})