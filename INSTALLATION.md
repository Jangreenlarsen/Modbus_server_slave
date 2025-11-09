# Installation Guide - Modbus RTU Server

## Trin 1: Download projektet

Download hele projektmappen til din computer.

## Trin 2: Åbn i VS Code

1. Start Visual Studio Code
2. Klik **File → Open Folder**
3. Vælg projektmappen (den der indeholder `platformio.ini`)

## Trin 3: Vent på PlatformIO

Første gang du åbner projektet:
- PlatformIO scanner automatisk `platformio.ini`
- Den downloader nødvendige tools og libraries
- Dette kan tage 1-5 minutter første gang
- Se status i bunden af VS Code

## Trin 4: Tilføj manglende .cpp filer

**VIGTIGT**: Projektet mangler implementeringsfilerne!

Du skal tilføje disse filer i `src/` mappen:

```
src/
├── main.cpp                 ✓ (allerede tilføjet)
├── modbus_core.cpp          ✗ (mangler)
├── modbus_globals.cpp       ✗ (mangler)
├── modbus_timers.cpp        ✗ (mangler)
├── modbus_counters.cpp      ✗ (mangler)
├── modbus_utils.cpp         ✗ (mangler)
├── cli.cpp                  ✗ (mangler)
└── eeprom_config.cpp        ✗ (mangler)
```

**Kopiér disse fra dit originale Arduino projekt til `src/` mappen.**

## Trin 5: Build projektet

Når alle .cpp filer er på plads:

1. Tryk på **✓** (checkmark) nederst i VS Code
2. Eller tryk `Ctrl+Alt+B`
3. Vent mens projektet bygges
4. Check for fejl i Terminal vinduet

## Trin 6: Tilslut Arduino Mega

1. Tilslut Arduino Mega til USB
2. Windows finder automatisk den rigtige port
3. PlatformIO vælger automatisk porten

## Trin 7: Upload

1. Tryk på **→** (arrow) nederst i VS Code
2. Eller tryk `Ctrl+Alt+U`
3. Vent mens koden uploades

## Trin 8: Åbn Serial Monitor

1. Tryk på **🔌** (plug) nederst i VS Code
2. Eller tryk `Ctrl+Alt+S`
3. Du skulle nu se startup-beskeder

## Forventede Startup-beskeder

```
=== MODBUS RTU SLAVE ===
Version: v3.1.7
Build: 20251108
===============================================
% No valid config in EEPROM -> using defaults
% Modbus core initialized
% ID: 10  Baud: 9600
% Server: RUNNING

% Enter CLI by typing: CLI
% Line ending: NL or CR or Both, 115200 baud
===============================================
```

## Fejlfinding

### "Cannot open include file" fejl
- Du mangler .cpp implementeringsfiler
- Kopier dem fra dit Arduino projekt til `src/`

### "Undefined reference" fejl  
- Funktioner er deklareret i .h men ikke implementeret
- Check at alle .cpp filer er tilstede

### Kan ikke se Serial Monitor output
- Check at baudrate er 115200
- Tryk reset-knap på Arduino
- Check at Serial Monitor er åben

### Upload fejler
- Luk Serial Monitor før upload
- Check USB forbindelse
- Check at Arduino Mega er valgt i platformio.ini

## Næste Skridt

Efter succesfuld installation:

1. **Test CLI** - Skriv `CLI` i Serial Monitor
2. **Check konfiguration** - Kommando: `show config`
3. **Test Modbus** - Brug Modbus Master software
4. **Tilpas indstillinger** - Via CLI kommandoer

## Hjælp

Hvis du støder på problemer:
1. Check at alle .cpp filer er kopieret
2. Check at platformio.ini peger på rigtig board
3. Se efter fejlbeskeder i Terminal output
4. Genstart VS Code hvis PlatformIO hænger
