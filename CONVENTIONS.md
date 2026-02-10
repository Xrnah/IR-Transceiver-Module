# Code Conventions (Review & Dev Guide)
The codebase follows the common conventions referenced by the project and the normalization pass applied here.

## Naming Rules
- Functions: `lowerCamelCase`
- Variables: `snake_case`
- Types (classes/structs/enums): `PascalCase`
- Acronyms: ALL CAPS inside identifiers (e.g., `MQTT`, `ACU`, `NTP`)
- Globals: `g_` prefix + above rules (e.g., `g_mqtt_client`)
- File-local constants: `k_` prefix in `snake_case` (e.g., `k_log_tag`)
- Booleans: `is_`, `has_`, `can_` prefix in `snake_case` (e.g., `g_is_mqtt_publish_in_progress`)
- User-config macros: ALL-CAPS `SNAKE_CASE` (e.g., `LOG_LEVEL`, `USE_ACU_ADAPTER`)

## File Layout & Visibility
- Use `#pragma once` in headers.
- Keep file-local helpers in an anonymous namespace.
- Prefer `constexpr` for compile-time constants.
- Globals are declared `extern` in headers and defined in one `.cpp`.
- Guard ESP8266-only modules with `#if !defined(ARDUINO_ARCH_ESP8266)` and `#error "ESP8266 only"`.

## API Shape
- Public module headers are interface-only (declarations and docstrings only).
- C-style public APIs are retained for embedded friendliness.

## Memory & Timing
- Prefer fixed-size buffers and `StaticJsonDocument` to avoid heap churn.
- Pre-allocate serialization buffers for MQTT payloads.
- Use `snprintf`/`strncpy` with explicit null termination.
- Yield in long or tight loops (`yield()`), especially around Wi-Fi, MQTT, or scans.

## Error Handling (MQTT)
- All MQTT errors are logged locally using the logging module.
- High-context error details can be published to a dedicated MQTT topic when enabled (see Logging section).

## Logging
Logging is centralized in `include/logging.h` / `src/logging.cpp`.

### Build Flags
Set these in `platformio.ini` under `build_flags`:
- `LOG_LEVEL` (0=Error, 1=Warn, 2=Info, 3=Debug)
- `LOG_SERIAL_ENABLE` (0/1). If not defined, it defaults to `1` when `LOG_LEVEL > 1`, otherwise `0`.
- `LOG_MQTT_ERROR_CONTEXT_MIN_LOG_LEVEL` (0-3, 255=off). MQTT `/error` publishing is enabled when `LOG_LEVEL >= LOG_MQTT_ERROR_CONTEXT_MIN_LOG_LEVEL`.

### Behavior
- Serial logging only initializes when `LOG_SERIAL_ENABLE` is enabled.
- All log calls are gated by `LOG_LEVEL`.
- Debug-only routines are gated by `LOG_LEVEL >= 3`.

### Debug Flags
- `ENABLE_IR_DEBUG_INPUT`: enables raw 64-bit IR input over Serial (only active when `LOG_SERIAL_ENABLE=1`).

## References
- Common Coding Conventions (TUM ESI): https://github.com/tum-esi/common-coding-conventions
