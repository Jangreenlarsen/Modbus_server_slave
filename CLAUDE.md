# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Language Requirement

**ALTID TALE PÅ DANSK** - Uanset om brugeren skriver på dansk, engelsk eller andet sprog, skal Claude ALTID svare og kommunikere på dansk. Dette er et dansk projekt med dansk udvikler.

## Project Overview

**Modbus RTU Server v3.6.1** is a production-ready embedded system for Arduino Mega 2560 that implements a complete Modbus RTU slave server with advanced features including TimerEngine, CounterEngine with unified prescaler strategy, frequency measurement, and an interactive CLI interface.

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

3. **CounterEngine** (`src/modbus_counters.cpp`, `src/modbus_counters_hw.cpp`, `src/modbus_counters_sw_int.cpp`) - 4 independent counters with:
   - Edge detection (rising/falling/both)
   - Unified prescaler (1, 4, 8, 16, 64, 256, 1024), direction (up/down), bit width (8/16/32/64)
   - Float scale factor for value transformation
   - Debounce filtering (configurable ms)
   - **Three Operating Modes (v3.6.1 unified prescaler strategy)**:
     - **SW (Polling)**: `hw-mode:sw` - Software polling via discrete inputs
     - **SW-ISR (Interrupt)**: `hw-mode:sw-isr` - Hardware interrupt-driven (INT0-INT5: pins 2,3,18,19,20,21)
     - **HW (Timer5)**: `hw-mode:hw-t5` - Hardware Timer5 external clock (pin 2, max ~20 kHz)
   - **CRITICAL Hardware Limitation (v3.4.7)**:
     - ATmega2560 Timer5 external clock mode CANNOT use hardware prescaler
     - Internal prescaler modes count system clock (16MHz), NOT external pulses
     - Solution: Hardware ALWAYS uses external clock mode, prescaler implemented in software
   - **Unified Prescaler Strategy (v3.6.0-v3.6.1)**:
     - ALL modes count EVERY edge/pulse in counterValue
     - Prescaler division happens ONLY at output (raw register)
     - Consistent behavior: HW, SW, and SW-ISR all use same prescaler approach
   - **Three output registers per counter**:
     - `index-reg`: Scaled value (counterValue × scale) - full precision
     - `raw-reg`: Prescaled value (counterValue / prescaler) - reduced size for register space
     - `freq-reg`: Frequency measurement in Hz (0-20000)
   - Frequency measurement (Hz) updates once per second with validation
   - Runs via `counters_loop()` from main loop

### Global Data Organization

- **Modbus Registers** (`modbus_globals.h`):
  - `holdingRegs[160]` and `inputRegs[160]` - Modbus register space (0-159)
  - `coils[32]` and `discreteInputs[32]` - Coil/input bit arrays (0-255, packed)
  - Static mappings: `regStaticAddr/Val`, `coilStaticIdx/Val` for fixed register/coil assignments
  - GPIO mappings: `gpioToCoil[54]`, `gpioToInput[54]` for pin-to-modbus binding

- **Configuration Storage** (`include/modbus_core.h`, `src/config_store.cpp`):
  - `PersistConfig` struct holds all runtime config (timers, counters, GPIO, static regs)
  - EEPROM schema versioning (current v10 with CRC checksum validation)
  - Functions: `configLoad()`, `configSave()`, `configDefaults()`, `configApply()`
  - Note: v3.3.0 moved PersistConfig from stack to global RAM (~1KB) to prevent stack overflow

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

**Counter Architecture (v3.6.1 - Unified Prescaler Strategy):**

- Counter configuration is in `CounterConfig` struct (see `modbus_counters.h`)
- Update `configLoad()`/`configSave()` in `config_store.cpp` if adding new fields

**Three Output Registers (IMPORTANT):**
  - `index-reg` (value): Scaled value = counterValue × scale (FULL precision, NO prescaler division)
  - `raw-reg`: Prescaled value = counterValue / prescaler (reduced size for register space)
  - `freq-reg`: Frequency in Hz (0-20000), measured once per second

**Unified Prescaler Strategy (v3.6.0-v3.6.1):**
  - **ALL modes count EVERY edge/pulse** - no skipping!
  - HW mode: Hardware counts all pulses → raw = counterValue / prescaler
  - SW mode: Software counts all edges → raw = counterValue / prescaler
  - SW-ISR mode: ISR counts all edges → raw = counterValue / prescaler
  - **Prescaler division happens ONLY in `store_value_to_regs()`** (one place)
  - Supported values: **1, 4, 8, 16, 64, 256, 1024**
  - **Example:** prescaler=64, 76800 edges → counterValue=76800, value=76800, raw=1200

