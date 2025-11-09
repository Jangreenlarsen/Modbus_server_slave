# TODO Checklist - Modbus RTU Server Migration

## ✅ Færdige Opgaver

- [x] PlatformIO projektstruktur oprettet
- [x] platformio.ini konfigureret til Arduino Mega 2560
- [x] main.cpp konverteret fra .ino format
- [x] Alle header filer (.h) kopieret til include/
- [x] README og dokumentation oprettet
- [x] .gitignore tilføjet
- [x] Installation guide skrevet

## ❌ Manglende Filer (KRITISK)

Du skal tilføje disse implementeringsfiler i `src/` mappen:

- [ ] **modbus_core.cpp**
  - Indeholder: initModbus(), processModbusFrame(), modbusLoop()
  - CRC16 beregning
  - RS485 direction control
  - Modbus function code handlers (FC01-FC06, FC15-FC16)

- [ ] **modbus_globals.cpp**
  - Implementering af globale arrays og variabler
  - coils[], discreteInputs[], holdingRegs[], inputRegs[]
  - Statistik variabler

- [ ] **modbus_utils.cpp**
  - Utility funktioner: bitReadArray(), bitWriteArray(), packBits()
  - calculateCRC16()
  - rtuGapUs()
  - printHex()

- [ ] **modbus_timers.cpp**
  - TimerEngine implementering
  - timers_init(), timers_loop()
  - Timer mode logik (One-shot, Monostable, Astable, Trigger)
  - timers_config_set(), timers_get()

- [ ] **modbus_counters.cpp**
  - CounterEngine v3 implementering
  - counters_init(), counters_loop()
  - Edge detection med debounce
  - Overflow/underflow håndtering
  - Scaling og direction

- [ ] **cli.cpp**
  - CLI (Command Line Interface) implementering
  - Kommando parsing
  - Show/set kommandoer
  - Help system

- [ ] **eeprom_config.cpp**
  - EEPROM persistence
  - configLoad(), configSave(), configDefaults()
  - configApply()
  - CRC checksum

## 🔧 Næste Skridt

### 1. Kopier .cpp filer (FØRST)
```bash
# Fra dit Arduino projekt til PlatformIO:
cp /path/to/arduino/projekt/*.cpp /path/to/platformio/src/
```

### 2. Test build
```bash
pio run
```

### 3. Fix eventuelle fejl
- Check #include paths
- Check function declarations
- Check missing dependencies

### 4. Test upload
```bash
pio run -t upload
```

### 5. Test Serial Monitor
```bash
pio device monitor
```

## 📋 Valideringsliste

Efter tilføjelse af .cpp filer, test:

- [ ] Projekt kompilerer uden fejl
- [ ] Upload virker til Arduino Mega
- [ ] Serial Monitor viser startup-beskeder
- [ ] LED blinker (heartbeat)
- [ ] CLI kan aktiveres med "CLI" kommando
- [ ] Modbus server starter
- [ ] EEPROM config loader/gemmes korrekt

## 🐛 Kendte Problemer at Holde Øje Med

1. **Serial1 vs Serial**
   - Serial = USB debug (115200)
   - Serial1 = Modbus RTU (9600)
   - Check at disse ikke blandes sammen

2. **F() Macro**
   - Virker fint i PlatformIO
   - Bruges til at spare RAM

3. **EEPROM Library**
   - Arduino Mega bruger EEPROM.h
   - Ingen ændringer nødvendige

4. **Timing**
   - millis() og micros() virker ens
   - RTU gap timing skal valideres

## 💡 Optimeringer (Senere)

- [ ] Tilføj unit tests i test/ mappe
- [ ] Implementer OTA (Over-The-Air) updates?
- [ ] Tilføj Modbus TCP/IP variant for ESP32?
- [ ] Opret library af Modbus engine til genbrug
- [ ] Dokumenter API i Doxygen format

## 📝 Noter

**Hvor finder jeg .cpp filerne?**
- De er normalt i samme mappe som .ino filen i Arduino IDE
- Eller de kan være spredt i forskellige tabs i Arduino IDE
- Eksporter evt. via Arduino IDE: Sketch → Export compiled Binary

**Hvad hvis en .cpp fil mangler?**
- Check om funktionaliteten er implementeret i .ino filen
- Nogle gange er alt inkluderet i hovedfilen
- Ellers skal du muligvis skrive implementeringen selv

**Hjælp! Jeg kan ikke finde alle filerne!**
- Send mig hele din Arduino projektmappe
- Jeg kan hjælpe med at identificere manglende dele
