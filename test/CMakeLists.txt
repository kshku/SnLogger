enable_testing()

set(SRCS
    test.c
)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # Set stack size to 10 MB
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /STACK:10485760")
endif()

add_executable(sn_logger_test)
target_sources(sn_logger_test PRIVATE ${SRCS})
target_link_libraries(sn_logger_test PRIVATE snlogger)

add_test(NAME sn_logger_test COMMAND sn_logger_test)

add_executable(test1 test1.c)
target_link_libraries(test1 PRIVATE snlogger)

add_test(NAME test1 COMMAND test1)

add_executable(test2 test2.c)
target_link_libraries(test2 PRIVATE snlogger)

add_test(NAME test2 COMMAND test2)

add_executable(formatting formatting.c)
target_link_libraries(formatting PRIVATE snlogger)

add_test(NAME formatting COMMAND formatting)

add_executable(example_formatting example_formatting.c)
target_link_libraries(example_formatting PRIVATE snlogger)

add_test(NAME example_formatting COMMAND example_formatting)


if(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND SN_LOGGER_BUILD_SHARED)
    add_custom_target(copy_dlls ALL
        COMMENT "Copy the dlls"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_RUNTIME_DLLS:sn_logger_test>" "$<TARGET_FILE_DIR:sn_logger_test>"
        COMMAND_EXPAND_LISTS
    )

    add_dependencies(copy_dlls sn_logger_test test1 test2 formatting example_formatting)
endif()