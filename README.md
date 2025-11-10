# Modbus RTU Server v3.2.0

🚀 **Production-Ready** Arduino Mega 2560 Modbus RTU Server med CLI, Timer Engine og Counter Engine.

[![Version](https://img.shields.io/badge/version-3.2.0-blue.svg)](https://github.com/Jangreenlarsen/Modbus_server_slave/releases/tag/v3.2.0)
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

### CounterEngine v3 (with v3.2.0 enhancements)
- ✅ 4 independent counters
- ✅ Edge detection (rising/falling/both)
- ✅ Prescaler (1-256)
- ✅ Direction (up/down)
- ✅ BitWidth (8/16/32/64)
- ✅ Float scale factor
- ✅ Debounce (configurable ms)
- ✅ **Frequency measurement (0-20 kHz) - NEW in v3.2.0**
- ✅ **Separate raw register - NEW in v3.2.0**
- ✅ **Consistent naming (index-reg, raw-reg, freq-reg, etc.) - NEW in v3.2.0**
- ✅ Control register (reset/start/stop/reset-on-read)
- ✅ Overflow detection & auto-reset
- ✅ Auto-start on boot (configurable)
- ✅ EEPROM persistence

### EEPROM Configuration
- ✅ Persistent configuration (schema v9)
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

### 4. Configure Counter with Frequency Measurement
```bash
# Configure counter 1 with all new v3.2.0 features
set counter 1 mode 1 parameter count-on:rising start-value:0 res:32 prescaler:1 \
  index-reg:100 raw-reg:104 freq-reg:108 \
  overload-reg:120 ctrl-reg:130 input-dis:20 \
  direction:up scale:1.0 debounce:on debounce-ms:50

# Enable auto-start on boot
set counter 1 start enable

# Save configuration
save
```

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

### Counter Commands (v3.2.0)
```bash
# Configure counter with new parameters
set counter <id> mode 1 parameter \
  count-on:<rising|falling|both> \
  start-value:<n> \
  res:<8|16|32|64> \
  prescaler:<1..256> \
  index-reg:<reg>      # Scaled value register
  raw-reg:<reg>        # Raw unscaled register (NEW)
  freq-reg:<reg>       # Frequency in Hz (NEW)
  overload-reg:<reg>   # Overflow flag register
  ctrl-reg:<reg>       # Control register
  input-dis:<idx>      # Discrete input index
  direction:<up|down> \
  scale:<float> \
  debounce:<on|off> \
  debounce-ms:<ms>

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

## 🎯 Counter v3.2.0 Features

### Frequency Measurement
```bash
# Configure counter with frequency measurement
set counter 1 mode 1 parameter \
  count-on:rising res:32 prescaler:1 \
  index-reg:100 freq-reg:108 input-dis:20

# Read via Modbus
read reg 100   # Scaled counter value
read reg 108   # Frequency in Hz (0-20000)
```

**Features:**
- Automatic Hz measurement every second
- Stable under Modbus bus activity
- Timing window validation (1-2 sec)
- Delta count validation (max 100kHz)
- Result clamping (0-20000 Hz)
- Automatic reset on counter reset/overflow

### Raw Register
```bash
# Separate registers for scaled and raw values
set counter 1 mode 1 parameter \
  index-reg:100 raw-reg:104 scale:2.5

# Read via Modbus
read reg 100   # Scaled value (counterValue × 2.5)
read reg 104   # Raw unscaled value
```

### Consistent Naming
**New parameter names (v3.2.0):**
- `index-reg` (prev. reg/count-reg) = scaled value
- `raw-reg` (new) = raw unscaled value
- `freq-reg` (new) = frequency in Hz
- `overload-reg` (prev. overload) = overflow flag
- `ctrl-reg` (prev. control-reg) = control bits
- `input-dis` (prev. input) = discrete input index

**Backward compatible:** Old names still work!

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
- **[Modbus server V3.2.0 Manual](Modbus%20server%20V3.2.0%20Manual%20-%20counter%20adv%20mode.html)** - Komplet system manual (dansk)
  - Timer og Counter dokumentation
  - CLI kommando reference
  - Versionshistorik
  - Eksempler og troubleshooting

### Guides
- **[INSTALLATION.md](INSTALLATION.md)** - Detaljeret installations guide
- **[PLATFORMIO_REFERENCE.md](PLATFORMIO_REFERENCE.md)** - PlatformIO quick reference
- **[STATUS.md](STATUS.md)** - Projekt status og build info
- **[TODO.md](TODO.md)** - Future enhancements roadmap

---

## 📊 Version Info

```
Version:        v3.2.0
Build Date:     2025-11-10
EEPROM Schema:  v9
Platform:       Arduino Mega 2560 (ATmega2560 @ 16MHz)
Framework:      Arduino + PlatformIO
RAM Usage:      56.8% (4655 / 8192 bytes)
Flash Usage:    20.1% (51058 / 253952 bytes)
Status:         ✅ PRODUCTION READY
```

### v3.2.0 Highlights (2025-11-10)
- ✨ Counter frequency measurement (Hz)
- ✨ Configurable raw register
- ✨ Consistent parameter naming
- ✨ CLI buffer increased to 256 chars
- ✨ Frequency stability improvements
- ✨ Backward compatible with v3.1.x

### Version History
| Version | Date | Key Features |
|---------|------|-------------|
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

*Last updated: 2025-11-10 | v3.2.0*
