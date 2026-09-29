# Changelog

## [0.3.1] - 2026-09-28

### Added
- sn_console_write_va and sn_console_write_rgb_va, which take a va_list rather
  than being variadic. A caller that has already collected its arguments cannot
  forward them through a plain ... , and previously had to format into its own
  buffer first, which brings back the truncation handling the console sink
  exists to avoid

### Changed
- The palette, the rgb and the va_list forms now share one emit path, so they
  cannot drift apart in how they build or omit the escape codes

## [0.3.0] - 2026-09-28

### Added
- An optional console sink, in console_sink.h. It is a convenience the library
  never uses itself, in the same way sn_std_allocator is one that SnMemory
  never uses, so there is no option to turn it off and nothing depends on it
  being there
- sn_console_write and sn_console_write_rgb, for writing one colored record
  without involving a logger. Color comes from the 8 palette colors or from 24
  bit RGB
- sn_console_std_sink, a ready made sink on stdout that is usable as the program
  starts, with no init call, the way sn_std_allocator needs none
- sn_console_sink_init, which is only needed to send output somewhere other than
  stdout
- sn_console_sink_set_color, taking a new SnConsoleColorMode. AUTO follows the
  stream, so a redirected or piped build gets plain text, and OFF and ON force
  the choice
- sn_console_sink_set_level_color, to color records by log level
- sn_console_flush
- The sink fills in the whole SnSink lifecycle, open, write, flush and close.
  close does not close the stream, because the sink borrows it from the caller
  rather than opening it, and the logger flushes just before calling close
  anyway
- Escape codes are never written and then stripped. When color is off the plain
  text is emitted, so a redirected log carries no escape noise at all
- The color state is resolved on each write instead of being cached at init, so a
  sink stays correct if the stream is reconfigured or a terminal comes and goes

## [0.2.1] - 2026-09-28

### Fixed
- Fix SN_LOGGER_API in api.h, which branched on SN_LOGGER_EXPORT, a macro
  nothing defines. A shared build exported no symbols at all and could not be
  linked against

### Added
- Build the tests against a shared library in CI, which is what catches a
  symbol that is missing an export macro

## [0.2.0] - 2026-06-12

## Changed
- sn_async_logger_set_memory_hook changed to sn_async_logger_set_memory_allocator, taking SnMemoryAllocator

## [0.1.0] - 2026-06-11

- First release. See [0.0.0] section in CHANGELOG.md for full changelog.

## [0.0.0] - 2025-12-10

### Added
- Static logger with dedicated ring buffer
- Async logger with background thread dispatch
- Transport abstraction (sink interface for custom output)
- Multiple logging levels (debug, info, warn, error, fatal)
- Timestamp and context metadata in log entries
- No-implicit-I/O and no-global-state design
- Async drain and flush operations
- Thread-safe ring buffer integration (via SnMemory)
- SnCore + SnMemory dependencies
- Test suite covering sync and async logging
- CI workflows (Linux, macOS, Windows, formatting)
