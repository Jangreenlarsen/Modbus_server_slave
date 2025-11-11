# Modbus RTU Server v3.3.0

🚀 **Production-Ready** Arduino Mega 2560 Modbus RTU Server med CLI, Timer Engine og Hardware/Software Counter Engine.

[![Version](https://img.shields.io/badge/version-3.3.0-blue.svg)](https://github.com/Jangreenlarsen/Modbus_server_slave/releases/tag/v3.3.0)
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

### CounterEngine v4 (with v3.3.0 HW/SW support)
- ✅ 4 independent counters
- ✅ **Hybrid HW/SW operation mode - NEW in v3.3.0**
  - **HW Mode**: Direct hardware timer input (Timer1/3/4/5 on pins 5/47/6/46)
  - **SW Mode**: Software edge detection on GPIO pins
- ✅ Edge detection (rising/falling/both)
- ✅ Prescaler (1-256)
- ✅ Direction (up/down)
- ✅ BitWidth (8/16/32/64)
- ✅ Float scale factor
- ✅ Debounce (configurable ms for SW mode)
- ✅ **Frequency measurement (0-20 kHz) with 1-2 sec timing windows**
- ✅ **Separate raw register (unscaled values)**
- ✅ **Consistent parameter naming (index-reg, raw-reg, freq-reg)**
- ✅ **Automatic GPIO pin management for HW mode**
- ✅ **Dynamic GPIO mappings display**
- ✅ Control register (reset/start/stop/reset-on-read)
- ✅ Overflow detection & auto-reset
- ✅ Auto-start on boot (configurable)
- ✅ EEPROM persistence (schema v10)

### EEPROM Configuration
- ✅ Persistent configuration (schema v10 - HW counter support)
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
- **RAM**: 8 KB (56.8% used)
- **Flash**: 256 KB (20.1% used)
- **EEPROM**: 4 KB

### Pin Assignments
| Pin | Function | Description |
|-----|----------|-------------|
| 0-1 | USB Serial | Debug/CLI (115200 baud) |
| 18-19 | Serial1 | Modbus RTU (configurable baud) |
| 8 | RS-485 DIR | Direction control for RS-485 transceiver |
| 13 | LED | Heartbeat indicator |
| 2-53 | GPIO | Available for coils/inputs (except above) |

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

### 4. Configure Counter - Hardware Mode (NEW in v3.3.0)
```bash
# Configure counter 1 in HARDWARE mode (Timer1 on pin 5)
set counter 1 mode 1 parameter hw-mode:hw-t1 \
  count-on:rising start-value:0 res:32 prescaler:1 \
  index-reg:100 raw-reg:104 freq-reg:108 \
  overload-reg:120 ctrl-reg:130 input-dis:125 \
  direction:up scale:1.0

# Configure counter 2 in SOFTWARE mode (GPIO pin-based)
set counter 2 mode 1 parameter hw-mode:sw \
  count-on:rising start-value:0 res:32 prescaler:1 \
  index-reg:110 raw-reg:114 freq-reg:118 \
  ctrl-reg:131 input-dis:20 direction:up scale:2.5 \
  debounce:on debounce-ms:50

# Enable auto-start on boot
set counter 1 start enable
set counter 2 start enable

# Save configuration
save
```

**Hardware Timer Selection:**
- `hw-mode:hw-t1` - Timer1 on pin 5 (recommended frequency input)
- `hw-mode:hw-t3` - Timer3 on pin 47
- `hw-mode:hw-t4` - Timer4 on pin 6
- `hw-mode:hw-t5` - Timer5 on pin 46
- `hw-mode:sw` - Software mode (GPIO pin-based edge detection)

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

### Manualer
- **[MANUAL.md](MANUAL.md)** - Komplet Modbus RTU Server manual v3.3.0 (dansk/english)
  - Hardware/Software counter modes (NEW in v3.3.0)
  - GPIO mapping and DYNAMIC configuration
  - CLI kommando reference med eksempler
  - Frequency measurement og timer features
  - Troubleshooting og best practices
- **[Modbus server V3.2.0 Manual](Modbus%20server%20V3.2.0%20Manual%20-%20counter%20adv%20mode.html)** - Legacy manual (dansk)

### Guides
- **[INSTALLATION.md](INSTALLATION.md)** - Detaljeret installations guide
- **[PLATFORMIO_REFERENCE.md](PLATFORMIO_REFERENCE.md)** - PlatformIO quick reference
- **[STATUS.md](STATUS.md)** - Projekt status og build info
- **[TODO.md](TODO.md)** - Future enhancements roadmap

---

## 📊 Version Info

```
Version:        v3.3.0
Build Date:     2025-11-11
EEPROM Schema:  v10 (HW counter support)
Platform:       Arduino Mega 2560 (ATmega2560 @ 16MHz)
Framework:      Arduino + PlatformIO
RAM Usage:      82.4% (6751 / 8192 bytes)
Flash Usage:    21.2% (53956 / 253952 bytes)
Status:         ✅ PRODUCTION READY
```

### v3.3.0 Highlights (2025-11-11) - Hardware Counter Engine
- ✨ **NEW: Hardware counter support (HW/SW dual mode)**
  - 4 independent hardware timers (T1/T3/T4/T5 on pins 5/47/6/46)
  - Software GPIO-based fallback mode
  - Automatic GPIO mapping (DYNAMIC) for HW mode
  - Explicit timer selection: `hw-mode:hw-t1|hw-t3|hw-t4|hw-t5`
- ✨ Improved CLI configuration display
  - Separate discrete input and GPIO pin display
  - DYNAMIC GPIO mappings in config section
  - Corrected pin column in `show counters` (actual GPIO pins)
- ✨ Fixed SAVE command stack overflow (global config allocation)
- ✨ EEPROM schema v10 with hw-mode field
- ✨ Backward compatible with v3.2.0

### Version History
| Version | Date | Key Features |
|---------|------|-------------|
| v3.3.0 | 2025-11-11 | Hardware counter mode, GPIO management, improved CLI display |
| v3.2.0 | 2025-11-10 | Frequency measurement, raw register, consistent naming |
| v3.1.9 | 2025-11-09 | Counter control improvements, CLI enhancements |
| v3.1.7 | 2025-11 | Raw counter value, reset-on-read, auto-start |
| v3.1.4 | 2025-05 | CounterEngine v3 (scale/direction) |

---

## 🤝 Contributing

Contributions are welcome! Please:
1. Fork the repository
2. Create a feature branch
3. Submit a pull request

---

## 📞 Support

- **GitHub Issues**: [Report bugs or request features](https://github.com/Jangreenlarsen/Modbus_server_slave/issues)
- **Latest Release**: [v3.2.0](https://github.com/Jangreenlarsen/Modbus_server_slave/releases/tag/v3.2.0)
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

*Last updated: 2025-11-11 | v3.3.0*
