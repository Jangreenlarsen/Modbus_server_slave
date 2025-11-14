#pragma once
// ============================================================================
//  Filnavn : version.h
//  Projekt  : Modbus RTU Server / CLI
//  Version  : v3.6.2 (2025-11-14) - GPIO Mapping Conflict Fix
//  Forfatter: JanG at modbus_slave@laces.dk
//  Formål   : Global versions-info og ændringshistorik
//  Ændringer:
//    - v3.6.2: CRITICAL FIX: HW mode GPIO mapping conflicts
//              - PROBLEM: Når HW counter konfigureres, kunne andre PINs blive mappet til samme inputIndex
//              - Eksempel: PIN 2 (Timer5) til input 12, mens PIN 47 OGSÅ var mappet til 12
//              - RESULTAT: Begge counters læste samme discrete input → tælte identiske værdier
//              - LØSNING: Når HW mode sætter GPIO mapping, fjernes nu alle andre PINs
//                        mappet til samme inputIndex først (konflikt-håndtering)
//              - BONUS: Fixed display PIN for hwMode=5 (var 47, skal være 2 - PE4)
//    - v3.6.1: CRITICAL FIX: SW-ISR mode prescaler
//    - v3.6.0: SW mode prescaler konsistens fix (edgeCount fjernet)
//    - v3.5.0: Show counters display fix (læs fra registre)
//    - v3.4.7: HW mode external clock implementation
//    - v3.3.0: Hybrid HW/SW Counter Engine
//    - v3.2.0: Frequency measurement
//    - v3.1.4: CounterEngine v3 (direction + scale)
// ============================================================================

#define VERSION_MAJOR    3
#define VERSION_MINOR    6
#define VERSION_PATCH    2
#ifndef VERSION_STRING_NY
#define VERSION_STRING_NY  "v3.6.2"
#endif
#ifndef VERSION_BUILD
#define VERSION_BUILD   "20251114"
#endif
#ifndef CLI_VERSION
#define CLI_VERSION     "v3.6.1"
#endif

