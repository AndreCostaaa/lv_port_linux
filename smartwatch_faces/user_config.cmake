# =============================================================================
# user_config.cmake - Add your custom source files here
# =============================================================================
#
# This file is included by the generated CMakeLists.txt and allows you to
# add extra source files to the project without modifying generated files
# (which may be overwritten).
#
# Tip:
#   - Use ${CMAKE_CURRENT_LIST_DIR} to get paths relative to this file
#
# =============================================================================

# Hand-written C of the smartwatch demo (navigation, callbacks, clock)
list(APPEND LV_EDITOR_PROJECT_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/src/lv_demo_watch.c
    ${CMAKE_CURRENT_LIST_DIR}/src/watch_bench.c
    ${CMAKE_CURRENT_LIST_DIR}/src/watch_callbacks.c
    ${CMAKE_CURRENT_LIST_DIR}/src/watch_depth.c
    ${CMAKE_CURRENT_LIST_DIR}/src/watch_nav.c
    ${CMAKE_CURRENT_LIST_DIR}/src/watch_thumbs.c
    ${CMAKE_CURRENT_LIST_DIR}/src/watch_time.c
)
