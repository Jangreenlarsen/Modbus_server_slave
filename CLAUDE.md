# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Modbus RTU Server v3.2.0** is a production-ready embedded system for Arduino Mega 2560 that implements a complete Modbus RTU slave server with advanced features including TimerEngine, CounterEngine with frequency measurement, and an interactive CLI interface.

### Key Architecture Components

The codebase is organized into three main execution subsystems:

1. **Modbus RTU Server** (`src/modbus_*.cpp`) - Handles all Modbus protocol communication over RS-485:
   - Frame reception/parsing (`modbus_core.cpp`)
   - Function code processing (`modbus_fc.cpp`)
   - CRC16 validation and transmission (`modbus_tx.cpp`)
   - Supports FC01-04 (reads), FC05-06 (write single), FC0F-10 (write multiple)
   - Runs continuously in background via `processModbusTick()` from main loop

2. **TimerEngine** (`src/modbus_timers.cpp`, `include/modbus_timers.h`) - 4 independent timers with 4 modes:
   - Mode 1: One-shot sequences (3-phase timing)
   - Mode 2: Monostable (retriggerable pulse)
   - Mode 3: Astable (blink/toggle)
   - Mode 4: Input-triggered (responds to discrete inputs)
   - Global status/control registers at configurable indices
   - Runs via `timers_loop()` from main loop

3. **CounterEngine** (`src/modbus_counters.cpp`, `include/modbus_counters.h`) - 4 independent input counters with:
   - Edge detection (rising/falling/both)
   - Prescaler (1-256), direction (up/down), bit width (8/16/32/64)
   - Float scale factor for value transformation
   - Debounce filtering (configurable ms)
   - **v3.2.0 Features**: Frequency measurement (Hz), separate raw registers, consistent naming
   - Three output registers per counter: index-reg (scaled), raw-reg (unscaled), freq-reg (Hz)
   - Runs via `counters_loop()` from main loop

### Global Data Organization

- **Modbus Registers** (`modbus_globals.h`):
  - `holdingRegs[160]` and `inputRegs[160]` - Modbus register space (0-159)
  - `coils[32]` and `discreteInputs[32]` - Coil/input bit arrays (0-255, packed)
  - Static mappings: `regStaticAddr/Val`, `coilStaticIdx/Val` for fixed register/coil assignments
  - GPIO mappings: `gpioToCoil[54]`, `gpioToInput[54]` for pin-to-modbus binding

- **Configuration Storage** (`include/modbus_core.h`, `src/config_store.cpp`):
  - `PersistConfig` struct holds all runtime config (timers, counters, GPIO, static regs)
  - EEPROM schema versioning (current v9 with CRC checksum validation)
  - Functions: `configLoad()`, `configSave()`, `configDefaults()`, `configApply()`

- **CLI State** (`src/cli_shell.cpp`):
  - Command parser with 256-char buffer
  - Command history (3 commands) with arrow navigation
  - State machine: idle → CLI mode → command processing

### Main Loop Flow

```
setup():
  - Initialize serial (USB @ 115200, Modbus serial @ configurable baud)
  - Load EEPROM config
  - Initialize Modbus server (RS-485 direction control on pin 8)

loop():
  if (CLI active) {
    cli_loop()  // Process CLI commands
  } else {
    processModbusTick()  // Handle Modbus frames
    timers_loop()        // Update timer states
    counters_loop()      // Update counter values + frequency measurement
    heartbeat_led()      // Blink LED
  }
```

## Development Commands

### Build & Upload
```bash
# Build firmware
pio run

# Upload to Arduino Mega 2560
pio run -t upload

# Clean build
pio clean && pio run
```

### Testing & Monitoring
```bash
# Open serial monitor (CLI interface)
pio device monitor

# Monitor specific port/speed
pio device monitor -p COM3 -b 115200
```

### Configuration
- Edit `platformio.ini` to change upload speed or build flags
- EEPROM schema versioning in `include/version.h` and `modbus_core.h`
- Slave ID and default baudrate in `include/modbus_globals.h` (SLAVE_ID, BAUDRATE constants)

## Code Modification Guidelines

### Adding Counter Features
- Counter configuration is in `CounterConfig` struct (see `modbus_counters.h`)
- Update `configLoad()`/`configSave()` in `config_store.cpp` if adding new fields
- New counters output three register types:
  - `indexReg`: scaled value (counterValue × scale)
  - `rawReg`: unscaled raw counter value
  - `freqReg`: frequency measurement in Hz (0-20000)