// ============================================================================
//  CHANGELOG (uddrag)
// ============================================================================
//
//  v3.6.1 (2025-11-14) - CRITICAL FIX: SW-ISR Mode Prescaler Consistency
//   • BUGFIX: SW-ISR mode havde samme edgeCount prescaler problem som SW mode
//   • PROBLEM: Efter v3.6.0 fix fik SW-ISR mode DOBBELT prescaling:
//       - ISR handler: c.edgeCount++ → tæller kun hver X'te edge (prescaled)
//       - Output: raw = counterValue / prescaler (divideret IGEN!)
//       - Resultat: raw alt for lille (dobbelt division) ❌
//   • LØSNING: Fjernet edgeCount prescaler check fra modbus_counters_sw_int.cpp
//   • UNIFIED PRESCALER STRATEGI for ALLE counter modes:
//       - HW mode (Timer5): Hardware tæller alle pulses → raw divideres ved output
//       - SW mode (Polling): Software tæller alle edges → raw divideres ved output
//       - SW-ISR mode (Interrupt): ISR tæller alle edges → raw divideres ved output
//   • KONSISTENT ADFÆRD: Prescaler division sker KUN i store_value_to_regs()
//   • EKSEMPEL med prescaler=64, 76800 edges:
//       - counterValue = 76800 (alle edges talt)
//       - value = 76800 × 1.0 = 76800
//       - raw = 76800 / 64 = 1200 ✅
//
//  v3.6.0 (2025-11-14) - CRITICAL FIX: SW Mode Prescaler Konsistens
//   • MAJOR BUGFIX: SW mode prescaler adfærd nu konsistent med HW mode
//   • ROOT CAUSE: SW mode brugte to forskellige prescaler strategier:
//       1. Ved tælling: edgeCount reducerede counterValue (tæl hver X edge)
//       2. Ved output: raw blev IKKE divideret (kun HW mode)
//       → Resultat: value=1200, raw=1200 (begge ens) ❌
//   • LØSNING: Unified prescaler strategi for SW og HW mode:
//       - Fjernet edgeCount prescaler check (linje 530-540)
//       - SW mode tæller nu ALLE edges i counterValue
//       - Raw register divideres med prescaler for BÅDE SW og HW mode
//       - Prescaler division sker KUN ved output (store_value_to_regs)
//   • KONSEKVENT ADFÆRD:
//       - HW mode: hardware tæller alle pulses → raw=pulses/prescaler
//       - SW mode: software tæller alle edges → raw=edges/prescaler
//   • EKSEMPEL: Prescaler=64, 76800 edges modtaget:
//       - counterValue = 76800 (alle edges)
//       - value = 76800 × 1.0 = 76800 (fuld præcision)
//       - raw = 76800 / 64 = 1200 (reduceret størrelse)
//   • BREAKING CHANGE: SW mode counterValue øges nu HVER edge (ikke hver X edge)
//       - Gamle konfigurationer skal justeres hvis de forventer gammel adfærd
//
//  v3.5.0 (2025-11-14) - CRITICAL FIX: Show Counters Display Bug
//   • BUGFIX: show counters viste forkert value og raw (læste fra c.counterValue)
//   • ROOT CAUSE: Display kode læste ikke fra registre (linje 906-907)
//       - value = c.counterValue × scale (FORKERT - ingen prescaler division anvendt)
//       - raw = c.counterValue (FORKERT - ingen prescaler division anvendt)
//   • FIX: Læser nu fra holdingRegs[] for korrekt display:
//       - value = holdingRegs[regIndex] (skaleret værdi)
//       - raw = holdingRegs[rawReg] (prescaler-divideret værdi)
//   • VERIFICATION: read reg kommandoer viste korrekt, men show counters ikke
//   • RESULTAT: show counters viser nu samme værdier som read reg kommandoer ✅
//
//  v3.4.9 (2025-11-14) - FIX: Raw Register Korrekt Definition (Revert v3.4.8)
//   • FIX: Raw register viser nu KORREKT værdi (divideret med prescaler)
//   • DEFINITION (nu korrekt implementeret):
//       - Raw register = counterValue / prescaler (efter prescale, FØR scale)
//       - Value register = counterValue × scale (fuld præcision, INGEN prescaler)
//       - Freq register = faktiske Hz (direkte måling)
//   • FORMÅL med raw register:
//       - Viser "prescaled" værdi med reduceret størrelse (f.eks. 14 i stedet for 14097)
//       - Nyttig for at spare register plads når høje værdier ikke er nødvendige
//       - Value register beholder fuld præcision for skaleret output
//   • RESULTAT med prescaler=1024, 14097 pulses:
//       - raw = 13 eller 14 (14097 / 1024)
//       - value = 14097 (14097 × 1.0)
//       - freq = ~1002 Hz
//
//  v3.4.8 (2025-11-14) - FEJL: Misfortolket Raw Register Definition
//   • FEJL: Raw blev sat til counterValue UDEN division (forkert)
//   • Revertet i v3.4.9
//
//  v3.4.7 (2025-11-14) - CRITICAL: Software Prescaler Implementation for HW Counters
//   • ROOT CAUSE IDENTIFIED: ATmega2560 Timer5 hardware limitation!
//       - External clock mode (0x07): Tæller pulses på T5 pin, INGEN prescaler
//       - Internal prescaler modes (0x02-0x05): Tæller 16MHz system clock, IKKE pulses!
//   • FIX: Hardware bruges NU ALTID i external clock mode (tæller alle pulses)
//   • Raw register division med prescaler implementeret korrekt ✅
//
//  v3.4.6 (2025-11-14) - CRITICAL: HW Counter Prescaler Display & Value Scaling Fix
//   • FEJL: Denne version brugte stadig hardware prescaler modes forkert!
//   • BUGFIX: HW counter prescaler display viste mode-værdier (1,3,4,5,6) i stedet
//     for faktiske prescaler værdier (1,8,64,256,1024)
//   • BUGFIX: HW counter values blev ikke multipliceret med prescaler faktor
//       - Med prescaler=/8 og 1000 pulses viste counter 125 (forkert)
//       - Nu vises 1000 (125 × 8) korrekt ✅
//   • NEW: sanitizeHWPrescaler() returnerer nu faktisk prescaler værdi
//   • NEW: hwPrescalerToMode() helper til mode conversion ved hw_counter_init()
//   • IMPROVED: Counter value scaling i counters_loop(): actualCount = hwValue × prescaler
//   • IMPROVED: CLI validation tillader nu alle værdier: 1,4,8,16,64,256,1024
//       - HW mode understøtter: 1,8,64,256,1024 (hardware limitation)
//       - Auto-mapping: 4→8, 16→64 for forward compatibility
//   • IMPROVED: Frequency calculation bruger korrekt prescaler multiplier
//   • RAM: 83.7%, Flash: 27.5%
//
//  v3.4.5 (2025-11-14) - HW Prescaler CLI Validation Fix
//   • BUGFIX: CLI validation manglede prescaler:8 (kun tillod 1,4,16,64,256,1024)
//   • NEW: CLI parser accepterer nu 1,4,8,16,64,256,1024
//   • Help-tekst opdateret med korrekt prescaler værdier
//
//  v3.4.4 (2025-11-14) - GPIO Display Fix for Timer5
//   • BUGFIX: GPIO display viser nu korrekt pin 2 for Timer5 (HW-T5 mode)
//   • cli_shell.cpp: "gpio 2 DYNAMIC at input X (counterY HW-T5)" ✅
//
//  v3.4.3 (2025-11-13) - Timer5 Pin Mapping Fix (46/47 → 2)
//   • CRITICAL FIX: Timer5 external clock bruger pin 2 (PE4), ikke pin 47!
//   • modbus_counters.cpp: Alle Timer5 pin references ændret til pin 2
//   • modbus_counters_hw.cpp: pinMode(2) for Timer5 (verified working)
//   • Root cause: Standard ATmega2560 docs siger T5=pin 47, men Arduino Mega
//     router faktisk T5 til pin 2 (PE4) - verificeret med hardware test
//
//  v3.4.1 (2025-11-13) - HW Counter Prescaler & Frequency Fix
//   • BUGFIX: HW counter prescaler blev ignoreret i frekvens-beregning
//   • NEW: hwCounterPrescalerMode[4] array til at gemme prescaler mode
//   • IMPROVED: hw_counter_update_frequency() multiplicerer med prescaler faktor
//   • Example: prescaler=/8 + 1000 Hz input → frequency viser nu 1000 Hz (ikke 125 Hz)
//
//  v3.4.0 (2025-11-13) - SW-ISR Interrupt Mode Fix (Production)
//   • BREAKTHROUGH: SW-ISR interrupt mode virker nu perfekt!
//   • BUGFIX: attachInterrupt() brugte hardcoded pin mapping i stedet for
//     digitalPinToInterrupt() macro (Arduino Mega 2560 specific)
//   • BUGFIX: ISR function array var local scope - ændret til static
//   • CLEANUP: Alle debug counters og test code fjernet (production ready)
//   • INT0-INT5 pins (2,3,18,19,20,21) verified working
//   • User feedback: "fuck det KØRE sgu nu" ✅
//
//  v3.3.0 (2025-11-11) - Hardware Counter Engine with HW/SW Selection
//   • NEW: Hybrid HW/SW Counter Engine (CounterEngine v4):
//       - Hardware Mode: Direct timer input (T1/T3/T4/T5 on pins 5/47/6/46)
//       - Software Mode: GPIO pin-based edge detection
//       - Explicit timer selection via hw-mode parameter
//       - Automatic GPIO mapping (DYNAMIC) for HW mode
//   • NEW: Improved CLI configuration display:
//       - Separate discrete input (input-dis) vs GPIO pin display
//       - DYNAMIC GPIO mappings in GPIO section
//       - Corrected pin column in "show counters" (actual GPIO pins)
//   • BUGFIX: Fixed SAVE command stack overflow (global config allocation)
//   • EEPROM schema updated to v10 (added hwMode field to CounterConfig)
//   • CLI parser: hw-mode:<sw|hw-t1|hw-t3|hw-t4|hw-t5> parameter
//   • configSave() optimized: single verify buffer, const_cast on reference
//   • Backward compatible with v3.2.0 configurations
//   • RAM: 82.4%, Flash: 21.2%
//
//  v3.2.0 (2025-11-10)
//   • NEW: Counter frequency measurement (currentFreqHz) med freq-reg parameter
//   • NEW: Raw counter register (raw-reg) separat fra skaleret værdi
//   • IMPROVED: Konsistent parameter-navngivning for counter commands:
//       - index-reg (tidl. count-reg) = skaleret værdi
//       - raw-reg = rå u-skaleret værdi (ny)
//       - freq-reg = målt frekvens i Hz (ny)
//       - overload-reg (tidl. overload) = overflow flag
//       - ctrl-reg (tidl. control-reg) = control bits
//       - input-dis (tidl. input) = discrete input index
//   • EEPROM schema opdateret til v9 (CounterConfig med freq/raw fields)
//   • CLI help opdateret med ny parameter syntax
//   • "show counters" viser nu også freq (Hz) kolonne
//   • Backward compatibility: CLI parser accepterer både gamle og nye navne
//
//  v3.1.9 (2025-11-09)
//   • MAJOR: Counter reset-on-read er nu individuel pr. counter via global
//     counterResetOnReadEnable[4] array (ikke længere via bit 3 i controlReg).
//   • CLI-kommando: "set counters reset-on-read counter1:on counter2:off ..."
//   • "show config" viser nu "counter1 reset-on-read ENABLE/DISABLE"
//   • "show counters" viser reset-on-read status for hver counter
//   • EEPROM schema opdateret til v7 med counterResetOnReadEnable array
//   • Bit 3 i controlReg bruges ikke længere (frigivet til fremtidig brug)
//
//  v3.1.8 (2025-11-09)
//   • Counter control register vises nu i "show config" med fortolkning:
//       bit0=RESET, bit1=START, bit2=STOP, bit3=Reset-On-Read ENABLE
//   • "show counters" viser nu også control register status med fortolket værdi
//   • BUGFIX: Counter controlFlags bit 3 persisterer nu korrekt efter genstart
//
//  v3.1.7:
//   • CounterEngine v3: debounce pr. kanal (debounceEnable + debounceTimeMs).
//   • CLI: set counter ... debounce:on|off debounce-ms:<n>, visning i
//     show config / show counters.
//
//  v3.1.4 (2025-11-04)
//   • CounterEngine v3: direction (up/down) + float scale-faktor.
//   • Auto-reset ved overflow/underflow til startValue og overflowFlag-register.
//   • controlReg: bit0=reset, bit1=start, bit2=stop (soft-control).
//   • EEPROM schema v5 (CounterConfig v3) + CLEAN defaults ved schema<5.
//   • CLI: set/show counter udvidet med direction/scale.
//
//  v3.1.3-patch2 (2025-11-04)
//   • CLEAN build baseline uden demo-init, unified headers.
//   • configApply() fallback m.m.
//
