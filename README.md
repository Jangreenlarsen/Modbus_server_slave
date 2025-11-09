# Modbus RTU Server v3.1.7 - PlatformIO Edition

Arduino Mega 2560 Modbus RTU Server med CLI, Timer Engine og Counter Engine.

## Projektstruktur

```
ModbusGateway/
├── platformio.ini          # PlatformIO konfiguration
├── src/
│   └── main.cpp           # Hovedprogram
├── include/               # Header filer
│   ├── version.h
│   ├── modbus_core.h
│   ├── modbus_globals.h
│   ├── modbus_timers.h
│   └── modbus_counters.h
└── lib/                   # Custom libraries (tom indtil videre)
```

## Installation

### 1. Åbn projektet i VS Code
```bash
cd /sti/til/ModbusGateway
code .
```

### 2. Build projektet
- Tryk på ✓ (checkmark) nederst i VS Code, ELLER
- Tryk `Ctrl+Alt+B`

### 3. Upload til Arduino Mega
- Tilslut Arduino Mega via USB
- Tryk på → (arrow) nederst i VS Code, ELLER
- Tryk `Ctrl+Alt+U`

### 4. Åbn Serial Monitor
- Tryk på 🔌 (plug) nederst i VS Code, ELLER
- Tryk `Ctrl+Alt+S`

## Hardware Konfiguration

- **Board**: Arduino Mega 2560
- **Modbus Serial**: Serial1 (TX1/RX1, pins 18/19)
- **RS485 Direction**: Pin 8
- **Debug Serial**: Serial (USB, 115200 baud)
- **LED**: Pin 13 (heartbeat)

## Funktioner

### Modbus RTU Server
- Slave ID: Konfigurerbart (default: 10)
- Baudrate: Konfigurerbart (default: 9600)
- RS485 med automatisk direction control
- Understøtter FC01-FC06, FC15-FC16

### Timer Engine
- Op til 4 uafhængige timere
- Modes: One-shot, Monostable, Astable, Trigger
- Coil-styring med alarm og timeout

### Counter Engine v3
- Op til 4 uafhængige input-tællere
- 8/16/32/64-bit precision
- Direction: Up/Down
- Float scaling faktor
- Prescaler og debounce
- Overflow detection

### CLI (Command Line Interface)
- Aruba/Cisco-style kommandoer
- Enter CLI med: `CLI`
- Konfiguration af alle parametre
- EEPROM persistence
- Show status, timers, counters

## CLI Kommandoer (Eksempler)

```
CLI                              # Enter CLI mode
show config                      # Vis konfiguration
show timers                      # Vis timer status
show counters                    # Vis counter status
set slave-id 10                  # Sæt Modbus slave ID
set baudrate 9600                # Sæt baudrate
server start                     # Start Modbus server
server stop                      # Stop Modbus server
save                             # Gem til EEPROM
exit                             # Forlad CLI
```

## Fejlfinding

### Kan ikke uploade
- Tjek at Arduino Mega er tilsluttet
- Tjek at du har valgt den rigtige USB port
- Luk Serial Monitor før upload

### Serial Monitor viser ingenting
- Tjek baudrate er sat til 115200
- Tjek at board er startet (tryk reset-knap)

### Kompileringsfejl
Projektet mangler implementeringsfilerne (.cpp). Du skal tilføje:
- modbus_core.cpp
- modbus_globals.cpp
- modbus_timers.cpp
- modbus_counters.cpp
- modbus_utils.cpp
- cli.cpp
- eeprom_config.cpp

Placer disse filer i `src/` mappen.

## Næste Skridt

1. **Tilføj .cpp implementeringsfiler** - Projektet kompilerer ikke uden disse
2. **Test Modbus kommunikation** - Brug Modbus Master software
3. **Konfigurer timere/counters** - Via CLI
4. **Tilpas hardware pins** - Efter behov i modbus_core.h

## Version Info

- **Version**: v3.1.7
- **Build**: 20251108
- **EEPROM Schema**: v5
- **Platform**: Arduino Mega 2560
- **Framework**: Arduino + PlatformIO

## Support

Se header-filerne for detaljeret dokumentation af hver modul.
