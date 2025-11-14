# 🎯 Projekt Status - Modbus RTU Server v3.6.1

**Status:** ✅ **PRODUCTION READY** (Unified Prescaler Architecture)
**Sidste opdatering:** 2025-11-14
**GitHub:** [Jangreenlarsen/Modbus_server_slave](https://github.com/Jangreenlarsen/Modbus_server_slave)

---

## 📊 Build Status

```
Platform: Arduino Mega 2560 (ATmega2560 @ 16MHz)
RAM:      83.5% (6841 / 8192 bytes)
Flash:    27.2% (68994 / 253952 bytes)
EEPROM:   Schema v10
Status:   ✅ STABLE - Unified Prescaler Implementation
```

**RAM Usage Note:** RAM steg fra 56.8% (v3.2.0) til 82.4% (v3.3.0) pga. bevidst fix for stack overflow. `PersistConfig` (~1KB) blev flyttet fra stack til global RAM for at undgå crashes. Systemet har stadig 1.3KB fri RAM til runtime operations.

---

## ✅ Komplet Implementering

### Core System
- ✅ **Modbus RTU Server** - Fuld Modbus RTU slave implementering
  - FC01, FC02, FC03, FC04 (Read operations)
  - FC05, FC06 (Write single)
  - FC0F, FC10 (Write multiple)
  - RS-485 direction control (pin 8)
  - CRC16 validering
  - Automatic frame gap timing

### CLI (Command Line Interface)
- ✅ **Interactive Shell** - Cisco-style CLI over USB Serial
  - Remote echo og backspace support
  - Command history (3 commands, arrow navigation)
  - 256 char command buffer
  - Case-insensitive kommandoer
  - Aliases (sh, wr, rd, sv, ld, etc.)
  - Help system med eksempler

### TimerEngine v2
- ✅ **4 Uafhængige Timers**
  - Mode 1: One-shot sequences (3-phase)
  - Mode 2: Monostable (retriggerable)
  - Mode 3: Astable (blink/toggle)
  - Mode 4: Input-triggered (sub-modes 0/1)
  - Global status/control registers
  - EEPROM persistence

### CounterEngine v6 (v3.6.1 - Unified Prescaler)
- ✅ **4 Uafhængige Counters med 3 Modes**
  - **SW (Polling)**: Software polling via discrete inputs
  - **SW-ISR (Interrupt)**: Hardware interrupt-driven (INT0-INT5 pins: 2,3,18,19,20,21)
  - **HW (Timer5)**: Hardware Timer5 external clock (pin 2, max ~20 kHz)
  - **UNIFIED PRESCALER STRATEGI (v3.6.0-v3.6.1)**:
    - ALL modes count EVERY edge/pulse (no skipping!)
    - Prescaler division ONLY at output (raw register)
    - Consistent behavior: HW, SW, SW-ISR identical approach
  - Edge detection (rising/falling/both)
  - Unified prescaler (1, 4, 8, 16, 64, 256, 1024)
  - Direction (up/down)
  - BitWidth (8/16/32/64)
  - Scale (float multiplier)
  - Debounce (configurable ms)
  - Frequency measurement (0-20 kHz, direct Hz no compensation)
  - **Three output registers**:
    - `index-reg`: Scaled value (counterValue × scale) - FULL precision
    - `raw-reg`: Prescaled value (counterValue / prescaler) - reduced size
    - `freq-reg`: Frequency in Hz (0-20000)
  - Control register (reset/start/stop/reset-on-read)
  - Overflow detection & auto-reset
  - EEPROM persistence

### EEPROM Config
- ✅ **Persistent Configuration**
  - Schema versioning (current: v11)
  - CRC checksum validation
  - Load/Save/Defaults commands
  - Modbus SAVE via FC06 (write reg 0 = 255)
  - Hostname persistence
  - Static reg/coil configuration
  - GPIO mapping persistence

### GPIO Management
- ✅ **54 Digital Pins**
  - Dynamic mapping (pin ↔ coil/input)
  - Static configuration persistence
  - Automatic GPIO mapping for HW counters
  - Conflict detection for timer/counter pins
  - Show gpio command for status

---

## 📈 Komplet Versionshistorik & Tidslinje

### **v3.6.1** (2025-11-14) - SW-ISR Mode Prescaler Fix ✅
**Status:** 🟢 **PRODUCTION READY**

**CRITICAL FIX:** SW-ISR mode prescaler konsistens (samme problem som SW mode)

**Problem:**
- Efter v3.6.0 fix fik SW-ISR mode DOBBELT prescaling
- ISR handler: `c.edgeCount++` tællede kun hver X'te edge (prescaled)
- Output: `raw = counterValue / prescaler` (divideret IGEN!)
- Resultat: `raw` alt for lille (dobbelt division) ❌

**Root Cause:**
- SW-ISR mode brugte samme edgeCount prescaler check som SW mode
- Blev ikke fjernet i v3.6.0 fix
- modbus_counters_sw_int.cpp linje 105-112

**Ændringer:**
- Fjernet edgeCount prescaler check fra SW-ISR ISR handler
- SW-ISR mode tæller nu ALLE edges (konsistent med HW og SW)
- UNIFIED PRESCALER STRATEGI for ALLE counter modes:
  - HW mode: Hardware tæller alle pulses → raw divideres ved output
  - SW mode: Software tæller alle edges → raw divideres ved output
  - SW-ISR mode: ISR tæller alle edges → raw divideres ved output
- Prescaler division sker KUN i `store_value_to_regs()`

**RAM/Flash:**
- RAM: 83.5% (6841 bytes)
- Flash: 27.2% (68994 bytes)

**Commit:** Pending

---

### **v3.6.0** (2025-11-14) - SW Mode Prescaler Consistency Fix ✅
**Status:** 🟢 **MILESTONE**

**MAJOR BUGFIX:** SW mode prescaler nu konsistent med HW mode

**Problem:**
- SW mode: `value=1200`, `raw=1200` (begge ens) ❌
- HW mode: `value=435679`, `raw=425` (forskellige) ✅
- `read reg 20` = 1200, `read reg 24` = 1200 (samme værdi)

**Root Cause - Dobbelt Prescaler Strategi:**
1. **Ved tælling** (linje 530-540):
   ```cpp
   c.edgeCount++;
   if (c.edgeCount < prescaler) continue;  // Skip
   c.counterValue++;  // Tæl kun hver X'te edge
   ```
   → `counterValue` var allerede reduceret

2. **Ved output** (linje 193-195):
   ```cpp
   if (c.hwMode != 0 && c.prescaler > 1) {  // ← SPRINGES OVER for SW!
     raw = raw / c.prescaler;
   }
   ```
   → `raw` blev IKKE divideret for SW mode

**Løsning:**
- Fjernet edgeCount prescaler check (linje 530-540)
- SW mode tæller nu ALLE edges i counterValue
- Raw register divideres med prescaler for BÅDE SW og HW mode
- Unified strategi: Prescaler division kun ved output

**BREAKING CHANGE:**
- SW mode counterValue øges nu HVER edge (ikke hver X edge)
- Eksisterende counters vil resette efter upgrade

**RAM/Flash:**
- RAM: 83.5% (6841 bytes)
- Flash: 27.2% (69108 bytes)

**Commit:** Pending

---

### **v3.5.0** (2025-11-14) - Show Counters Display Bug Fix ✅

**BUGFIX:** `show counters` viste forkert value og raw

**Problem:**
- `read reg 5` = 435679 (korrekt) ✅
- `read reg 9` = 425 (korrekt) ✅
- `show counters`: value=161342, raw=161342 (begge ens) ❌

**Root Cause:**
- Display kode (linje 906-907) læste fra `c.counterValue` direkte
- Ikke fra holdingRegs[] som `read reg` kommandoer gør
- Registrene havde korrekte værdier, men display viste forkert

**Ændringer:**
- `modbus_counters.cpp` linje 906-930: Læser nu fra holdingRegs[]
- Value: læses fra `holdingRegs[regIndex]`
- Raw: læses fra `holdingRegs[rawReg]`
- Understøtter 16-bit og 32-bit counters (multi-word reads)

**RAM/Flash:**
- RAM: 83.5% (6841 bytes)
- Flash: 27.3% (69224 bytes)

**Commit:** Pending

---

### **v3.4.9** (2025-11-14) - Raw Register Definition Fix (Revert v3.4.8) ✅

**FIX:** Revertet v3.4.8 - raw register nu korrekt divideret

**Problem (v3.4.8):**
- Raw register blev sat til `counterValue` UDEN division
- Misfortolkning af "raw" betydning

**Løsning:**
- Revertet til v3.4.7 logik
- Raw register = `counterValue / prescaler` (efter prescale, FØR scale)
- Value register = `counterValue × scale` (fuld præcision)

**Definition (nu korrekt):**
- **Raw:** Prescaled value (reduceret størrelse)
- **Value:** Scaled output (fuld præcision)
- **Freq:** Faktisk Hz

**RAM/Flash:**
- RAM: 83.5% (6841 bytes)
- Flash: 27.2% (69068 bytes)

**Commit:** Pending

---

### **v3.4.8** (2025-11-14) - Raw Register Definition Fix (FEJL - Revertet) ❌

**FEJL:** Misfortolket raw register definition

**Ændring (forkert):**
- Fjernede prescaler division fra raw register
- Raw = counterValue (UDEN division)

**Problem:**
- Brugeren rapporterede at prescaler fungerede og value var korrekt
- Men raw skulle divideres med prescaler (ikke være ens med value)

**Status:** Revertet i v3.4.9

**RAM/Flash:**
- RAM: 83.5% (6841 bytes)
- Flash: 27.2% (68980 bytes)

**Commit:** Pending

---

### **v3.4.7** (2025-11-14) - Software Prescaler Implementation (Breakthrough) ✅
**Status:** 🟢 **MILESTONE**

**CRITICAL FIX:** HW counter prescaler NU 100% software implementation

**ROOT CAUSE IDENTIFIED - ATmega2560 Timer5 Hardware Limitation:**
- External clock mode (TCCR5B=0x07): Tæller pulses på T5 pin, INGEN prescaler mulig
- Internal prescaler modes (TCCR5B=0x02-0x05): Tæller 16MHz system clock, IGNORERER T5 pin!

**Problem:**
- v3.4.6 satte hardware til internal prescaler mode når prescaler ≠ 1
- Med prescaler=8: Timer tællede 2MHz system clock (16MHz/8) i stedet for pulses! ❌

**Løsning:**
- Hardware bruges NU ALTID i external clock mode (tæl alle pulses)
- Prescaler implementeret 100% i software:
  - `raw = hwValue / prescaler` (software division)
  - `value = hwValue × scale` (fuld præcision)
  - `frequency = faktiske Hz` (ingen kompensation)

**Ændringer:**
- `modbus_counters_hw.cpp`: Hardware ALTID external clock mode (linje 86-91)
- `modbus_counters.cpp`: Alle prescaler værdier supporteret (1,4,8,16,64,256,1024)
- Fjernet hwCounterPrescalerMode array (ikke længere nødvendig)

**RAM/Flash:**
- RAM: 83.5% (6841 bytes)
- Flash: 27.2% (69068 bytes)

**Commit:** Pending

---

### **v3.4.6** (2025-11-14) - HW Prescaler Fix (FORKERT TILGANG) ❌

**BUGFIX (forkert løsning):** HW counter prescaler display og value scaling

**Problem:**
- Display viste mode-værdier (1,3,4,5,6) i stedet for prescaler (1,8,64,256,1024)
- Counter values blev ikke multipliceret med prescaler faktor

**Ændringer (forkert tilgang):**
- `sanitizeHWPrescaler()` returnerer faktiske prescaler værdier
- `hwPrescalerToMode()` converter til mode ved hw_counter_init()
- Counter value scaling: `actualCount = hwValue × prescaler`

**Problem:**
- Brugte stadig hardware prescaler modes forkert
- Rettet fundamentalt i v3.4.7

**RAM/Flash:**
- RAM: 83.7% (6853 bytes)
- Flash: 27.5% (69724 bytes)

**Commit:** Pending

---

### **v3.4.5** (2025-11-14) - HW Prescaler CLI Validation Fix ✅

**BUGFIX:** CLI validation manglede prescaler:8

**Problem:**
- CLI rejektede `prescaler:8` selvom det er valid for HW mode
- Validation kun tillod 1,4,16,64,256,1024

**Ændringer:**
- CLI parser accepterer nu 1,4,8,16,64,256,1024
- Help-tekst opdateret med korrekte værdier

**RAM/Flash:**
- RAM: 83.7% (6853 bytes)
- Flash: 27.5% (69724 bytes)

**Commit:** Pending

---

### **v3.4.4** (2025-11-14) - GPIO Display Fix ✅

**Ændringer:**
- FIX: GPIO display viser nu korrekt pin 2 for Timer5 (HW-T5 mode)
  - cli_shell.cpp: `Serial.print(F("46"))` → `Serial.print(F("2"))`
  - DYNAMIC GPIO mapping: `gpio 2 DYNAMIC at input X (counterY HW-T5)`

**RAM/Flash:**
- RAM: 83.8% (6861 bytes)
- Flash: 27.4% (69468 bytes)

**Commit:** Pending

---

### **v3.4.3** (2025-11-13 22:40) - Timer5 Pin Mapping Fix
**Commit:** `88cc03d`

**KRITISK FIX:** Timer5 external clock bruger pin 2, ikke pin 47!

**Problem:**
- Kode brugte pin 47 baseret på standard ATmega2560 dokumentation
- I praksis virker Timer5 external clock på pin 2 (PE4)
- GPIO mapping viste forkert pin

**Root Cause:**
- Standard docs siger T5 = pin 47 (PL2)
- Men på Arduino Mega 2560 er T5 faktisk på pin 2 (PE4)
- Verificeret working med faktisk hardware test

**Ændringer:**
- `modbus_counters.cpp`: Timer5 pin 47 → 2 (alle 3 steder)
- `modbus_counters_hw.cpp`: pinMode(47) → pinMode(2)
- GPIO cleanup: oldPin/newPin for hwMode=5: 47 → 2

**RAM/Flash:**
- RAM: 82.4% (6747 bytes)
- Flash: 23.8% (60380 bytes)

---

### **v3.4.2** (2025-11-13 22:28) - Timer5 Pin 46→47 (Incorrect)
**Commit:** `99bf80b`

**Ændring:** (senere fundet forkert)
- Ændrede Timer5 pin fra 46 til 47 baseret på docs
- Viste sig at være forkert - rettet i v3.4.3

---

### **v3.4.1** (2025-11-13 21:43) - HW Counter Prescaler Fix
**Commit:** `28bbcc8`

**FIX:** HW counter prescaler og frekvens måling

**Problem:**
- Prescaler blev ignoreret i frekvens-beregning
- Med prescaler=/8 og 1000 Hz input viste frekvens 125 Hz (forkert)

**Root Cause:**
- Frekvens-beregning brugte rå counter delta uden at tage højde for prescaler division

**Ændringer:**
- Tilføjet `hwCounterPrescalerMode[4]` array til at gemme prescaler mode
- `hw_counter_init()`: Gemmer prescaler mode ved init
- `hw_counter_update_frequency()`: Multiplicerer counter delta med prescaler faktor
  - Mode 1: ×1 (no prescale)
  - Mode 3: ×8 (prescale /8)
  - Mode 4: ×64 (prescale /64)
  - Mode 5: ×256 (prescale /256)
  - Mode 6: ×1024 (prescale /1024)

**RAM/Flash:**
- RAM: 82.4% (6747 bytes)
- Flash: 23.8% (60380 bytes)

---

### **v3.4.0** (2025-11-13 20:50) - SW-ISR Interrupt Mode (Production)
**Commit:** `9f8c7d2`

**MILESTONE:** Production release med SW-ISR interrupt-baseret counter mode

**Features:**
- ✅ SW-ISR mode virker perfekt med hardware interrupts
- ✅ Alle debug-kode fjernet (clean production code)
- ✅ ISR trigger counters fjernet
- ✅ Pin polling test fjernet fra main.cpp
- ✅ Debug headers fjernet

**Stable build ready for deployment**

**RAM/Flash:**
- RAM: 82.4% (6747 bytes)
- Flash: 23.8% (60380 bytes)

---

### **v3.4.0-debug** (2025-11-13 20:46) - SW-ISR Fix with Debug
**Commit:** `ee0e252`

**BREAKTHROUGH:** SW-ISR interrupt mode virker nu!

**Problem:**
- Counter med `hw-mode:sw-isr interrupt-pin:2` tællede ikke
- SW polling mode virkede fint
- Pin polling test viste 2000 edges, men INT4 ISR = 0 triggers

**Root Cause 1: Forkert Interrupt Mapping**
- Brugte hardcoded pin-til-interrupt mapping
- Arduino Mega 2560 bruger `digitalPinToInterrupt()` macro
- Hardcoded `{pin 2, intNumber 4}` var forkert

**Root Cause 2: ISR Function Scope**
- ISR function array var local variable
- Array blev destroyed efter function return
- `attachInterrupt()` fik invalid function pointer

**Fixes:**
- Bruger nu `digitalPinToInterrupt(pin)` for korrekt mapping
- ISR function array er nu `static` for at bevare scope
- Tilføjet validering af interrupt pins (2,3,18,19,20,21)

**Debug Features Added:**
- ISR trigger counters per interrupt
- Pin polling test i main.cpp
- Debug output for ISR activity tracking

**User Feedback:** "fuck det KØRE sgu nu" ✅

**RAM/Flash:**
- RAM: 82.6% (6771 bytes) - med debug counters
- Flash: 23.9% (60738 bytes)

---

### **v3.3.6** (2025-11-12 16:09) - GPIO Persistence Fix
**Commit:** `8371818`

**FIX:** GPIO mapping persistence (MAX_GPIO_PINS → NUM_GPIO)

**Problem:**
- GPIO mappings blev ikke gemt korrekt til EEPROM
- Konstant mismatch mellem MAX_GPIO_PINS og NUM_GPIO

**Ændringer:**
- Standardiseret til NUM_GPIO = 54 alle steder
- Fjernet MAX_GPIO_PINS references
- EEPROM schema opdateret (ingen schema bump nødvendigt)

**RAM/Flash:**
- RAM: 82.4% (6747 bytes)
- Flash: 23.8% (60380 bytes)

---

### **v3.3.5** (2025-11-12 16:02) - EEPROM Clear Runtime Fields
**Commit:** `15f4736`

**FIX:** EEPROM persistence - Clear runtime fields før save

**Problem:**
- Runtime felter (counterValue, edgeCount, lastEdgeMs, etc.) blev gemt til EEPROM
- Forårsagede uventet counter state efter reboot

**Ændringer:**
- `configSave()`: Clearer alle runtime-felter før EEPROM write
- Runtime felter: counterValue, running, overflowFlag, lastLevel, edgeCount, lastEdgeMs, lastCountForFreq, lastFreqCalcMs, currentFreqHz

**RAM/Flash:**
- RAM: 82.4% (6747 bytes)
- Flash: 23.8% (60380 bytes)

---

### **v3.3.4** (2025-11-12 15:53) - Conflict Detection Safeguards
**Commit:** `5f2895c`

**Safeguards:** GPIO conflict detection og EEPROM persistence

**Ændringer:**
- Forbedret validation i `gpio_handle_dynamic_conflict()`
- Bedre fejlmeddelelser ved GPIO konflikter
- Sikrer at mappings bliver renset korrekt

**RAM/Flash:**
- RAM: 82.4% (6747 bytes)
- Flash: 23.8% (60380 bytes)

---

### **v3.3.3** (2025-11-12 15:41) - Global Interrupt Enable
**Commit:** `0398751`

**FIX:** Enable interrupts globally for HW counter ISRs

**Problem:**
- HW counter ISRs blev ikke trigget
- Interrupts var disabled efter setup

**Ændring:**
- `main.cpp`: Tilføjet `sei()` i slutningen af `setup()`
- Arduino disabler interrupts under setup - skal re-enables manuelt

**RAM/Flash:**
- RAM: 82.4% (6747 bytes)
- Flash: 23.8% (60380 bytes)

---

### **v3.3.2** (2025-11-12 15:23) - Auto GPIO Mapping for HW Counters
**Commit:** `771d2ea`

**Feature:** Automatic GPIO-input mapping for HW-mode counters

**Ændringer:**
- HW counters opretter automatisk DYNAMIC GPIO mapping
- Timer1 (hwMode=1) → pin 5
- Timer3 (hwMode=3) → pin 47
- Timer4 (hwMode=4) → pin 6
- Timer5 (hwMode=5) → pin 46
- Mappings cleares automatisk ved mode change

**RAM/Flash:**
- RAM: 82.4% (6747 bytes)
- Flash: 23.8% (60380 bytes)

---

### **v3.3.1** (2025-11-12 15:11) - GPIO Conflict Detection
**Commit:** `0f67802`

**Feature:** GPIO conflict detection for DYNAMIC timer/counter control

**Ændringer:**
- `gpio_handle_dynamic_conflict()` function
- Validation ved GPIO map/unmap
- Forhindrer konflikter mellem timers og counters
- Bedre fejlmeddelelser

**RAM/Flash:**
- RAM: 82.4% (6747 bytes)
- Flash: 23.8% (60380 bytes)

---

### **v3.3.0** (2025-11-11 23:40) - Hardware Counter Engine
**Commit:** `69be55a`

**MAJOR RELEASE:** Hybrid HW/SW Counter Engine (CounterEngine v4)

**New Features:**
- **Hardware Mode:** Direct timer input (T1/T3/T4/T5 på pins 5/47/6/46)
- **Software Mode:** GPIO pin-based edge detection med debounce
- Explicit timer selection via `hw-mode` parameter (hw-t1|hw-t3|hw-t4|hw-t5|sw)
- Automatic GPIO mapping (DYNAMIC) for hardware counters

**CLI Improvements:**
- Separate discrete input (input-dis) vs GPIO pin display
- DYNAMIC GPIO mappings vist i GPIO section
- Korrekt pin column i "show counters"

**CRITICAL FIX: Stack Overflow**
- `PersistConfig` flyttet fra stack til global RAM (main.cpp)
- Single verify buffer i `configSave()` (config_store.cpp)
- **Dette forklarer RAM stigning fra 56.8% til 82.4%**

**EEPROM:**
- Schema v10 (added hwMode field)

**Documentation:**
- NY: Comprehensive MANUAL.md (46 KB detailed user guide)
- NY: HTML manual v3.3.0

**RAM/Flash:**
- RAM: 82.4% (6751 bytes) ← **STIGNING fra v3.2.0 (56.8%)**
- Flash: 21.2% (53956 bytes)

**RAM Stigning Forklaring:**
- `PersistConfig globalConfig` ~1KB (permanent RAM)
- `static PersistConfig verifyTempConfig` ~1KB (permanent RAM)
- Total: ~2KB ekstra RAM (bevidst fix for stack overflow)

---

### **v3.2.0** (2025-11-10 13:24) - Frequency Measurement
**Commit:** `cc1b57f`

**MAJOR RELEASE:** Counter frequency measurement og consistent naming

**New Features:**
- **Frequency Measurement:**
  - Automatic Hz measurement opdateret hvert sekund
  - Robust timing windows (1-2 sec) med validation
  - Håndterer Modbus bus activity uden fejl (clamping 0-20kHz)
  - Reset ved counter reset, overflow, og 5sec timeout

- **Configurable Raw Register:**
  - Separate register for unscaled counter value
  - Backward compatible fallback til index-reg+4

- **Consistent Parameter Naming:**
  - `index-reg` (prev. reg/count-reg) = scaled value
  - `raw-reg` (new) = raw unscaled value
  - `freq-reg` (new) = frequency i Hz
  - `overload-reg` (prev. overload) = overflow flag
  - `ctrl-reg` (prev. control/control-reg) = control bits
  - `input-dis` (prev. input) = discrete input index
  - CLI accepterer både nye og gamle navne (backward compatibility)

**CLI Improvements:**
- Command buffer: 256 chars (fra 128)
- Support for både "res:" og "resolution:" parameter
- "show counters" tabel med nye kolonner (ir, rr, fr, or, cr, dis, hz)

**Frequency Stability:**
- Timing window validation (1-2 sec)
- Delta count validation (max 100kHz)
- Overflow wrap-around sanity check
- Result clamping (0-20000 Hz)
- Error recovery (5 sec timeout reset)

**EEPROM:**
- Schema v9 (CounterConfig med freq/raw fields)

**RAM/Flash:**
- RAM: 56.8% (4655 bytes)
- Flash: 20.1% (51058 bytes)

---

### **v3.1.9** (2025-11-10 11:38) - Counter Control Improvements
**Commit:** `bd8fdc2`

**Major Changes:**
- CLI syntax ændret: `set counter <id> reset-on-read ENABLE|DISABLE`
- CLI command: `set counter <id> start ENABLE|DISABLE`
- Control register fully writable via Modbus FC6
- Kun bit 3 (reset-on-read) persisterer i EEPROM
- Optimeret command history til 3 commands (RAM: 47.3%)
- Fixed EEPROM schema mismatch (schema 7→8)
- Fixed scale float display i show counters
- Remote echo og backspace support i CLI

**EEPROM:**
- Schema v8

---

### **v3.1.7** (2025-11-09 17:33) - Initial Git Commit
**Commit:** `94977bd`

**Initial Features:**
- Modbus RTU Server (FC01-10)
- Interactive CLI (Cisco-style)
- TimerEngine (4 timers, 4 modes)
- CounterEngine v2 (4 counters)
- EEPROM persistence
- GPIO management

---

## 📚 Dokumentation

### Manualer
- ✅ **Modbus server V3.3.0 Manual - HW-SW Counter Engine.html**
  - Komplet system manual (dansk)
  - Timer og Counter dokumentation
  - CLI kommando reference
  - Versionshistorik
  - Eksempler og troubleshooting

### README Filer
- ✅ README.md - Projekt oversigt
- ✅ INSTALLATION.md - Installations guide
- ✅ PLATFORMIO_REFERENCE.md - PlatformIO reference
- ✅ CLAUDE.md - Claude Code instruktioner

---

## 🔧 Udviklings Værktøjer

### Build System
- ✅ PlatformIO - Modern build system
- ✅ Git version control
- ✅ GitHub remote repository

### Development Environment
- ✅ VS Code + PlatformIO extension
- ✅ Serial Monitor integration
- ✅ Upload automation

---

## 🎯 Known Issues & Limitations

### Hardware Limitations
- ⚠️ **Timer5 kun**: Arduino Mega 2560 router kun Timer5 external clock til headers (pin 2)
  - Timer1, Timer3, Timer4 external clock inputs er IKKE tilgængelige på board headers
  - Derfor virker kun `hw-mode:hw-t5` for hardware counter mode

### RAM Constraints
- ⚠️ **83.8% RAM usage**: 1.3KB fri RAM tilgængelig
  - PersistConfig (~2KB) er permanent i RAM (fix for stack overflow)
  - Begrænsning for store dynamiske allocations
  - Overvej RAM cleanup hvis nye features tilføjes

---

## 🎯 Næste Skridt (Valgfri)

### Mulige Forbedringer
- ⏳ Unit tests (test/ mappe)
- ⏳ Modbus TCP/IP variant (ESP32)
- ⏳ Web interface for konfiguration
- ⏳ MQTT integration
- ⏳ OTA updates
- ⏳ Optimering af RAM forbrug (hvis nødvendigt)

### Optimering
- ⏳ Flash optimization (27.4% brugt, god margin)
- ⏳ Response time profiling
- ⏳ Counter frequency measurement optimization for højere frekvenser

---

## 💡 Brug

### Upload til Arduino Mega
```bash
pio run -t upload
```

### Monitor Serial (CLI)
```bash
pio device monitor
```

### Test Modbus
```bash
# Eksempel: Læs holding registers 100-115
# Via Modbus master tool på RS-485 bus
```

---

## 📞 Support

**Issues:** [GitHub Issues](https://github.com/Jangreenlarsen/Modbus_server_slave/issues)
**Seneste Release:** [v3.4.3](https://github.com/Jangreenlarsen/Modbus_server_slave/releases/tag/v3.4.3)

---

## 🎉 Konklusion

✅ **Projektet er komplet og production-ready!**

- Alle features implementeret og testet
- Stabil frekvens-måling under Modbus load
- SW-ISR interrupt mode verified working
- Hardware counter mode (Timer5) verified working på pin 2
- Omfattende dokumentation
- Version controlled med git tags
- RAM og Flash godt inden for grænser (83.8% / 27.4%)

**Status: READY FOR DEPLOYMENT** 🚀

---

## 📊 Udviklings Statistik

**Total Commits:** 24
**Development Period:** 2025-11-09 til 2025-11-14 (6 dage)
**Major Versions:** v3.1.7 → v3.2.0 → v3.3.0 → v3.4.0 → v3.4.4
**Lines of Code:** ~8000+ (estimeret)
**Critical Fixes:** 8 (stack overflow, ISR mapping, prescaler, GPIO persistence, etc.)

**Development Highlights:**
- 🎯 Rapid prototyping og debugging
- 🔧 Hardware verification (Timer5 på pin 2)
- 🐛 Root cause analysis (ISR scope, interrupt mapping)
- 📚 Comprehensive documentation
- ✅ Production-ready stability
