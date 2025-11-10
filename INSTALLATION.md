# Installation Guide - Modbus RTU Server v3.2.0

Komplet guide til installation og opsætning af Modbus RTU Server på Arduino Mega 2560.

---

## 📋 Forudsætninger

### Software
- [Visual Studio Code](https://code.visualstudio.com/)
- [PlatformIO Extension](https://platformio.org/install/ide?install=vscode)
- Git (valgfrit, til cloning)

### Hardware
- Arduino Mega 2560
- USB kabel (Type A til Type B)
- RS-485 transceiver modul (f.eks. MAX485)
- Breadboard og jumper wires (til test)

---

## 🚀 Installation (Hurtig Start)

### Metode 1: Git Clone (Anbefalet)

```bash
# Clone repository
git clone https://github.com/Jangreenlarsen/Modbus_server_slave.git

# Gå ind i projektmappen
cd Modbus_server_slave

# Åbn i VS Code
code .
```

### Metode 2: Download ZIP

1. Gå til [GitHub Repository](https://github.com/Jangreenlarsen/Modbus_server_slave)
2. Klik **Code → Download ZIP**
3. Udpak ZIP filen
4. Åbn mappen i VS Code: **File → Open Folder**

---

## 📦 Projekt Setup

### Trin 1: Åbn projektet i VS Code

1. Start **Visual Studio Code**
2. Klik **File → Open Folder**
3. Vælg projektmappen (den der indeholder `platformio.ini`)
4. VS Code genkenner automatisk PlatformIO projektet

### Trin 2: Vent på PlatformIO initialisering

**Første gang du åbner projektet:**
- PlatformIO scanner automatisk `platformio.ini`
- Downloader nødvendige tools og libraries
- Kompilerer toolchain (avr-gcc, etc.)
- Dette kan tage **2-5 minutter** første gang
- Se status nederst i VS Code statusbar

**Status indikatorer:**
```
PlatformIO: Loading...          (downloading)
PlatformIO: Ready               (klar til build)
```

### Trin 3: Verificer projektstruktur

Projektet er **komplet** og klar til build:

```
Modbus_server_slave/
├── platformio.ini              # PlatformIO konfiguration
├── README.md                   # Projekt dokumentation
├── src/                        # Source filer (alle inkluderet ✅)
│   ├── main.cpp
│   ├── cli_shell.cpp
│   ├── config_store.cpp
│   ├── modbus_counters.cpp
│   ├── modbus_fc.cpp
│   ├── modbus_globals.cpp
│   ├── modbus_timers.cpp
│   ├── modbus_tx.cpp
│   ├── modbus_utils.cpp
│   └── status_info.cpp
├── include/                    # Header filer
│   ├── version.h               # v3.2.0
│   ├── modbus_core.h
│   ├── modbus_counters.h
│   ├── modbus_globals.h
│   └── modbus_timers.h
└── lib/                        # Libraries (ingen eksterne)
```

**✅ Projektet er komplet - ingen manglende filer!**

---

## 🔨 Build & Upload

### Trin 4: Build projektet

**Via VS Code toolbar (nemmest):**
1. Klik på **✓** (checkmark) nederst i VS Code
2. Vent mens projektet bygges (~10-30 sekunder)
3. Se output i Terminal vinduet

**Via keyboard shortcut:**
- `Ctrl+Alt+B` (Windows/Linux)
- `Cmd+Alt+B` (macOS)

**Via Terminal:**
```bash
pio run
```

**Forventet output:**
```
Building in release mode
Compiling .pio\build\megaatmega2560\src\main.cpp.o
...
Linking .pio\build\megaatmega2560\firmware.elf
RAM:   [======    ]  56.8% (used 4655 bytes from 8192 bytes)
Flash: [==        ]  20.1% (used 51058 bytes from 253952 bytes)
========================= [SUCCESS] =========================
```

### Trin 5: Tilslut Arduino Mega

1. **Tilslut Arduino Mega til USB**
2. Windows finder automatisk den rigtige driver
3. PlatformIO vælger automatisk den korrekte port

**Verificer port (valgfrit):**
```bash
pio device list
```

Output viser noget lignende:
```
/dev/ttyUSB0
Hardware ID: USB VID:PID=2341:0042
Description: Arduino Mega 2560
```

### Trin 6: Upload firmware

**Via VS Code toolbar (nemmest):**
1. Klik på **→** (arrow) nederst i VS Code
2. Vent mens koden uploades (~30 sekunder)

**Via keyboard shortcut:**
- `Ctrl+Alt+U` (Windows/Linux)
- `Cmd+Alt+U` (macOS)

**Via Terminal:**
```bash
pio run -t upload
```

**Forventet output:**
```
Uploading .pio\build\megaatmega2560\firmware.hex
avrdude: 51058 bytes of flash written
avrdude: verifying ...
avrdude: 51058 bytes of flash verified
========================= [SUCCESS] =========================
```

### Trin 7: Åbn Serial Monitor

**Via VS Code toolbar:**
1. Klik på **🔌** (plug) nederst i VS Code

**Via keyboard shortcut:**
- `Ctrl+Alt+S` (Windows/Linux)
- `Cmd+Alt+S` (macOS)

**Via Terminal:**
```bash
pio device monitor
```

**Serial Monitor indstillinger:**
- **Baudrate:** 115200
- **Line ending:** Any (NL, CR, eller Both)

---

## ✅ Verificer Installation

### Forventede Startup-beskeder

Du skulle nu se startup-beskeder i Serial Monitor:

```
=== MODBUS RTU SERVER v3.2.0 ===
Build: 20251110
Platform: Arduino Mega 2560
RAM: 8KB (56.8% used)
Flash: 256KB (20.1% used)
===============================================
⚠ EEPROM schema !=9 – resetting to defaults
✓ Config defaults applied
✓ Modbus core initialized
✓ ID: 10  Baud: 9600
✓ Server: RUNNING

Enter CLI by typing: CLI
Line ending: NL or CR or Both, 115200 baud
===============================================
```

### Test CLI

1. Skriv `CLI` i Serial Monitor og tryk Enter
2. Du skulle nu se CLI prompt: `rgh-modbus#`
3. Test en kommando: `show version`

**Forventet output:**
```
Version: v3.2.0
Build: 20251110
```

### Test Counter med Frequency Measurement

```bash
# Konfigurer counter 1 med frekvens-måling
set counter 1 mode 1 parameter count-on:rising start-value:0 res:32 prescaler:1 \
  index-reg:100 raw-reg:104 freq-reg:108 \
  overload-reg:120 ctrl-reg:130 input-dis:20 \
  direction:up scale:1.0 debounce:on debounce-ms:50

# Enable auto-start
set counter 1 start enable

# Save configuration
save

# Show status
show counters
```

---

## 🔧 Hardware Opsætning

### RS-485 Wiring

Tilslut RS-485 transceiver modul til Arduino Mega:

| Arduino Mega Pin | RS-485 Modul | Beskrivelse |
|------------------|--------------|-------------|
| 18 (TX1) | DI | Data transmit |
| 19 (RX1) | RO | Data receive |
| 8 | DE + RE | Direction enable |
| 5V | VCC | Power supply |
| GND | GND | Ground |

**RS-485 Bus:**
| RS-485 Modul | Bus Terminal |
|--------------|--------------|
| A | A/D+ |
| B | B/D- |
| GND | GND (hvis galvanisk isolation ikke bruges) |

### Test Inputs/Outputs

**GPIO til discrete input (test):**
```bash
gpio map 22 input 20
save
show gpio
```

**GPIO til coil output (test):**
```bash
gpio map 23 coil 10
save
show gpio
```

---

## 🐛 Fejlfinding

### Build Fejler

#### "PlatformIO: Command not found"
**Problem:** PlatformIO extension ikke installeret korrekt

**Løsning:**
1. Genstart VS Code
2. Installer PlatformIO extension igen
3. Vent på at extension initialiserer

#### "undefined reference to..." fejl
**Problem:** Sjælden linker fejl (alle filer er inkluderet)

**Løsning:**
```bash
# Clean build
pio run -t clean

# Rebuild
pio run
```

### Upload Fejler

#### "Permission denied" eller "Access denied"
**Problem:** Serial Monitor blokerer porten

**Løsning:**
1. Luk Serial Monitor først
2. Upload igen
3. Åbn Serial Monitor efter upload

#### "Device not found"
**Problem:** Arduino ikke tilsluttet eller driver mangler

**Løsning:**
1. Tjek USB forbindelse
2. Tjek Device Manager (Windows) - skal vise "Arduino Mega 2560"
3. Installer Arduino drivers hvis nødvendigt

### Serial Monitor Problemer

#### "Ingen output"
**Løsning:**
1. Check baudrate er **115200**
2. Tryk reset-knap på Arduino
3. Genstart Serial Monitor

#### "Ugyldige tegn / garbage"
**Problem:** Forkert baudrate

**Løsning:**
1. Sæt baudrate til **115200** i Serial Monitor
2. Genstart connection

### Runtime Problemer

#### "Counter tæller ikke"
**Tjek:**
1. Er counter startet? `show counters` - se "running" kolonne
2. Er input mappet korrekt? `show gpio`
3. Er signal tilsluttet til korrekt pin?

#### "Frequency viser 0 Hz"
**Tjek:**
1. Er freq-reg konfigureret? `show config`
2. Er counter running?
3. Er der faktisk pulser på input?
4. Er prescaler sat korrekt? (høj prescaler = færre tællinger = lavere frekvens)

---

## 📚 Næste Skridt

### 1. Grundlæggende Konfiguration

```bash
# Enter CLI
CLI

# Vis aktuel konfiguration
show config

# Sæt Modbus slave ID
set id 10

# Sæt baudrate
set baud 9600

# Sæt hostname
set hostname my-modbus-gateway

# Gem konfiguration
save
```

### 2. Konfigurer Counter med v3.2.0 Features

```bash
# Counter med frequency measurement
set counter 1 mode 1 parameter \
  count-on:rising start-value:0 res:32 prescaler:1 \
  index-reg:100 raw-reg:104 freq-reg:108 \
  overload-reg:120 ctrl-reg:130 input-dis:20 \
  direction:up scale:1.0

set counter 1 start enable
save
```

### 3. Konfigurer Timer

```bash
# Astable blink timer
set timer 1 mode 3 parameter \
  coil:10 P1:high P2:low T1 500 T2 500

save
show timers
```

### 4. Test Modbus Kommunikation

Brug Modbus Master software (f.eks. QModMaster, ModScan) til at teste:

**Læs holding registers:**
- Function Code: 03 (Read Holding Registers)
- Address: 100 (scaled counter value)
- Quantity: 1

**Læs frequency:**
- Function Code: 03
- Address: 108 (frequency in Hz)
- Quantity: 1

**Start counter via Modbus:**
- Function Code: 06 (Write Single Register)
- Address: 130 (control register)
- Value: 0x0002 (bit1 = start)

---

## 📖 Yderligere Dokumentation

- **[README.md](README.md)** - Projekt oversigt og quick start
- **[Modbus server V3.2.0 Manual](Modbus%20server%20V3.2.0%20Manual%20-%20counter%20adv%20mode.html)** - Komplet manual (dansk)
- **[STATUS.md](STATUS.md)** - Build status og features
- **[TODO.md](TODO.md)** - Roadmap for fremtidige features

### CLI Hjælp

```bash
# Vis alle kommandoer
help

# Vis version
show version

# Vis fuld konfiguration
show config

# Vis counters med frekvens
show counters

# Vis timers
show timers
```

---

## 🎉 Installation Komplet!

Din Modbus RTU Server v3.2.0 er nu installeret og klar til brug.

**Status:** ✅ **PRODUCTION READY**

**Næste skridt:**
1. Tilslut RS-485 hardware
2. Konfigurer counters og timers efter behov
3. Test Modbus kommunikation
4. Deploy i produktion

---

**Support:**
- **GitHub Issues:** [Report problems](https://github.com/Jangreenlarsen/Modbus_server_slave/issues)
- **Latest Release:** [v3.2.0](https://github.com/Jangreenlarsen/Modbus_server_slave/releases/tag/v3.2.0)

*Installation guide opdateret: 2025-11-10 | v3.2.0*
