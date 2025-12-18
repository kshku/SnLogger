if(SN_LOGGER_BUILD_STATIC)
    add_library(snlogger STATIC)
    target_compile_definitions(snlogger PRIVATE SN_LOGGER_STATIC)
else()
    add_library(snlogger SHARED)
    target_compile_definitions(snlogger PRIVATE SN_EXPORT)
endif()

target_include_directories(snlogger PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(snlogger PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(snlogger PRIVATE configs)

add_subdirectory(src)
