# Modbus RTU Server v3.6.5

🚀 **Production-Ready** Arduino Mega 2560 Modbus RTU Server med Unified Prescaler Architecture, 3 Counter Modes (HW/SW/SW-ISR), CLI, og Timer Engine.

[![Version](https://img.shields.io/badge/version-3.6.5-blue.svg)](https://github.com/Jangreenlarsen/Modbus_server_slave/releases/tag/v3.6.5)
[![Platform](https://img.shields.io/badge/platform-Arduino%20Mega%202560-green.svg)](https://www.arduino.cc/en/Main/ArduinoBoardMega2560)
[![Framework](https://img.shields.io/badge/framework-PlatformIO-orange.svg)](https://platformio.org/)

---

## 📋 Indholdsfortegnelse

- [Features](#-features)
- [Hardware Setup](#️-hardware-setup)
- [Installation](#-installation)
- [Quick Start](#-quick-start)
- [CLI Kommandoer](#-cli-kommandoer)
- [Counter v3.2.0 Features](#-counter-v320-features)
- [Timer Features](#-timer-features)
- [Dokumentation](#-dokumentation)
- [Version Info](#-version-info)

---

## ✨ Features

### Modbus RTU Server
- ✅ Full Modbus RTU slave implementering
- ✅ Function codes: FC01, FC02, FC03, FC04, FC05, FC06, FC0F, FC10
- ✅ RS-485 automatic direction control (pin 8)
- ✅ Configurable slave ID (1-247 eller 0 for broadcast)
- ✅ Configurable baudrate (300-115200 bps)
- ✅ CRC16 validation
- ✅ Automatic RTU frame gap timing

### CLI (Command Line Interface)
- ✅ Interactive Cisco-style shell over USB Serial
- ✅ Remote echo & backspace support
- ✅ Command history (3 commands, arrow key navigation)
- ✅ 256 character command buffer
- ✅ Case-insensitive commands
- ✅ Aliases (sh, wr, rd, sv, ld, etc.)
- ✅ Comprehensive help system

### TimerEngine v2
- ✅ 4 independent timers
- ✅ Mode 1: One-shot sequences (3-phase)
- ✅ Mode 2: Monostable (retriggerable)
- ✅ Mode 3: Astable (blink/toggle)
- ✅ Mode 4: Input-triggered (2 sub-modes)
- ✅ Global status/control registers
- ✅ EEPROM persistence

### CounterEngine v7 (with v3.6.5 Unified Prescaler)
- ✅ 4 independent counters with **3 operating modes**
- ✅ **HW Mode (Timer5 ONLY)**: Direct hardware timer input on PIN 47 (PL2)
  - Highest precision, MAX ~20 kHz
  - CRITICAL FIX (v3.6.2): Timer5 uses PIN 47 (NOT pin 2!)
  - Only Timer5 routed to Arduino headers (Timer1/3/4 not available)
- ✅ **SW-ISR Mode**: Interrupt-driven edge detection (INT0-INT5: pins 2,3,18,19,20,21)
  - Hardware interrupt triggered, deterministic
  - MAX ~20 kHz capable
  - NEW in v3.4.0, unified prescaler in v3.6.1, production ready
- ✅ **SW Mode**: Software polling-based edge detection on GPIO pins
  - Flexible GPIO selection
  - Debounce filtering (1-255 ms)
  - MAX ~500 Hz (polling-limited)
  - Suitable for low-frequency signals
- ✅ **UNIFIED Prescaler Strategy (v3.6.0+)** - BREAKING CHANGE
  - ALL modes count EVERY edge (no edge skipping)
  - Prescaler division ONLY at output (raw register)
  - Consistent behavior: HW, SW, SW-ISR identical
  - Prescaler values: 1, 4, 8, 16, 64, 256, 1024
- ✅ Edge detection (rising/falling/both)
- ✅ Direction (up/down)
- ✅ BitWidth (8/16/32/64)
- ✅ Float scale factor
- ✅ **Frequency measurement (0-20 kHz) with 1-2 sec timing windows**
- ✅ **Separate raw register (counterValue / prescaler)**
- ✅ **Consistent parameter naming (index-reg, raw-reg, freq-reg)**
- ✅ **Automatic GPIO pin management for HW mode**
- ✅ **Dynamic GPIO mappings display**
- ✅ Control register (reset/start/stop/reset-on-read)
- ✅ Overflow detection & auto-reset
- ✅ Auto-start on boot (configurable)
- ✅ EEPROM persistence (schema v12)

### EEPROM Configuration
- ✅ Persistent configuration (schema v12 - GPIO persistence restored)
- ✅ CRC checksum validation
- ✅ Load/Save/Defaults commands
- ✅ Modbus SAVE via FC06 (write reg 0 = 255)
- ✅ Hostname persistence

### GPIO Management
- ✅ 54 digital pins available
- ✅ Dynamic mapping (pin ↔ coil/input)
- ✅ Static configuration persistence

---

## 🛠️ Hardware Setup

### Board Configuration
- **Platform**: Arduino Mega 2560 (ATmega2560 @ 16MHz)
- **RAM**: 8 KB (83.0% used, 1.3 KB free) - increased due to v3.3.0 global config allocation
- **Flash**: 256 KB (27.7% used) - plenty of headroom
- **EEPROM**: 4 KB (schema v12)

### Pin Assignments
| Pin | Function | Description |
|-----|----------|-------------|
| 0-1 | USB Serial | Debug/CLI (115200 baud) |
| 18-19 | Serial1 | Modbus RTU (configurable baud) |
| 8 | RS-485 DIR | Direction control for RS-485 transceiver |
| 13 | LED | Heartbeat indicator |
| **47 (PL2)** | **HW Timer5 - ONLY IMPLEMENTED** | **Timer5 external clock (HW counter mode) – v3.6.2 correct PIN** |
| 5, 6, 47 | Timer1/3/4 pins (NOT ROUTED) | Not available on Arduino headers |
| **2, 3, 18, 19, 20, 21** | **INT0-INT5** | **External interrupts (SW-ISR counter mode)** |
| 2-53 | GPIO | Available for coils/inputs and SW counters |

### RS-485 Wiring
| Arduino Pin | RS-485 Module | Function |
|-------------|---------------|----------|
| 18 (TX1) | DI | Data transmit |
| 19 (RX1) | RO | Data receive |
| 8 | DE + RE | Driver enable |
| VCC | VCC | 5V power |
| GND | GND | Ground |

---

## 💿 Installation

### Requirements
- [VS Code](https://code.visualstudio.com/)
- [PlatformIO Extension](https://platformio.org/install/ide?install=vscode)
- Arduino Mega 2560
- USB cable

### Quick Install
```bash
# Clone repository
git clone https://github.com/Jangreenlarsen/Modbus_server_slave.git
cd Modbus_server_slave

# Open in VS Code
code .

# Build
pio run

# Upload
pio run -t upload

# Monitor
pio device monitor
```

---

## 🚀 Quick Start

### 1. Upload Firmware
```bash
pio run -t upload
```

### 2. Open Serial Monitor
```bash
pio device monitor
```

### 3. Enter CLI Mode
Type `CLI` and press Enter to enter command mode.

### 4. Configure Counter - Three Operating Modes (v3.6.1)

#### Hardware Mode (Highest Precision, MAX ~20 kHz)
```bash
# Configure counter 1 in HARDWARE mode (Timer5 only on PIN 47)
set counter 1 mode 1 parameter hw-mode:hw-t5 \
  count-on:rising start-value:0 res:32 prescaler:1 \
  index-reg:100 raw-reg:104 freq-reg:108 \
  ctrl-reg:130 input-dis:125 direction:up scale:1.0

# Enable auto-start
set counter 1 start enable
```

#### Software-ISR Mode (Interrupt-Driven, MAX ~20 kHz)
```bash
# Configure counter 2 in SW-ISR mode (INT0 on pin 2)
set counter 2 mode 1 parameter hw-mode:sw-isr \
  count-on:rising start-value:0 res:32 prescaler:1 \
  index-reg:110 raw-reg:114 freq-reg:118 \
  ctrl-reg:131 input-dis:20 direction:up scale:1.0

# Map interrupt pin to discrete input (REQUIRED for SW-ISR!)
gpio map 2 input 20

# Enable auto-start
set counter 2 start enable
```

#### Software Polling Mode (Flexible GPIO, MAX ~500 Hz)
```bash
# Configure counter 3 in SOFTWARE mode (GPIO pin-based)
set counter 3 mode 1 parameter hw-mode:sw \
  count-on:rising start-value:0 res:32 prescaler:1 \
  index-reg:120 raw-reg:124 freq-reg:128 \
  ctrl-reg:132 input-dis:30 direction:up scale:2.5 \
  debounce:on debounce-ms:50

# Map GPIO pin 22 to discrete input 30
gpio map 22 input 30

# Enable auto-start
set counter 3 start enable
```

```bash
# Save all configuration
save
```

**Counter Mode Selection:**
- `hw-mode:hw-t5` - Timer5 on PIN 47 (PL2) - ONLY HW MODE IMPLEMENTED (v3.6.1+)
  - Timer1/3/4 NOT routed to Arduino headers (not available)
  - CRITICAL FIX v3.6.2: PIN 47, NOT pin 2!
- `hw-mode:sw-isr` - Software-ISR on INT0-INT5 (pins 2,3,18,19,20,21) – MAX ~20 kHz
- `hw-mode:sw` - Software polling on any GPIO pin – MAX ~500 Hz

**BREAKING CHANGE (v3.6.0+):**
- ALL modes now count EVERY edge (no edge skipping)
- Prescaler division happens ONLY at output (raw register)
- Unified values: 1, 4, 8, 16, 64, 256, 1024

### 5. View Status
```bash
show counters
show config
```

---

## 📟 CLI Kommandoer

### System Commands
```bash
show config              # Display full configuration
show version             # Show firmware version
save                     # Save config to EEPROM
load                     # Load config from EEPROM
defaults                 # Reset to factory defaults
reboot                   # Reboot system
set hostname <name>      # Set CLI hostname
```

### Counter Commands (v3.3.0)
```bash
# Configure counter with HW/SW mode and all parameters
set counter <id> mode 1 parameter \
  hw-mode:<sw|hw-t1|hw-t3|hw-t4|hw-t5>  # NEW: Hardware/Software mode selection
  count-on:<rising|falling|both> \
  start-value:<n> \
  res:<8|16|32|64> \
  prescaler:<1..256> \
  index-reg:<reg>      # Scaled value register
  raw-reg:<reg>        # Raw unscaled register
  freq-reg:<reg>       # Frequency in Hz
  overload-reg:<reg>   # Overflow flag register
  ctrl-reg:<reg>       # Control register
  input-dis:<idx>      # Discrete input index (Modbus input mapping)
  direction:<up|down> \
  scale:<float> \
  debounce:<on|off> \
  debounce-ms:<ms>     # Debounce delay in ms (SW mode only)

# Counter control
set counter <id> start ENABLE|DISABLE        # Auto-start on boot
set counter <id> reset-on-read ENABLE|DISABLE
reset counter <id>                           # Reset to start value
clear counters                               # Reset all counters

# View status
show counters            # Show counter status with Hz column
```

### Timer Commands
```bash
# Configure timer
set timer <id> mode <1|2|3|4> parameter \
  coil:<idx> P1:<high|low> P2:<high|low> \
  T1:<ms> T2:<ms> [T3:<ms>] \
  [trigger <di_idx> edge <rising|falling|both> sub <0|1>]

# Timer control
set timers status-reg:<n> control-reg:<n>
no set timer <id>        # Remove timer

# View status
show timers
```

### GPIO Commands
```bash
gpio map <pin> coil <idx>     # Map pin to coil
gpio map <pin> input <idx>    # Map pin to input
gpio unmap <pin>              # Remove mapping
show gpio                     # Show all mappings
```

### Modbus Commands
```bash
show regs [start] [count]     # Display holding registers
show coils                    # Display coil states
show inputs                   # Display input states
write reg <addr> <value>      # Write holding register
write coil <idx> <0|1>        # Write coil
```

---

## 🎯 Counter v3.3.0 Features

### Hardware Mode (NEW in v3.3.0)
```bash
# Configure counter in HARDWARE mode (direct timer input)
set counter 1 mode 1 parameter hw-mode:hw-t1 \
  count-on:rising start-value:0 res:32 prescaler:1 \
  index-reg:100 raw-reg:104 freq-reg:108 \
  ctrl-reg:130 input-dis:125 direction:up scale:1.0

# View configuration with GPIO mapping
show config    # GPIO section shows: "gpio 5 DYNAMIC at input 125 (counter1 HW-T1)"
show counters  # pin column shows: "5" (hardware GPIO pin for Timer1)
```

**Hardware Advantages:**
- Direct hardware timer input (no software overhead)
- Accurate frequency measurement (0-20 kHz)
- Fixed GPIO pins per timer (5, 47, 6, 46)
- Automatic GPIO mapping (DYNAMIC)
- Best for precision frequency measurement

**Hardware Timer Mapping:**
| Timer | GPIO Pin | Input Type | Use Case |
|-------|----------|-----------|----------|
| T1 | 5 | Input Capture | Recommended for frequency |
| T3 | 47 | Input Capture | High precision counting |
| T4 | 6 | Input Capture | Parallel frequency measurement |
| T5 | 46 | Input Capture | Additional high-speed input |

### Software Mode (GPIO Pin-Based)
```bash
# Configure counter in SOFTWARE mode (GPIO-based edge detection)
set counter 2 mode 1 parameter hw-mode:sw \
  count-on:rising start-value:0 res:32 prescaler:1 \
  index-reg:110 raw-reg:114 freq-reg:118 \
  ctrl-reg:131 input-dis:20 direction:up scale:2.5 \
  debounce:on debounce-ms:50

# Map GPIO pin to discrete input (required for SW mode)
gpio map 22 input 20

# View configuration
show config    # GPIO section shows: "gpio 22 at input 20"
show counters  # pin column shows: "22" (mapped GPIO pin)
```

**Software Advantages:**
- Flexible GPIO pin selection (any digital pin)
- Prescaler and debounce filtering
- Discrete input mapping (via `gpio map` command)
- Suitable for slow signals or variable mapping

**Software Limitations:**
- Software-based edge detection (less precise)
- Cannot use hardware timer inputs
- Prescaler only available in software mode

### Frequency Measurement
```bash
# Automatic frequency measurement (HW or SW mode)
# Updated every second with validation
set counter 1 mode 1 parameter \
  hw-mode:hw-t1 count-on:rising res:32 \
  index-reg:100 freq-reg:108 input-dis:125

# Read via Modbus FC04 (Read Input Registers)
read reg 100   # Scaled counter value
read reg 108   # Frequency in Hz (0-20000)
```

**Frequency Features:**
- Automatic Hz measurement every second
- Timing window validation (1-2 sec)
- Delta count validation (max 100kHz)
- Overflow wrap-around detection
- Result clamping (0-20000 Hz)
- Stable under Modbus bus activity
- Auto-reset on counter overflow

**Frequency Algorithm:**
1. Measure count delta over 1-2 second window
2. Validate delta against max 100kHz threshold
3. Detect overflow wrap-around
4. Calculate: `Hz = (delta_count × prescaler) / window_seconds`
5. Clamp result to 0-20000 Hz range
6. Reset on counter reset, overflow, or 5-sec timeout

### Raw Register
```bash
# Separate scaled and raw value registers
set counter 1 mode 1 parameter \
  index-reg:100 raw-reg:104 scale:2.5

# Read via Modbus
read reg 100   # Scaled value: counterValue × 2.5
read reg 104   # Raw value: actual counter value
```

**Use Cases:**
- `index-reg`: Application-specific scaled values (temperature, flow, etc.)
- `raw-reg`: Diagnostics and debugging
- Both updated simultaneously

### Consistent Naming
**Parameter names (v3.3.0 - backward compatible):**
- `hw-mode` = operation mode (NEW: sw, hw-t1, hw-t3, hw-t4, hw-t5)
- `index-reg` = scaled value register
- `raw-reg` = raw unscaled register
- `freq-reg` = frequency in Hz
- `overload-reg` = overflow flag register
- `ctrl-reg` = control bits register
- `input-dis` = discrete input index (Modbus mapping)

**Legacy names still work:**
- `reg` → `index-reg`
- `count-reg` → `index-reg`
- `control-reg` → `ctrl-reg`
- `input` → `input-dis`

### GPIO Mapping Reference
```bash
# View all GPIO mappings (including DYNAMIC HW counter mappings)
show config

# Example output for HW counter 1 on Timer1:
# GPIO Mappings:
#   gpio 5 DYNAMIC at input 125 (counter1 HW-T1)
#
# For SW counter 2 on pin 22:
#   gpio 22 at input 20

# Manually map GPIO pin to discrete input (SW mode)
gpio map 22 input 20
gpio map 25 input 21
gpio unmap 22

# Map GPIO pin to coil
gpio map 30 coil 10
gpio unmap 30
```

**GPIO Mapping Rules:**
- **HW Mode**: GPIO pins are automatic (5, 47, 6, 46 for T1, T3, T4, T5)
- **SW Mode**: User must map GPIO pin via `gpio map <pin> input <idx>`
- One GPIO pin can only map to one input or coil
- GPIO mappings are persistent (saved in EEPROM)

---

## ⏱️ Timer Features

### Mode 1: One-shot
```bash
set timer 1 mode 1 parameter \
  coil:10 P1:high P2:low P3:low \
  T1 1000 T2 500 T3 0
```

### Mode 3: Astable (Blink)
```bash
set timer 2 mode 3 parameter \
  coil:11 P1:high P2:low \
  T1 500 T2 500
```

### Mode 4: Input-triggered
```bash
# Single-phase pulse (retrigger after T1)
set timer 3 mode 4 parameter \
  coil:15 P1:high P2:low T1 200 \
  trigger 12 edge rising sub 0

# Two-phase sequence (retrigger after T1+T2)
set timer 4 mode 4 parameter \
  coil:16 P1:high P2:low T1 200 T2 300 \
  trigger 13 edge rising sub 1
```

---

## 📚 Dokumentation

### Manualer (v3.6.5)
- **[Modbus server V3.6.5 Manual - Complete System Reference.html](docs/Modbus%20server%20V3.6.3%20Manual%20-%20Display%20Commands%20&%20Bugfixes.html)** - ✨ **LATEST** Complete HTML manual with all v3.6.5 features
  - v3.6.5: Slave ID persistence fix, Baudrate sync improvements
  - Counter frequency limitations documented: HW/ISR ~20kHz, SW ~500Hz
  - Unified prescaler architecture (v3.6.0-v3.6.1)
  - All 3 counter modes: HW, SW, SW-ISR (v3.3.0 foundation + v3.6.5 complete)
  - ATmega2560 hardware limitation documentation (v3.4.7 discovery)
  - BREAKING CHANGES documentation
  - Interactive CLI command reference
  - Frequency measurement with prescaler
  - Practical examples with all 3 modes
  - Complete troubleshooting guide

- **[MANUAL.md](MANUAL.md)** - Komplet Markdown manual v3.3.0 (dansk/english)
  - Hardware/Software counter modes
  - GPIO mapping and DYNAMIC configuration
  - CLI kommando reference med eksempler
  - Frequency measurement og timer features
  - Extensive troubleshooting section

### Guides
- **[INSTALLATION.md](INSTALLATION.md)** - Detaljeret installations guide
- **[PLATFORMIO_REFERENCE.md](PLATFORMIO_REFERENCE.md)** - PlatformIO quick reference
- **[STATUS.md](STATUS.md)** - Projekt status og build info
- **[TODO.md](TODO.md)** - Future enhancements roadmap

---

## 📊 Version Info

```
Version:        v3.6.5
Build Date:     2025-11-15
EEPROM Schema:  v12
Platform:       Arduino Mega 2560 (ATmega2560 @ 16MHz)
Framework:      Arduino + PlatformIO
RAM Usage:      83.0% (6798 / 8192 bytes) - 1.3 KB free
Flash Usage:    27.7% (70226 / 253952 bytes) - good headroom
Status:         ✅ PRODUCTION READY (Complete with Slave ID Persistence Fix)
```

### v3.6.5 Highlights (2025-11-15) - Slave ID Persistence & Frequency Documentation
- 🔴 **CRITICAL BUGFIX: Slave ID and Baudrate Persistence**
  - `set id` and `set baud` commands now sync globalConfig immediately
  - Prevents accidental revert if configApply() called later
  - User must still run `save` to persist to EEPROM
- 📊 **DOCUMENTATION: Counter Frequency Limitations**
  - HW (Timer5 only): MAX ~20 kHz input frequency
  - SW-ISR (INT0-INT5): MAX ~20 kHz input frequency
  - SW (Polling): MAX ~500 Hz input frequency (main loop overhead)
- ✅ **Cleaned Debug Output** – removed verbose boot messages
- 📝 **Updated HTML Manual** – added frequency limitations table and Timer5 PIN documentation

### v3.6.1 Highlights (2025-11-14) - Unified Prescaler Architecture Complete
- 🔴 **CRITICAL FIX: SW-ISR mode prescaler consistency**
  - Removed edge-skipping from interrupt handler
  - NOW: ALL three modes (HW, SW, SW-ISR) count EVERY edge
  - Prescaler division happens ONLY at output (raw register)
  - Complete consistency across all counter modes
- ✅ **Unified Prescaler Strategy (v3.6.0-v3.6.1)**
  - HW mode: Hardware counts all pulses → raw = value / prescaler
  - SW mode: Software counts all edges → raw = value / prescaler
  - SW-ISR mode: ISR counts all edges → raw = value / prescaler
  - Prescaler values: 1, 4, 8, 16, 64, 256, 1024
- ✅ **SW-ISR Mode Production Ready**
  - Interrupt-driven edge detection (INT0-INT5: pins 2,3,18,19,20,21)
  - Deterministic, 5-20 kHz capable
  - Fully integrated with unified prescaler architecture
- ⚠️ **BREAKING CHANGE**: SW/SW-ISR counters will show different values after upgrade

### v3.6.0 Highlights (2025-11-14) - CRITICAL: SW Mode Prescaler Fix
- 🔴 **CRITICAL FIX: SW mode prescaler consistency with HW mode**
  - Removed edgeCount prescaler check (was double-counting)
  - ALL modes now count EVERY edge (no edge skipping)
  - Prescaler division happens ONLY at output
  - Unified behavior: HW, SW, SW-ISR identical
- 💡 **Discovery: ATmega2560 Hardware Limitation (v3.4.7)**
  - External clock mode CANNOT use hardware prescaler
  - Solution: 100% software prescaler implementation

### v3.4.7 Highlights (2025-11-14) - Software Prescaler Implementation
- 🔍 **ROOT CAUSE DISCOVERED**: ATmega2560 Timer5 hardware limitation
  - External clock (0x07): Counts pulses, no prescaler available
  - Internal prescaler (0x02-0x05): Count 16MHz system clock, ignore pulses!
  - Solution: Hardware ALWAYS uses external clock mode
  - Prescaler implemented 100% in software

### v3.4.3 Highlights (2025-11-13) - Timer5 Pin Mapping Fix
- 🔴 **CRITICAL: Timer5 uses pin 2 (NOT pin 46!)**
  - Verified working with hardware testing
  - Updated all documentation

### Version History (Selection)
| Version | Date | Key Features |
|---------|------|-------------|
| **v3.6.5** | 2025-11-15 | 🔴 Slave ID/Baud persistence fix, Counter frequency limitations documented, clean debug output |
| **v3.6.3** | 2025-11-15 | 🔴 reset-on-read HW fix, frequency DOWN direction fix, show config improvements |
| **v3.6.1** | 2025-11-14 | 🔴 SW-ISR prescaler fix, unified architecture, SW-ISR production ready |
| **v3.6.0** | 2025-11-14 | 🔴 CRITICAL: SW mode prescaler fix, unified strategy |
| v3.5.0 | 2025-11-14 | Show counters display bug fix |
| v3.4.7 | 2025-11-14 | 🔴 Software prescaler implementation, ATmega2560 limitation discovered |
| v3.4.3 | 2025-11-13 | 🔴 Timer5 pin correction (PIN 47, not pin 2) |
| v3.4.0 | 2025-11-13 | SW-ISR interrupt mode production ready |
| v3.3.0 | 2025-11-11 | Hardware counter mode, HW/SW dual modes, GPIO management |
| v3.2.0 | 2025-11-10 | Frequency measurement, raw register, consistent naming |
| v3.1.9 | 2025-11-09 | Counter control improvements, CLI enhancements |

---

## 🤝 Contributing

Contributions are welcome! Please:
1. Fork the repository
2. Create a feature branch
3. Submit a pull request

---

## 📞 Support

- **GitHub Issues**: [Report bugs or request features](https://github.com/Jangreenlarsen/Modbus_server_slave/issues)
- **Latest Release**: [v3.6.5](https://github.com/Jangreenlarsen/Modbus_server_slave/releases/tag/v3.6.5)
- **Repository**: [Jangreenlarsen/Modbus_server_slave](https://github.com/Jangreenlarsen/Modbus_server_slave)

---

## 📄 License

This project is open source. Check repository for license details.

---

## 🎉 Acknowledgments

Built with:
- [PlatformIO](https://platformio.org/) - Modern embedded development
- [Arduino Framework](https://www.arduino.cc/) - Hardware abstraction
- [VS Code](https://code.visualstudio.com/) - Development environment

---

**Status: PRODUCTION READY** ✅

*Last updated: 2025-11-15 | v3.6.5 - Slave ID Persistence Fix & Frequency Documentation*
