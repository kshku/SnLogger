set(SRCS
    test.c
)

add_executable(test)

target_sources(test PRIVATE ${SRCS})

target_link_libraries(test PRIVATE snlogger)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_custom_target(copy_dlls ALL
        COMMENT "Copy the dlls"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_RUNTIME_DLLS:test>" "$<TARGET_FILE_DIR:test>"
        COMMAND_EXPAND_LISTS
    )

    add_dependencies(copy_dlls test)
endif()

add_executable(test1 test1.c)
target_link_libraries(test1 PRIVATE snlogger)

add_executable(test2 test2.c)
target_link_libraries(test2 PRIVATE snlogger)

add_executable(formatting formatting.c)
target_link_libraries(formatting PRIVATE snlogger)

add_executable(example_formatting example_formatting.c)
target_link_libraries(example_formatting PRIVATE snlogger)