- CLI parsing in `cli_shell.cpp` uses `set counter <id> mode 1 parameter ...` syntax
- Frequency measurement runs every second with validation (timing window 1-2sec, delta validation, clamping)

### Adding Timer Features
- Timer configuration in `TimerConfig` struct (see `modbus_timers.h`)
- Global status/control register indices configured via `set timers status-reg:<n> control-reg:<n>`
- CLI parsing in `cli_shell.cpp` for `set timer <id> mode <1..4> parameter ...`
- Each timer updates bits in global status register independently

### Modifying EEPROM Schema
- Current schema version: **v9** (see `modbus_core.h` and `version.h`)
- All configuration changes require schema bump
- `configLoad()` validates CRC16 checksum; if invalid, defaults are used
- Migration path: add new fields to `PersistConfig`, update version number, handle old versions in load function

### Modbus Function Code Implementation
- FC implementations in `modbus_fc.cpp` use utility functions:
  - `bitReadArray()`, `bitWriteArray()` for coil/input bit access
  - `holdingRegs[]`, `inputRegs[]` direct access for register operations
  - `sendException()` for error responses
  - `sendResponse()` for success responses with CRC

### CLI Command Structure
- Commands parsed in `cli_shell.cpp` with keyword matching
- Examples: `show config`, `set counter 1 ...`, `write reg 100 1000`
- Aliases supported (sh, wr, rd, sv, ld, etc.)
- Help text via `?` or `help` command
- New commands should follow existing patterns (case-insensitive, space-delimited parameters)

## Important Files Reference

| File | Purpose |
|------|---------|
| `src/main.cpp` | Entry point, setup/loop, heartbeat |
| `include/modbus_core.h` | Constants, function codes, buffer sizing, PersistConfig |
| `include/modbus_globals.h` | Global data declarations (registers, coils, GPIO mappings) |
| `src/modbus_globals.cpp` | Global data definitions |
| `src/modbus_fc.cpp` | Modbus function code handlers (FC01-10) |
| `src/modbus_core.cpp` | Frame reception, parsing, CRC validation |
| `src/modbus_tx.cpp` | Response transmission, RS-485 direction control |
| `src/modbus_timers.cpp` | TimerEngine implementation (4 independent timers) |
| `src/modbus_counters.cpp` | CounterEngine implementation (4 counters, freq measurement) |
| `src/config_store.cpp` | EEPROM load/save/defaults, config persistence |
| `src/cli_shell.cpp` | Interactive CLI command parsing and execution |
| `src/status_info.cpp` | Status display helpers (`show config`, `show counters`, etc.) |
| `include/version.h` | Version string, build date |

## Resource Constraints

- **RAM**: 56.8% of 8 KB used (4655 bytes) - comfortable headroom
- **Flash**: 20.1% of 256 KB used (51058 bytes) - plenty of room for features
- **EEPROM**: 4 KB - stores PersistConfig with CRC
- **Timers**: 4 maximum (hardware constraint)
- **Counters**: 4 maximum (hardware constraint)
- **Frequency measurement**: 0-20 kHz range (by design)

## Backward Compatibility Notes

- **v3.2.0** introduced consistent parameter naming (index-reg, raw-reg, freq-reg) but maintains backward compatibility with old names (reg, count-reg, raw, overload, control-reg, input)
- CLI parser accepts both new and old parameter names
- EEPROM schema migrations happen automatically on load
- Frequency measurement feature added without breaking existing counter functionality

## Testing Considerations

- CLI commands can be tested directly via serial monitor
- Modbus frames can be tested with external Modbus master tools over RS-485
- Counter frequency measurement validated with timing window checks (1-2 sec), delta validation, overflow detection
- Timer modes tested by observing coil state changes via CLI (`show counters`)
- EEPROM persistence tested by issuing `save`, then `reboot`, then `show config`

## Performance Notes

- Modbus processing runs in background loop, no blocking
- Counter frequency measurement updates once per second
- Debounce filtering prevents noise on input edges
- Timer updates happen in `timers_loop()` every loop iteration
- CLI command parsing runs only when CLI active (doesn't block Modbus processing)

