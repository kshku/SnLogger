set(HEADERFILES
    test.h
)

set(SRCS
    test.c
)

set(INCLUDE_BASE "${PROJECT_SOURCE_DIR}/logger/include/snlogger")

list(TRANSFORM HEADERFILES PREPEND "${INCLUDE_BASE}/")

target_sources(snlogger PRIVATE ${HEADERFILES} ${SRCS})
