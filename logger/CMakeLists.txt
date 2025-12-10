add_library(snlogger SHARED)

target_include_directories(snlogger PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(snlogger PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(snlogger PRIVATE configs)

target_compile_definitions(snlogger PRIVATE SN_EXPORT)

add_subdirectory(src)
