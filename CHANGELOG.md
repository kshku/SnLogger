# Changelog

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
