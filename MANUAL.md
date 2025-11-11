# Modbus RTU Server v3.3.0 - Complete User Manual

**Version**: v3.3.0
**Date**: 2025-11-11
**Platform**: Arduino Mega 2560
**EEPROM Schema**: v10
**Status**: Production Ready ✅

---

## 📋 Table of Contents

1. [Introduction](#introduction)
2. [Hardware Setup](#hardware-setup)
3. [Quick Start](#quick-start)
4. [Counter Engine v4 (HW/SW Modes)](#counter-engine-v4-hwsw-modes)
5. [Timer Engine v2](#timer-engine-v2)
6. [GPIO Management](#gpio-management)
7. [CLI Command Reference](#cli-command-reference)
8. [Modbus Register Reference](#modbus-register-reference)
9. [Practical Examples](#practical-examples)
10. [Troubleshooting](#troubleshooting)
11. [Version History](#version-history)

---

## Introduction

Modbus RTU Server v3.3.0 is a production-ready embedded system for Arduino Mega 2560 that implements a complete Modbus RTU slave server with advanced features:

- **Modbus RTU Server**: Full protocol implementation with RS-485 support
- **Counter Engine v4**: 4 independent counters with hardware/software dual modes
- **Timer Engine v2**: 4 independent programmable timers with 4 operation modes
- **Interactive CLI**: Cisco-style command shell over USB serial
- **EEPROM Persistence**: Configuration saved to non-volatile memory (schema v10)

### Key Features

| Feature | Count | Details |
|---------|-------|---------|
| Independent Counters | 4 | HW timers (T1/T3/T4/T5) or software GPIO-based |
| Independent Timers | 4 | Modes: one-shot, monostable, astable, input-triggered |
| Modbus Registers | 160 | Holding registers + Input registers |
| Coils/Inputs | 256 | Packed bit arrays for digital I/O |
| GPIO Pins | 54 | Digital pins available for mapping |
| Static Config | 32 regs + 64 coils | Persistent register/coil values |

---

## Hardware Setup

### Board Configuration

- **Platform**: Arduino Mega 2560 (ATmega2560 @ 16 MHz)
- **RAM**: 8 KB (82.4% utilized)
- **Flash**: 256 KB (21.2% utilized)
- **EEPROM**: 4 KB (configuration storage)

### Pin Assignments

| Pin(s) | Function | Baud Rate | Notes |
|--------|----------|-----------|-------|
| 0-1 | USB Serial (Serial) | 115200 | Debug/CLI interface |
| 18-19 | Modbus RTU (Serial1) | Configurable | RS-485 data lines |
| 8 | RS-485 Direction | - | Control pin for transceiver enable |
| 13 | LED Heartbeat | - | 1 Hz toggle indicator |
| **5** | **Timer1 (HW Counter)** | - | Hardware input capture (ICP1) |
| **47** | **Timer3 (HW Counter)** | - | Hardware input capture (ICP3) |
| **6** | **Timer4 (HW Counter)** | - | Hardware input capture (ICP4) |
| **46** | **Timer5 (HW Counter)** | - | Hardware input capture (ICP5) |
| 2-4, 7, 9-12, 14-17, 20-45, 48-53 | GPIO | - | Available for digital I/O |

### RS-485 Wiring Diagram

```
Arduino Mega 2560          RS-485 Module (MAX485 or similar)
==================         ===============================

TX1 (pin 18)    -------->  DI (Data In)
RX1 (pin 19)    <--------  RO (Data Out)
GPIO 8          -------->  DE + RE (Driver Enable, both tied together)
VCC             -------->  VCC (+5V)
GND             -------->  GND

RS-485 Bus (Modbus Network)
A  ----+---- (twisted pair)
B  ----+---- (twisted pair)
GND -- ground reference (optional)
```

### Power Requirements

- **Arduino**: 5V via USB or barrel jack
- **RS-485 Module**: 5V from Arduino (or external)
- **Total Current**: ~200 mA typical (includes RS-485 transceiver)

---

## Quick Start

### 1. Build and Upload

```bash
# Open repository in VS Code
cd Modbus_server_slave
code .

# Build firmware
pio run

# Upload to Arduino Mega 2560
pio run -t upload

# Monitor serial output
pio device monitor -b 115200
```

### 2. Initial Configuration

```bash
# Enter CLI mode (type at serial monitor)
CLI

# Configure system
set hostname mymodbusserver
set slaveid 1
set baudrate 9600

# Save configuration to EEPROM
save

# Reboot to apply
reboot
```

### 3. Configure Counter with Frequency Measurement

#### Hardware Mode (Recommended)
```bash
# Counter 1 in HARDWARE mode (Timer1, pin 5)
set counter 1 mode 1 parameter \
  hw-mode:hw-t1 \
  count-on:rising \
  start-value:0 \
  res:32 \
  prescaler:1 \
  index-reg:100 \
  raw-reg:104 \
  freq-reg:108 \
  ctrl-reg:130 \
  input-dis:125 \
  direction:up \
  scale:1.0

# Enable auto-start on boot
set counter 1 start enable

# Save configuration
save
```

#### Software Mode (GPIO-Based)
```bash
# Counter 2 in SOFTWARE mode (GPIO pin 22)
set counter 2 mode 1 parameter \
  hw-mode:sw \
  count-on:rising \
  start-value:0 \
  res:32 \
  prescaler:1 \
  index-reg:110 \
  raw-reg:114 \
  freq-reg:118 \
  ctrl-reg:131 \
  input-dis:20 \
  direction:up \
  scale:2.5 \
  debounce:on \
  debounce-ms:50

# Map GPIO pin to discrete input
gpio map 22 input 20

# Enable auto-start
set counter 2 start enable

# Save
save
```

### 4. Verify Configuration

```bash
# View complete configuration
show config

# Display counter status with frequency
show counters

# Display timer status
show timers

# Display GPIO mappings
show gpio
```

---

## Counter Engine v4 (HW/SW Modes)

### Overview

The Counter Engine v4 is a hybrid system supporting both hardware and software counting modes:

- **Hardware Mode**: Direct timer input from hardware timers T1/T3/T4/T5
- **Software Mode**: GPIO pin-based edge detection with software filtering

### Hardware Mode (HW)

#### Advantages
- **Direct Hardware Input**: Uses Arduino timer input capture pins (ICP1, ICP3, ICP4, ICP5)
- **No Software Overhead**: Hardware counts edges independently
- **Accurate Frequency**: Stable frequency measurement even under Modbus traffic
- **Best Precision**: Ideal for high-frequency signals (up to 20 kHz)

#### Timer Selection

Each hardware timer maps to a fixed GPIO pin:

```
Timer   GPIO Pin   Use Case              Description
-----   --------   --------              -----------
T1      5          Frequency input       Recommended for precision
T3      47         High-speed counting   Parallel operation possible
T4      6          Frequency measurement Additional independent channel
T5      46         General input capture 4th independent counter
```

#### Configuration Example

```bash
# Counter 1 on Timer1 (pin 5) - Frequency measurement
set counter 1 mode 1 parameter \
  hw-mode:hw-t1 \
  count-on:rising \
  res:32 \
  prescaler:1 \
  index-reg:100 \
  raw-reg:104 \
  freq-reg:108 \
  ctrl-reg:130 \
  input-dis:125 \
  direction:up \
  scale:1.0

# Counter 3 on Timer4 (pin 6) - Independent measurement
set counter 3 mode 1 parameter \
  hw-mode:hw-t4 \
  count-on:both \
  res:32 \
  prescaler:1 \
  index-reg:120 \
  raw-reg:124 \
  freq-reg:128 \
  ctrl-reg:132 \
  input-dis:126 \
  direction:up \
  scale:1.0
```

#### GPIO Mapping (HW Mode)

Hardware counters automatically create **DYNAMIC GPIO mappings** that appear in `show config`:

```
GPIO Mappings:
  gpio 5 DYNAMIC at input 125 (counter1 HW-T1)
  gpio 47 DYNAMIC at input 126 (counter2 HW-T3)
  gpio 6 DYNAMIC at input 127 (counter3 HW-T4)
  gpio 46 DYNAMIC at input 128 (counter4 HW-T5)
```

These mappings:
- Are **read-only** (determined by hardware)
- Are **displayed in show config** GPIO section
- Appear in `show counters` pin column
- Cannot be manually overridden

### Software Mode (SW)

#### Advantages
- **Flexible GPIO**: Use any digital pin (2-53 except reserved pins)
- **Debounce Filtering**: Built-in noise filtering (1-255 ms)
- **Prescaler Support**: Hardware edge detection with software counting
- **Variable Mapping**: Change GPIO pins via `gpio map` command

#### Advantages
- **Less Precise**: Software-based edge detection has jitter
- **Slower Maximum**: Limited by software loop frequency
- **No Parallel Timer**: Requires separate discrete input mapping

#### Limitations
- Maximum frequency ~5 kHz (software-limited)
- Cannot use hardware timer input capture
- Prescaler is optional but recommended

#### Configuration Example

```bash
# Counter 2 on GPIO pin 22 - Software mode
set counter 2 mode 1 parameter \
  hw-mode:sw \
  count-on:rising \
  res:32 \
  prescaler:1 \
  index-reg:110 \
  raw-reg:114 \
  freq-reg:118 \
  ctrl-reg:131 \
  input-dis:20 \
  direction:up \
  scale:2.5 \
  debounce:on \
  debounce-ms:50

# Map GPIO pin 22 to discrete input 20
gpio map 22 input 20

# Verify mapping
show gpio    # Should show: gpio 22 at input 20
show config  # GPIO section shows the mapping
```

#### GPIO Mapping (SW Mode)

Software counters require manual GPIO mapping via `gpio map` command:

```bash
# Map multiple GPIO pins to discrete inputs
gpio map 22 input 20
gpio map 25 input 21
gpio map 30 input 22

# View mappings
show config

# Output:
# GPIO Mappings:
#   gpio 22 at input 20
#   gpio 25 at input 21
#   gpio 30 at input 22
```

Rules:
- One GPIO pin per discrete input
- One GPIO pin per coil
- Cannot map same GPIO to multiple inputs
- Mappings are persistent (saved in EEPROM)

### Counter Parameters

#### Mode Selection

```
hw-mode:<sw|hw-t1|hw-t3|hw-t4|hw-t5>
```

| Value | Mode | GPIO Pin | Use |
|-------|------|----------|-----|
| `sw` | Software | User-selectable | GPIO-based counting |
| `hw-t1` | Hardware T1 | 5 (ICP1) | Frequency input |
| `hw-t3` | Hardware T3 | 47 (ICP3) | Independent counting |
| `hw-t4` | Hardware T4 | 6 (ICP4) | Parallel measurement |
| `hw-t5` | Hardware T5 | 46 (ICP5) | Additional input |

#### Edge Detection

```
count-on:<rising|falling|both>
```

- **rising**: Count rising edges (low→high)
- **falling**: Count falling edges (high→low)
- **both**: Count all edges (both directions)

#### Resolution (Bit Width)

```
res:<8|16|32|64>
```

Counter value bit width:
- **8**: 0-255 (auto-resets at 256)
- **16**: 0-65535 (auto-resets at 65536)
- **32**: 0-4294967295 (most common)
- **64**: 0-18446744073709551615 (maximum range)

#### Prescaler

```
prescaler:<1-256>
```

Divides input frequency before counting:
- **1**: Count every edge (no division)
- **2**: Count every 2nd edge
- **4**: Count every 4th edge
- **256**: Count every 256th edge

Used with frequency measurement: `Hz = (raw_edges / prescaler) / time_window`

#### Registers

```
index-reg:<0-159>      # Scaled counter value
raw-reg:<0-159>        # Raw unscaled value
freq-reg:<0-159>       # Frequency in Hz
overload-reg:<0-159>   # Overflow flag register
ctrl-reg:<0-159>       # Control register
```

Each register holds:
- **index-reg**: `counterValue × scale` (application value)
- **raw-reg**: Raw counter value (unscaled)
- **freq-reg**: Frequency in Hz (0-20000)
- **overload-reg**: Bit 0 = overflow flag
- **ctrl-reg**: Control bits (see Control Register section)

#### Discrete Input Mapping

```
input-dis:<0-NUM_DISCRETE>
```

Maps counter to Modbus discrete input index for:
- **HW Mode**: Not used for GPIO (DYNAMIC mapping)
- **SW Mode**: Must match `gpio map <pin> input <idx>`

Example: Counter using discrete input 125
```bash
set counter 1 mode 1 parameter input-dis:125 ...
gpio map 5 input 125   # For SW mode only
```

#### Direction

```
direction:<up|down>
```

- **up**: Increment on count pulse (normal counting)
- **down**: Decrement on count pulse (reverse counting)

#### Scale Factor

```
scale:<float>
```

Applied to index-reg output:
- **1.0**: No scaling (index = raw)
- **2.5**: index = raw × 2.5
- **0.5**: index = raw × 0.5
- **0.1**: index = raw × 0.1

Example: Flow meter with 0.1 GPM per pulse → `scale:0.1`

#### Debounce (SW Mode Only)

```
debounce:<on|off>
debounce-ms:<1-255>
```

Filters noise in software mode:
- **on**: Enable debounce filtering
- **off**: Disable debounce (count immediately)
- **debounce-ms**: Filter delay in milliseconds

Example: `debounce:on debounce-ms:50` waits 50ms before counting edge

### Control Register (ctrlReg)

The control register is a Modbus holding register with bit-level control:

```
Bit 0: Reset            (RW) - Set to 1 to reset counter to start-value
Bit 1: Start            (RW) - Set to 1 to enable counter (start counting)
Bit 2: Stop             (RW) - Set to 1 to disable counter (stop counting)
Bit 3: Reset-on-Read    (RW) - Auto-reset after read (persistent)
Bits 4-15: Reserved
```

#### Control Examples

```bash
# Read control register
read reg 130

# Reset counter via Modbus FC06 (Write Single Register)
# Write 0x0001 to control register (set bit 0)
write reg 130 1

# Stop counter
write reg 130 4

# Start counter
write reg 130 2

# Enable reset-on-read
write reg 130 8

# Reset + Auto-start (bits 0 + 1)
write reg 130 3
```

### Frequency Measurement

Frequency is automatically measured and stored in `freq-reg` every second.

#### Frequency Algorithm

```
1. Measure raw counter delta over 1-2 second window
2. Validate delta against max 100 kHz threshold
3. Detect overflow wrap-around (for 32/64-bit widths)
4. Calculate: Hz = (delta_count × prescaler) / window_seconds
5. Clamp result to 0-20000 Hz range
6. Reset on counter overflow or 5-second idle timeout
```

#### Frequency Features

- **Window**: 1-2 second measurement window
- **Delta Validation**: Max 100 kHz accepted (higher = rejected)
- **Overflow Detection**: Handles counter bit-width wraparound
- **Clamping**: Result forced to 0-20000 Hz range
- **Stability**: Immune to Modbus read traffic
- **Reset**: Auto-resets on counter reset, overflow, or timeout

#### Frequency Examples

```bash
# Example 1: Measure 1000 Hz signal (1kHz oscillator)
# Configuration:
set counter 1 mode 1 parameter \
  hw-mode:hw-t1 count-on:rising res:32 \
  prescaler:1 freq-reg:108 ...

# Expected result after 1 second:
# freq-reg (108) = 1000 Hz

# Example 2: Measure 50 Hz mains frequency
set counter 2 mode 1 parameter \
  hw-mode:hw-t3 count-on:both res:32 \
  prescaler:1 freq-reg:118 ...

# With prescaler=1, count-on:both → 100 edges per second
# Expected: freq-reg = 50 Hz (100 edges / 2 = 50 Hz)

# Example 3: Measure high frequency with prescaler
set counter 3 mode 1 parameter \
  hw-mode:hw-t4 count-on:rising res:32 \
  prescaler:4 freq-reg:128 ...

# 4000 edge/sec input ÷ 4 prescaler = 1000 counted edges/sec
# Expected: freq-reg = 1000 Hz
```

### Overflow Handling

When counter reaches maximum value for selected resolution:

```
Resolution 8:  255 → 0 (auto-reset)
Resolution 16: 65535 → 0 (auto-reset)
Resolution 32: 4294967295 → 0 (auto-reset)
Resolution 64: (very large number → 0)
```

Overflow behavior:
- **Auto-reset**: Counter wraps to 0 (hardware/software automatically)
- **Overflow flag**: Set in overload-reg (bit 0) when overflow occurs
- **Frequency reset**: Hz measurement resets to 0
- **Frequency stability**: Window detection prevents false frequency spikes

---

## Timer Engine v2

### Overview

4 independent programmable timers with 4 operation modes:

1. **Mode 1**: One-shot sequence (3-phase timing)
2. **Mode 2**: Monostable (retriggerable pulse)
3. **Mode 3**: Astable (free-running blink/toggle)
4. **Mode 4**: Input-triggered (responds to discrete inputs)

### Global Configuration

Before using timers, configure global status/control registers:

```bash
# Configure global timer status and control registers
set timers status-reg:140 control-reg:141
```

- **status-reg**: Modbus register showing all timer active states
- **control-reg**: Modbus register for global timer control

### Mode 1: One-Shot (3-Phase)

Executes a sequence of 3 phases with configurable output levels and timing.

#### Configuration

```bash
set timer 1 mode 1 parameter \
  coil:10 \
  P1:high P2:low P3:low \
  T1:1000 T2:500 T3:0
```

#### Parameters

| Parameter | Range | Description |
|-----------|-------|-------------|
| `coil` | 0-255 | Output coil index |
| `P1` | high/low | Phase 1 output level |
| `P2` | high/low | Phase 2 output level |
| `P3` | high/low | Phase 3 output level |
| `T1` | 1-60000 | Phase 1 duration (ms) |
| `T2` | 1-60000 | Phase 2 duration (ms) |
| `T3` | 0-60000 | Phase 3 duration (ms) - optional |

#### Timing Diagram

```
         T1=1000ms    T2=500ms      T3=0ms (skipped)
Coil 10: |------------|-------|
         High        Low    End
         (off=high)  (on=low)

Time:    0           1000   1500
```

#### Activation

```bash
# Start timer 1 (executes 3-phase sequence)
# After T1+T2+T3 complete, timer stops automatically
# Can be retriggered or stopped manually

# View status
show timers   # Timer 1 shows as "active"

# Stop timer manually
set timer 1 stop
```

### Mode 2: Monostable (Retriggerable Pulse)

Generates a single pulse that can be retriggered while active.

#### Configuration

```bash
set timer 2 mode 2 parameter \
  coil:11 \
  P1:high P2:low \
  T1:200 T2:1000
```

#### Parameters

| Parameter | Range | Description |
|-----------|-------|-------------|
| `coil` | 0-255 | Output coil index |
| `P1` | high/low | Active state output |
| `P2` | high/low | Inactive state output |
| `T1` | 1-60000 | Pulse duration (ms) |
| `T2` | 1-60000 | Minimum retrigger interval (ms) |

#### Behavior

```
First pulse:      T1 = 200ms
|------|

Retriggered:      T1 = 200ms (restarted from trigger)
|------|------|------|
```

#### Use Case

Watchdog timer: Reset on each heartbeat pulse, trigger alarm if no reset within T1 seconds.

### Mode 3: Astable (Free-Running Blink)

Continuous toggle between two states with independent on/off times.

#### Configuration

```bash
set timer 3 mode 3 parameter \
  coil:12 \
  P1:high P2:low \
  T1:500 T2:500
```

#### Parameters

| Parameter | Range | Description |
|-----------|-------|-------------|
| `coil` | 0-255 | Output coil index |
| `P1` | high/low | On state |
| `P2` | high/low | Off state |
| `T1` | 1-60000 | On duration (ms) |
| `T2` | 1-60000 | Off duration (ms) |

#### Timing Diagram

```
Coil:    |--T1--|--T2--|--T1--|--T2--|--T1--|
         High   Low   High   Low   High
         500ms  500ms  500ms  500ms ...
         (1 Hz toggle)
```

#### Use Cases

- Blinking indicator LED
- Periodic alarm signal
- Square wave oscillator
- Heartbeat indicator

### Mode 4: Input-Triggered

Timer responds to discrete input edges with configurable behavior.

#### Configuration

```bash
# Single-phase pulse on trigger
set timer 4 mode 4 parameter \
  coil:15 \
  P1:high P2:low \
  T1:200 \
  trigger:12 edge:rising sub:0

# Two-phase sequence on trigger
set timer 4 mode 4 parameter \
  coil:16 \
  P1:high P2:low \
  T1:200 T2:300 \
  trigger:13 edge:falling sub:1
```

#### Parameters

| Parameter | Range | Description |
|-----------|-------|-------------|
| `coil` | 0-255 | Output coil index |
| `P1` | high/low | Phase 1 state |
| `P2` | high/low | Phase 2 state |
| `T1` | 1-60000 | Phase 1 duration (ms) |
| `T2` | 1-60000 | Phase 2 duration (ms) - optional |
| `trigger` | 0-255 | Discrete input index to monitor |
| `edge` | rising/falling/both | Edge type to trigger on |
| `sub` | 0/1 | Sub-mode: 0=single, 1=dual phase |

#### Sub-Modes

**Sub-Mode 0**: Single-phase pulse
```
Trigger (rising):     |
Output T1=200ms:      |------|
```

**Sub-Mode 1**: Dual-phase sequence
```
Trigger (rising):     |
Output:               |------|------|
                      P1=200 P2=300
```

#### Use Cases

- Edge-triggered relay
- Pulse stretcher
- Signal shaper
- Input-dependent sequencer

---

## GPIO Management

### Overview

GPIO pin mapping system for:
- Mapping physical pins to Modbus discrete inputs
- Mapping physical pins to Modbus coils
- Configuring hardware counter pins
- Static configuration persistence

### Hardware Counter GPIO (DYNAMIC)

Hardware counters automatically map GPIO pins (read-only, DYNAMIC):

```bash
# View auto-generated mappings
show config

# Output:
# GPIO Mappings:
#   gpio 5 DYNAMIC at input 125 (counter1 HW-T1)
#   gpio 47 DYNAMIC at input 126 (counter2 HW-T3)
```

These mappings:
- **Cannot be changed** (hardware-determined)
- **Display as DYNAMIC** in configuration
- **Appear in GPIO section** of show config
- **Shown in show counters** pin column

### Manual GPIO Mapping

For software counters and general GPIO:

#### Syntax

```bash
# Map GPIO pin to discrete input
gpio map <pin> input <index>

# Map GPIO pin to coil
gpio map <pin> coil <index>

# Remove mapping
gpio unmap <pin>

# View all mappings
show gpio
```

#### Examples

```bash
# Map GPIO pin 22 to discrete input 20 (for SW counter 2)
gpio map 22 input 20

# Map GPIO pin 30 to coil 10
gpio map 30 coil 10

# Unmap pin
gpio unmap 22

# View all mappings
show gpio

# Output:
# GPIO Mappings:
#   gpio 22 at input 20
#   gpio 30 at coil 10
#   gpio 5 DYNAMIC at input 125 (counter1 HW-T1)
```

### Rules and Constraints

1. **One pin per mapping**: GPIO pin 22 can only map to one input or coil
2. **DYNAMIC mappings read-only**: Cannot manually override HW counter pins
3. **Input/Coil distinction**: Pin mapped to input cannot also map to coil
4. **Persistence**: All mappings saved in EEPROM (persistent across reboot)
5. **Pin validation**: Only valid Arduino Mega 2560 pins (2-53, except reserved)

### Reserved Pins

Cannot use these pins for GPIO mapping:
- **0, 1**: USB Serial (Debug CLI)
- **8**: RS-485 Direction control
- **13**: LED Heartbeat
- **18, 19**: Modbus RS-485 (Serial1)
- **5, 47, 6, 46**: Hardware counter inputs (if used in HW mode)

### GPIO Mapping Examples

#### Example 1: Software Counter with GPIO Input

```bash
# Configure software counter 2
set counter 2 mode 1 parameter \
  hw-mode:sw \
  count-on:rising \
  input-dis:20 \
  index-reg:110 freq-reg:118 \
  debounce:on debounce-ms:50

# Map GPIO pin 22 to discrete input 20
gpio map 22 input 20

# View configuration
show config
# Output includes:
# GPIO Mappings:
#   gpio 22 at input 20

# View counter status
show counters
# pin column shows "22"
```

#### Example 2: Multiple Manual Mappings

```bash
# Map input pins
gpio map 22 input 20
gpio map 23 input 21
gpio map 24 input 22

# Map output coil
gpio map 30 coil 10
gpio map 31 coil 11
gpio map 32 coil 12

# View all
show gpio
```

#### Example 3: Mixed HW/SW Setup

```bash
# Counter 1: Hardware on Timer1 (pin 5, DYNAMIC)
set counter 1 mode 1 parameter \
  hw-mode:hw-t1 count-on:rising \
  input-dis:125 freq-reg:108

# Counter 2: Software on GPIO 22
set counter 2 mode 1 parameter \
  hw-mode:sw count-on:rising \
  input-dis:20 freq-reg:118

# Manual GPIO mapping for SW counter
gpio map 22 input 20

# View configuration
show config
# GPIO section shows:
#   gpio 5 DYNAMIC at input 125 (counter1 HW-T1)
#   gpio 22 at input 20
```

---

## CLI Command Reference

### Command Syntax

- **Case-insensitive**: `SET COUNTER`, `set counter`, `Set Counter` all work
- **Space-delimited**: Parameters separated by spaces
- **Line continuation**: Use `\` to continue on next line (in scripts/macros)
- **Aliases**: Short versions available for common commands

### System Commands

#### show config
Display complete system configuration (registers, coils, timers, counters, GPIO).

```bash
show config

# Output includes:
# - Slave ID, Baudrate, Server status
# - Static registers and coils
# - Timer configuration
# - Counter configuration (HW/SW modes, registers, GPIO)
# - GPIO mappings (manual + DYNAMIC)
```

#### show version
Display firmware version and build information.

```bash
show version

# Output:
# Version: v3.3.0
# Build: 2025-11-11
# EEPROM Schema: v10
```

#### show status
Display runtime status (registers, coils, timers, counters active states).

```bash
show status

# Output includes:
# - Active timers
# - Counter values
# - Input/output status
```

#### set hostname <name>
Set CLI prompt hostname (max 15 characters).

```bash
set hostname mymodbusserver

# CLI prompt becomes:
# mymodbusserver>
```

#### set slaveid <id>
Set Modbus slave ID (1-247, or 0 for broadcast).

```bash
set slaveid 1
set slaveid 247
set slaveid 0    # Broadcast mode (responds to all)
```

**Note**: Requires reboot to take effect

#### set baudrate <baud>
Set Modbus RTU baudrate (9600, 19200, 38400, 57600, 115200, etc.).

```bash
set baudrate 9600
set baudrate 19200
set baudrate 38400
```

**Note**: Requires reboot to take effect

#### save
Save configuration to EEPROM.

```bash
save

# Output:
# OK: config saved to EEPROM
```

**CRITICAL**: Always `save` after configuration changes!

#### load
Load configuration from EEPROM (discards unsaved changes).

```bash
load

# Output:
# OK: config loaded from EEPROM
# (Current settings replaced with saved config)
```

#### defaults
Reset configuration to factory defaults (does not save automatically).

```bash
defaults

# Warning: Erases all configuration!
# Save manually: save
```

#### reboot
Restart the microcontroller (applies pending changes).

```bash
reboot

# System restarts...
# Reconnect serial monitor
```

#### status
Display microcontroller runtime status (memory, uptime, etc.).

```bash
status
```

### Counter Commands

#### show counters
Display all counter status with frequency measurement.

```bash
show counters

# Output table:
# id | enabled | hw  | val      | hz    | pin | ctrl-reg
# 1  | yes     | T1  | 12345    | 1000  | 5   | 130
# 2  | yes     | SW  | 9876     | 500   | 22  | 131
# 3  | no      | T4  | 0        | 0     | 6   | -
# 4  | no      | SW  | 0        | 0     | -   | -
```

#### set counter <id> mode 1 parameter ...
Configure counter with all parameters.

```bash
set counter <id> mode 1 parameter \
  hw-mode:<sw|hw-t1|hw-t3|hw-t4|hw-t5> \
  count-on:<rising|falling|both> \
  start-value:<n> \
  res:<8|16|32|64> \
  prescaler:<1-256> \
  index-reg:<0-159> \
  raw-reg:<0-159> \
  freq-reg:<0-159> \
  overload-reg:<0-159> \
  ctrl-reg:<0-159> \
  input-dis:<0-NUM_DISCRETE> \
  direction:<up|down> \
  scale:<float> \
  debounce:<on|off> \
  debounce-ms:<1-255>
```

**Minimum required parameters**:
- `hw-mode`, `count-on`, `res`, `prescaler`, `ctrl-reg`

**HW mode parameters** (hw-t1|hw-t3|hw-t4|hw-t5):
- Ignores: `debounce`, `debounce-ms`, `input-dis` (not needed)
- Must set: `hw-mode`, `index-reg` (minimum)

**SW mode parameters** (hw-mode:sw):
- Must set: `input-dis` (discrete input for GPIO mapping)
- Optional: `debounce`, `debounce-ms` (recommended)

#### set counter <id> start ENABLE|DISABLE
Enable/disable auto-start on boot.

```bash
# Enable auto-start (counter starts on reboot)
set counter 1 start enable

# Disable auto-start (manual start via control register)
set counter 1 start disable
```

#### set counter <id> reset-on-read ENABLE|DISABLE
Enable/disable auto-reset when counter read via Modbus.

```bash
# Enable: counter resets to start-value after Modbus read
set counter 1 reset-on-read enable

# Disable: counter value persists after read
set counter 1 reset-on-read disable
```

#### reset counter <id>
Manually reset counter to start-value.

```bash
reset counter 1
reset counter 2

# Counter value → start-value
# Frequency measurement → 0
```

#### clear counters
Reset all counters to start-value.

```bash
clear counters

# All 4 counters reset to start-value
```

### Timer Commands

#### show timers
Display all timer status.

```bash
show timers

# Output:
# id | mode | enabled | coil | T1/P1 | T2/P2 | T3/P3 | status
# 1  | 1    | yes     | 10   | 1000h | 500l  | 0     | active
# 2  | 3    | yes     | 11   | 500h  | 500l  | -     | active
# 3  | 4    | yes     | 15   | 200h  | -     | -     | inactive
# 4  | 0    | no      | -    | -     | -     | -     | -
```

#### set timer <id> mode <mode> parameter ...
Configure timer with mode and parameters.

```bash
# Mode 1: One-shot (3-phase)
set timer 1 mode 1 parameter \
  coil:10 P1:high P2:low P3:low T1:1000 T2:500 T3:0

# Mode 2: Monostable (retriggerable)
set timer 2 mode 2 parameter \
  coil:11 P1:high P2:low T1:200 T2:1000

# Mode 3: Astable (free-running blink)
set timer 3 mode 3 parameter \
  coil:12 P1:high P2:low T1:500 T2:500

# Mode 4: Input-triggered
set timer 4 mode 4 parameter \
  coil:15 P1:high P2:low T1:200 \
  trigger:12 edge:rising sub:0
```

#### set timers status-reg:<n> control-reg:<n>
Configure global timer status and control registers.

```bash
set timers status-reg:140 control-reg:141

# status-reg: Modbus register showing active timers
# control-reg: Modbus register for global control
```

#### set timer <id> start
Start timer (executes configured mode).

```bash
set timer 1 start

# Timer 1 begins execution according to configured mode
```

#### set timer <id> stop
Stop timer (halts execution, output to inactive state).

```bash
set timer 1 stop

# Timer 1 stops, output becomes P2 state
```

#### no set timer <id>
Remove/disable timer configuration.

```bash
no set timer 1

# Timer 1 disabled, configuration erased
```

### GPIO Commands

#### show gpio
Display all GPIO mappings (manual + DYNAMIC).

```bash
show gpio

# Output:
# GPIO Mappings:
#   gpio 5 DYNAMIC at input 125 (counter1 HW-T1)
#   gpio 22 at input 20
#   gpio 30 at coil 10
```

#### gpio map <pin> input <index>
Map GPIO pin to discrete input.

```bash
# Software counter requires GPIO to discrete input mapping
gpio map 22 input 20
gpio map 25 input 21

# View result
show config
# GPIO section now shows:
#   gpio 22 at input 20
#   gpio 25 at input 21
```

#### gpio map <pin> coil <index>
Map GPIO pin to coil (output).

```bash
gpio map 30 coil 10
gpio map 31 coil 11
```

#### gpio unmap <pin>
Remove GPIO mapping.

```bash
gpio unmap 22
gpio unmap 30

# Mappings deleted
```

### Modbus Register Commands

#### show regs [start] [count]
Display holding registers.

```bash
# Show all registers
show regs

# Show registers 100-109
show regs 100 10

# Show registers 110-119
show regs 110 10
```

#### show inputs
Display input registers.

```bash
show inputs

# Shows all input registers
```

#### show coils
Display coil states (packed bit array).

```bash
show coils

# Shows all coil states
```

#### write reg <addr> <value>
Write holding register (Modbus).

```bash
# Write counter control register (reset)
write reg 130 1    # Reset counter 1

# Write frequency target
write reg 100 5000 # Set to 5000 (application-specific)
```

#### write coil <idx> <0|1>
Write coil state.

```bash
# Set coil 10 to 1
write coil 10 1

# Set coil 10 to 0
write coil 10 0
```

#### read reg <addr>
Read holding register value.

```bash
read reg 100   # Read counter value
read reg 108   # Read frequency
read reg 130   # Read control register
```

#### read inputs
Read input register values (same as show inputs).

```bash
read inputs
```

---

## Modbus Register Reference

### Static Register Mapping

Holding registers 0-159 are available for application use. Below are common assignments used in examples:

### Counter Registers (Typical Assignment)

```
Counter 1:
  100: index-reg (scaled value)
  104: raw-reg (unscaled value)
  108: freq-reg (frequency Hz)
  120: overload-reg (overflow flag)
  130: ctrl-reg (control bits)

Counter 2:
  110: index-reg
  114: raw-reg
  118: freq-reg
  121: overload-reg
  131: ctrl-reg

Counter 3:
  100-109: (optional)
  132: ctrl-reg

Counter 4:
  110-119: (optional)
  133: ctrl-reg
```

### Timer Registers (Typical Assignment)

```
Timers:
  140: timerStatusReg (global status - all timers)
  141: timerStatusCtrlReg (global control - all timers)
```

### Control Register Bits

Any holding register used as ctrl-reg has these bit meanings:

```
Bit 0: Reset (W)            - Set to 1 to reset
Bit 1: Start (W)            - Set to 1 to enable
Bit 2: Stop (W)             - Set to 1 to disable
Bit 3: ResetOnRead (R/W)    - Auto-reset on Modbus read
Bits 4-15: Reserved         - Do not use
```

### Overflow Register Bits

Any holding register used as overload-reg:

```
Bit 0: Overflow Flag (R)    - Set when counter overflows
Bits 1-15: Reserved         - Do not use
```

### Timer Status Register Bits

Status register (default 140) shows active timers:

```
Bit 0: Timer 1 active
Bit 1: Timer 2 active
Bit 2: Timer 3 active
Bit 3: Timer 4 active
Bits 4-15: Reserved
```

### Discrete Inputs

Discrete inputs 0-255 (packed in bit arrays) represent:
- GPIO pin states (when mapped via `gpio map <pin> input <idx>`)
- Counter input indices (when used with counters)
- Timer trigger inputs (when used with mode 4)

### Coils

Coils 0-255 (packed in bit arrays) represent:
- GPIO pin outputs (when mapped via `gpio map <pin> coil <idx>`)
- Timer outputs (when configured)
- General digital outputs

---

## Practical Examples

### Example 1: Frequency Measurement (HW Mode)

**Scenario**: Measure 1 kHz signal using Timer1 (pin 5)

#### Configuration

```bash
# Enter CLI
CLI

# Configure counter 1 on Timer1
set counter 1 mode 1 parameter \
  hw-mode:hw-t1 \
  count-on:rising \
  start-value:0 \
  res:32 \
  prescaler:1 \
  index-reg:100 \
  raw-reg:104 \
  freq-reg:108 \
  ctrl-reg:130 \
  input-dis:125 \
  direction:up \
  scale:1.0

# Enable auto-start on boot
set counter 1 start enable

# View configuration
show config

# Expected output includes:
# GPIO Mappings:
#   gpio 5 DYNAMIC at input 125 (counter1 HW-T1)

# Save configuration
save

# View status
show counters
# pin column shows: 5
# hz column updates every second
```

#### Modbus Read

```bash
# Read frequency via Modbus FC04
read reg 108    # Frequency in Hz
read reg 100    # Counter value
```

#### Expected Results

```
Input Signal: 1000 Hz square wave on pin 5
After 1 second:
  index-reg (100): 1000
  raw-reg (104): 1000
  freq-reg (108): 1000 Hz
```

### Example 2: Software Counter with Debounce

**Scenario**: Count pulses on GPIO pin 22 with debounce filtering

#### Configuration

```bash
CLI

# Configure counter 2 in SOFTWARE mode
set counter 2 mode 1 parameter \
  hw-mode:sw \
  count-on:rising \
  start-value:0 \
  res:32 \
  prescaler:1 \
  index-reg:110 \
  raw-reg:114 \
  freq-reg:118 \
  ctrl-reg:131 \
  input-dis:20 \
  direction:up \
  scale:1.0 \
  debounce:on \
  debounce-ms:50

# Map GPIO pin 22 to discrete input 20
gpio map 22 input 20

# Enable auto-start
set counter 2 start enable

# Save
save

# View configuration
show config
# GPIO section shows:
#   gpio 22 at input 20

# View status
show counters
# pin column shows: 22
```

#### Operation

```
Input on pin 22: bouncy pulses (50ms debounce)
After debounce stable: counted

debounce-ms:50 means:
- Edge detected
- Wait 50ms
- If still high: count edge
- Continue detection
```

### Example 3: Multi-Counter Setup (Mixed HW/SW)

**Scenario**: 4 counters with different modes and registers

#### Configuration

```bash
CLI

# Counter 1: Hardware (Timer1, pin 5) - High precision
set counter 1 mode 1 parameter \
  hw-mode:hw-t1 count-on:rising res:32 \
  prescaler:1 index-reg:100 raw-reg:104 freq-reg:108 \
  ctrl-reg:130 input-dis:125 direction:up scale:1.0

# Counter 2: Hardware (Timer3, pin 47) - Parallel
set counter 2 mode 1 parameter \
  hw-mode:hw-t3 count-on:rising res:32 \
  prescaler:1 index-reg:110 raw-reg:114 freq-reg:118 \
  ctrl-reg:131 input-dis:126 direction:up scale:1.0

# Counter 3: Software (GPIO 22) - Variable location
set counter 3 mode 1 parameter \
  hw-mode:sw count-on:rising res:32 \
  prescaler:1 index-reg:120 raw-reg:124 freq-reg:128 \
  ctrl-reg:132 input-dis:20 direction:up scale:2.5 \
  debounce:on debounce-ms:50

# Counter 4: Software (GPIO 23) - Flow meter
set counter 4 mode 1 parameter \
  hw-mode:sw count-on:rising res:32 \
  prescaler:4 index-reg:130 raw-reg:134 freq-reg:138 \
  ctrl-reg:133 input-dis:21 direction:up scale:0.1 \
  debounce:on debounce-ms:30

# GPIO mappings for SW counters
gpio map 22 input 20
gpio map 23 input 21

# Enable auto-start for all
set counter 1 start enable
set counter 2 start enable
set counter 3 start enable
set counter 4 start enable

# Save
save

# View complete configuration
show config

# View status
show counters

# Expected output:
# id | hw  | index-reg | freq-reg | pin | hz
# 1  | T1  | 100       | 108      | 5   | 1000
# 2  | T3  | 110       | 118      | 47  | 2000
# 3  | SW  | 120       | 128      | 22  | 100
# 4  | SW  | 130       | 138      | 23  | 50
```

### Example 4: Timer with GPIO Output

**Scenario**: Blink LED on coil 10 with 1 Hz frequency

#### Configuration

```bash
CLI

# Map GPIO pin 30 to coil 10
gpio map 30 coil 10

# Configure Timer 1 in astable mode (blink)
set timer 1 mode 3 parameter \
  coil:10 \
  P1:high P2:low \
  T1:500 T2:500

# Set global timer registers
set timers status-reg:140 control-reg:141

# Start timer
set timer 1 start

# View status
show timers
# Timer 1 shows: mode=3, enabled=yes, status=active

# Save
save
```

#### Operation

```
Timer 1 astable mode:
- Every 500ms: coil 10 high (LED on)
- Every 500ms: coil 10 low (LED off)
- Repeats continuously (1 Hz blink)

GPIO mapping:
- GPIO pin 30 connected to coil 10
- Pin 30 toggles 1 Hz
```

---

## Troubleshooting

### Common Issues and Solutions

#### Issue: Counters not counting (HW mode)

**Symptom**: Counter value stays 0, frequency shows 0 Hz

**Causes**:
1. No input signal on timer pin (5, 47, 6, or 46)
2. Counter not started (auto-start:disable)
3. Signal frequency too low (< 1 Hz may not detect)

**Solutions**:
```bash
# Check configuration
show config     # Verify hw-mode and registers

# Check status
show counters   # Verify enabled=yes

# Check if counter started
read reg 130    # ctrl-reg should show bit 1 set (start=1)

# Manually start if needed
write reg 130 2 # Set bit 1 (start)

# Reset counter
write reg 130 1 # Set bit 0 (reset)

# Monitor for changes
show counters   # Repeat every second
```

**Verification**:
- Connect oscilloscope to timer pin (5, 47, 6, 46)
- Verify signal present and edges detected
- Check signal frequency (should be 1-20000 Hz for frequency measurement)

#### Issue: Counters not counting (SW mode)

**Symptom**: Software counter at 0, no increment even with GPIO input

**Causes**:
1. GPIO pin not mapped to discrete input
2. Wrong discrete input index used
3. Debounce filter too aggressive (50ms+ on 10Hz signal)
4. Counter not started

**Solutions**:
```bash
# Verify GPIO mapping
show gpio       # Should show: gpio 22 at input 20

# Verify counter configuration
show config     # Check: input-dis matches gpio mapping

# Check debounce setting
# For low-frequency signals, reduce debounce-ms
set counter 2 mode 1 parameter \
  hw-mode:sw ... debounce:on debounce-ms:10

# Manually check GPIO state
show inputs     # Check discrete input 20 state

# Manually start counter
write reg 131 2 # ctrl-reg bit 1

# Monitor
show counters   # Watch value increment
```

**Verification**:
- Use `show inputs` to verify discrete input state changes
- Slow down input signal to visible frequency (< 1 Hz) to manually verify

#### Issue: Frequency measurement shows wrong value

**Symptom**: freq-reg shows unexpected Hz value (too high, too low, or 0)

**Causes**:
1. Prescaler incorrect (if set != 1)
2. Frequency too high (> 20000 Hz gets clamped)
3. Frequency too low (< 1 Hz may not detect)
4. Timing window not yet stable (< 1 second)
5. Prescaler not accounted for in calculation

**Solutions**:
```bash
# Wait full second for measurement
# Frequency updates every 1 second, shows 0 for first second

# Check prescaler in configuration
show config     # Look for prescaler value

# If using prescaler, Hz calculation:
# Hz = (measured / prescaler) * edge_multiplier
# edge_multiplier = 1 for single-edge, 2 for both

# Example: prescaler:4, signal=4000 Hz
# count-on:rising → 4000 / 4 = 1000 Hz
# count-on:both → 4000 * 2 / 4 = 2000 Hz

# Reset counter to restart frequency measurement
write reg 130 1 # ctrl-reg bit 0 (reset)

# Monitor frequency update
read reg 108    # Frequency register (wait 1+ second)
```

**Frequency Limits**:
- **Minimum**: 1 Hz (need at least 1-2 count delta per second)
- **Maximum**: 20000 Hz (clamped in hardware/software)
- **Resolution**: Depends on measurement window (1-2 seconds)

#### Issue: SAVE command failed or caused reboot

**Symptom**: System restarts when executing `save` command

**Causes**:
1. **Stack overflow** (previous versions, fixed in v3.3.0)
2. **Corrupted EEPROM** (bad chip or power loss during write)
3. **Invalid configuration** causing CRC mismatch

**Solutions**:
```bash
# Reset to factory defaults
defaults

# Save defaults
save

# Verify save succeeded
# Should see: "OK: config saved to EEPROM"

# If persistent failures:
# Load from EEPROM (discards current changes)
load

# If EEPROM is corrupted:
# 1. Backup current config to Modbus registers manually
# 2. Execute: defaults
# 3. Save
# 4. Reconfigure via CLI (or Modbus write)
```

#### Issue: Timer not executing or not triggered

**Symptom**: Timer shows enabled but no action (coil unchanged, output not triggered)

**Causes**:
1. Timer stopped (check control register)
2. Output coil index invalid
3. Input-triggered timer no edge on trigger input
4. Global status/control registers not set

**Solutions**:
```bash
# Check timer status
show timers     # Verify: enabled=yes, status=active

# Check control register (global)
read reg 141    # timerStatusCtrlReg

# Manually start timer
# Bit 0 of control-reg starts timer
write reg 141 1 # Start timer

# For input-triggered timers (mode 4):
# Verify trigger discrete input has edge
show inputs     # Check trigger input state

# Manually trigger by writing to discrete input
# (via Modbus or GPIO mapping)

# If still not working, reconfigure:
set timer 1 mode 3 parameter \
  coil:10 P1:high P2:low T1:500 T2:500

set timers status-reg:140 control-reg:141

set timer 1 start

show timers     # Verify active
```

#### Issue: GPIO mapping rejected or not appearing

**Symptom**: `gpio map` command appears to work but mapping doesn't appear in `show config`

**Causes**:
1. Mapping overwritten by HW counter (DYNAMIC)
2. GPIO pin reserved (0, 1, 8, 13, 18, 19, etc.)
3. Mapping to invalid discrete input index

**Solutions**:
```bash
# Check reserved pins
# Cannot map: 0, 1, 8, 13, 18, 19
# Cannot map if used by HW counter: 5, 47, 6, 46

# Use different GPIO pin
gpio map 22 input 20    # OK
gpio map 25 input 21    # OK
gpio map 30 coil 10     # OK

# Verify mapping added
show gpio       # Should show new mapping

# If mapping disappears after save:
# May have been overwritten by HW counter
# Check show config GPIO section for DYNAMIC entries

# Unmap conflicting entry
gpio unmap 5    # Error (DYNAMIC, read-only)
```

#### Issue: Configuration lost after reboot

**Symptom**: Settings revert to defaults after power cycle

**Causes**:
1. Configuration not saved (`save` command not executed)
2. EEPROM save failed (corruption, bad chip)
3. Configuration loaded but wrong version (EEPROM format change)

**Solutions**:
```bash
# After making configuration changes, ALWAYS:
save            # Explicitly save to EEPROM

# Verify save succeeded
# Should see: "OK: config saved to EEPROM"

# Before reboot, verify:
show config     # Confirm settings correct
save            # Save again (harmless if already saved)

# If still lost after reboot:
# Load saved config (discards current)
load

# If EEPROM corrupted:
defaults
save

# Reconfigure manually or via Modbus
```

#### Issue: Very high RAM usage (82%+)

**Symptom**: System stability issues, slow response

**Causes**:
1. Large static allocations in RAM
2. Stack allocation of large structs (PersistConfig >1KB)
3. Multiple counter/timer instances with heavy memory

**Current Status**: v3.3.0 optimized to 82.4% (acceptable)

**If upgrading features**:
- Use global allocation instead of stack for large structs
- Minimize static buffers
- Consider alternative data structures

---

## Version History

### v3.3.0 (2025-11-11) - Hardware Counter Engine

**New Features**:
- **Hardware Counter Mode**: Direct timer input capture (HW T1/T3/T4/T5)
- **Software Fallback**: GPIO pin-based edge detection
- **Automatic GPIO Management**: DYNAMIC mappings for HW counters
- **Improved CLI Display**:
  - Separate discrete input vs GPIO pin columns
  - DYNAMIC GPIO mappings in config section
  - Corrected pin column in `show counters`
- **Fixed SAVE Command**: Global config allocation prevents stack overflow

**Changes**:
- EEPROM schema updated to v10 (added hw-mode field)
- CounterConfig struct extended with hardware mode
- CLI parser updated with hw-mode parameter
- configSave() optimized for RAM efficiency
- Global config allocation in main.cpp

**Bug Fixes**:
- Fixed stack overflow in SAVE command (global allocation)
- Fixed discrete input display (showing GPIO pin instead)
- Fixed missing DYNAMIC GPIO mappings in config display
- Fixed pin column in show counters (now shows actual GPIO pins)

**Performance**:
- RAM: 82.4% (6751 / 8192 bytes)
- Flash: 21.2% (53956 / 253952 bytes)
- Backward compatible with v3.2.0 configurations

**Breaking Changes**: None (backward compatible)

---

### v3.2.0 (2025-11-10) - Frequency Measurement

**New Features**:
- **Frequency Measurement**: Automatic Hz measurement (0-20000 Hz)
- **Raw Register**: Separate unscaled value register
- **Consistent Naming**: Updated parameter names (index-reg, raw-reg, freq-reg)
- **CLI Enhancements**: 256-char command buffer, improved help

**Changes**:
- Added freq-reg parameter for frequency measurement
- Added raw-reg parameter for unscaled values
- Counter loop implements frequency algorithm
- EEPROM schema updated to v9

**Performance**:
- RAM: 56.8% (4655 / 8192 bytes)
- Flash: 20.1% (51058 / 253952 bytes)

---

### v3.1.9 (2025-11-09) - Counter Control

**Features**:
- Individual reset-on-read enable per counter
- Individual auto-start enable per counter
- Improved counter display (`show counters`)
- CLI enhancements

---

### v3.1.7 (2025-11-08) - Counter Features

**Features**:
- Raw counter value output
- Reset-on-read functionality
- Auto-start on boot configuration

---

### v3.1.4 (2025-05) - Counter Engine v3

**Features**:
- Counter prescaler (1-256)
- Direction control (up/down)
- Float scale factor
- Debounce filtering
- EEPROM persistence

---

## Support and Resources

### Quick Reference

**CLI Entry**: Type `CLI` at serial monitor (115200 baud)

**Help System**: Type `?` or `help` in CLI for command list

**Configuration View**: `show config` displays complete system state

**Counter Status**: `show counters` shows all counters with frequency

**Save Before Reboot**: Always `save` before `reboot`

### Key Files

| File | Purpose |
|------|---------|
| include/modbus_core.h | Constants, structures, prototypes |
| include/modbus_counters.h | Counter configuration struct |
| src/modbus_counters.cpp | Counter engine implementation |
| src/cli_shell.cpp | CLI command parser |
| src/config_store.cpp | EEPROM persistence |

### External Resources

- [Arduino Mega 2560 Datasheet](https://ww1.microchip.com/uploads/datasheets/microchip/microcontrollers/avr/uc3/uc3a3256.pdf)
- [Modbus RTU Specification](https://modbus.org/specs.php)
- [PlatformIO Documentation](https://docs.platformio.org/)

---

**Document Version**: 1.0
**Last Updated**: 2025-11-11
**Status**: Complete and Ready for Production
