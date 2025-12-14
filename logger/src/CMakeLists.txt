set(HEADERFILES
    platform.h
    defines.h
    snlogger.h
)

set(SRCS
    snlogger.c
)

set(INCLUDE_BASE "${PROJECT_SOURCE_DIR}/logger/include/snlogger")

list(TRANSFORM HEADERFILES PREPEND "${INCLUDE_BASE}/")

target_sources(snlogger PRIVATE ${HEADERFILES} ${SRCS})
