# Changelog

## [0.1.0] — 2025-12-10

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