**CRITICAL: ATmega2560 Timer5 Hardware Limitation (v3.4.7):**
  - External clock mode (TCCR5B=0x07): Counts pulses on T5 pin, NO hardware prescaler
  - Internal prescaler modes (TCCR5B=0x02-0x05): Count 16MHz system clock, IGNORE T5 pin!
  - **Solution:** Hardware ALWAYS uses external clock mode (counts all pulses)
  - Prescaler implemented 100% in software (division at output only)

**Counter Setup Workflow:**
  1. **SW (Polling) Mode**: `set counter <id> mode 1 parameter hw-mode:sw input-dis:<N> prescaler:<P> ...`
     - Polls discrete input via `di_read()`
     - Counts every edge (no skipping)
     - User can map GPIO: `gpio map <pin> input <N>`

  2. **SW-ISR (Interrupt) Mode**: `set counter <id> mode 1 parameter hw-mode:sw-isr interrupt-pin:<PIN> prescaler:<P> ...`
     - Hardware interrupts on pins 2,3,18,19,20,21 (INT0-INT5)
     - ISR counts every edge immediately (deterministic, no polling delay)
     - Must use valid interrupt pins
     - Example: `set counter 1 mode 1 parameter hw-mode:sw-isr interrupt-pin:21 prescaler:64 index-reg:40 raw-reg:44`

  3. **HW (Timer5) Mode**: `set counter <id> mode 1 parameter hw-mode:hw-t5 prescaler:<P> ...`
     - Uses Timer5 external clock on pin 2 (PE4/T5)
     - Hardware counts all pulses (external clock mode only)
     - Auto-maps GPIO pin 2 to discrete input
     - Max frequency ~20 kHz
     - **See `docs/ATmega2560-timers-counters-configs.md` for full hardware limitations**

**ScaleFloat:**
  - Applied to value register: `value = counterValue × scale`
  - Does NOT affect raw register (raw = counterValue / prescaler only)
  - Example: scale=0.5, prescaler=64, 76800 edges → value=38400, raw=1200

**Frequency Measurement:**
  - Updates once per second (1-2 sec timing window)
  - Direct Hz measurement (no prescaler compensation needed)
  - Validation: delta check, overflow detection, clamping 0-20kHz
  - Works identically in all three modes

### Adding Timer Features
- Timer configuration in `TimerConfig` struct (see `modbus_timers.h`)
- Global status/control register indices configured via `set timers status-reg:<n> control-reg:<n>`
- CLI parsing in `cli_shell.cpp` for `set timer <id> mode <1..4> parameter ...`
- Each timer updates bits in global status register independently

### Modifying EEPROM Schema
- Current schema version: **v10** (see `modbus_core.h` and `version.h`)
- All configuration changes require schema bump
- `configLoad()` validates CRC16 checksum; if invalid, defaults are used
- Migration path: add new fields to `PersistConfig`, update version number, handle old versions in load function
- Note: Runtime fields (counterValue, edgeCount, etc.) are cleared before save (v3.3.5)

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

## ATmega2560 Hardware Reference

**IMPORTANT: See `docs/ATmega2560-timers-counters-configs.md` for comprehensive documentation on:**
- All available hardware timers (Timer0-Timer5) and external clock pins
- Arduino Mega 2560 pin mapping (only T0 and T5 are accessible)
- Timer5 hardware limitations and prescaler architecture
- SW-ISR interrupt pin availability (INT0-INT5)
- Practical considerations and conflicts
- Future upgrade paths for multiple counters

**Quick Facts:**
- Timer5 external clock input: **Pin 2 (PE4/T5)** - RECOMMENDED for hw-mode:hw-t5
- Timer0 external clock input: **Pin 38 (PD7/T0)** - AVOID (kernel dependencies)
- SW-ISR interrupt pins: **Pins 2,3,18,19,20,21 (INT0-INT5)** - Pin 2 har både T5 og INT2, Pin 20 har PE6/T3 konflikt
- Max Timer5 frequency: ~20 kHz (clamped for stability)

## Important Files Reference

