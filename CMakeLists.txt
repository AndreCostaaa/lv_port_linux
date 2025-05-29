cmake_minimum_required(VERSION 3.10)
project(lvgl)

set(CONF_PATH "${PROJECT_SOURCE_DIR}/lv_conf.h")

# Uncomment if the program needs debugging
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -ggdb")

set(CMAKE_C_STANDARD 99) # LVGL officially supports C99 and above
set(CMAKE_CXX_STANDARD 17) #C17
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)

set(CONFIG_LV_BUILD_DEMOS "Build demos" ON CACHE BOOL "Build demos" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES "Build examples" OFF CACHE BOOL "Build examples" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL "Use the internal version of ThorVG" OFF CACHE BOOL "Build ThorVG" FORCE)

add_subdirectory(lvgl)
add_executable(lvglsim src/main.c)
target_link_libraries(lvglsim lvgl lvgl::demos m pthread ${PKG_CONFIG_LIB})
