set(HEADERFILES
    platform.h
    defines.h
    log_level.h
    snlogger.h
    formatter.h
    sink.h
    static_logger.h
)

set(SRCS
    snlogger.c
    formatter.c
    static_logger.c
)

set(INCLUDE_BASE "${PROJECT_SOURCE_DIR}/logger/include/snlogger")

list(TRANSFORM HEADERFILES PREPEND "${INCLUDE_BASE}/")

target_sources(snlogger PRIVATE ${HEADERFILES} ${SRCS})