| File | Purpose |
|------|---------|
| `docs/ATmega2560-timers-counters-configs.md` | **Comprehensive hardware timer/counter reference** |
| `src/main.cpp` | Entry point, setup/loop, heartbeat |
| `include/modbus_core.h` | Constants, function codes, buffer sizing, PersistConfig |
| `include/modbus_globals.h` | Global data declarations (registers, coils, GPIO mappings) |
| `src/modbus_globals.cpp` | Global data definitions |
| `src/modbus_fc.cpp` | Modbus function code handlers (FC01-10) |
| `src/modbus_core.cpp` | Frame reception, parsing, CRC validation |
| `src/modbus_tx.cpp` | Response transmission, RS-485 direction control |
| `src/modbus_timers.cpp` | TimerEngine implementation (4 independent timers) |
| `src/modbus_counters.cpp` | CounterEngine main (SW/HW mode, prescaler logic, freq measurement) |
| `src/modbus_counters_hw.cpp` | HW counter implementation (Timer5 ISRs, external clock mode) |
| `src/modbus_counters_sw_int.cpp` | SW-ISR counter (INT0-INT5 interrupt handlers) |
| `include/modbus_counters.h` | CounterConfig struct, counter constants |
| `include/modbus_counters_hw.h` | HW counter interface |
| `include/modbus_counters_sw_int.h` | SW-ISR counter interface |
| `src/config_store.cpp` | EEPROM load/save/defaults, config persistence |
| `src/cli_shell.cpp` | Interactive CLI command parsing and execution |
| `src/status_info.cpp` | Status display helpers (`show config`, `show counters`, etc.) |
| `include/version.h` | Version string, build date, complete changelog |

## Resource Constraints

- **RAM**: 83.5% of 8 KB used (6841 bytes) - acceptable (v3.3.0 moved PersistConfig to global RAM)
  - Note: RAM increased from 56.8% (v3.2.0) to 82.4% (v3.3.0) due to stack overflow fix
  - Still 1.3 KB free for runtime operations
- **Flash**: 27.2% of 256 KB used (68994 bytes) - plenty of room for features
- **EEPROM**: 4 KB - stores PersistConfig with CRC (schema v10)
- **Timers**: 6 available on ATmega2560 chip (Timer0-Timer5)
  - Timer0, Timer1, Timer2: Standard timers
  - Timer3, Timer4, Timer5: Extended 16-bit timers (Arduino Mega only)
  - **External clock input pins**: Only T0 (pin 38) and T5 (pin 2) practically accessible on Arduino Mega
  - Timer5 recommended for hw-mode:hw-t5 (no kernel conflicts)
- **Counters**: 4 maximum per system (hardware/design constraint)
  - 3 operating modes per counter: SW (polling), SW-ISR (interrupt), HW (Timer5)
  - Each mode can use different hardware: GPIO polling, INT0-INT5 interrupts, or Timer5 external clock
- **Frequency measurement**: 0-20 kHz range (validated and clamped by design)
- **Interrupt pins**: 6 available for SW-ISR mode (INT0-INT5: pins 2,3,18,19,20,21)
  - ⚠️ Pin 20 (INT4/PE6): Potential conflict with Timer3 T3 pin (not used in current design)

## Backward Compatibility Notes

- **v3.6.0-v3.6.1 BREAKING CHANGE:** Counter prescaler behavior changed fundamentally
  - **OLD (v3.5.9 and earlier):** SW/SW-ISR modes used edgeCount to skip edges (counterValue prescaled)
  - **NEW (v3.6.0+):** ALL modes count EVERY edge/pulse (counterValue NOT prescaled)
  - **Impact:** SW/SW-ISR counters will count 64× more for prescaler=64
  - **Migration:** Existing SW/SW-ISR counters will reset to 0 after upgrade
  - **Benefit:** Consistent behavior across HW, SW, and SW-ISR modes

- **v3.4.7 BREAKING CHANGE:** HW mode Timer5 prescaler now software-only
  - **OLD:** Hardware prescaler modes tried to use internal prescaler (WRONG - counted system clock)
  - **NEW:** Hardware ALWAYS uses external clock mode, prescaler division in software only
  - **Impact:** HW counters behavior more predictable and correct

- **v3.2.0** introduced consistent parameter naming (index-reg, raw-reg, freq-reg)
  - Maintains backward compatibility with old names
  - CLI parser accepts both new and old parameter names

- **EEPROM schema** migrations happen automatically on load
  - Schema v10 (current) handles all previous versions
  - Runtime fields cleared before save to prevent state persistence bugs

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

