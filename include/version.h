#pragma once
// ============================================================================
//  Filnavn : version.h
//  Projekt  : Modbus RTU Server / CLI
//  Version  : v3.3.0 (2025-11-11) - Hardware Counter Engine
//  Forfatter: JanG at modbus_slave@laces.dk
//  Formål   : Global versions-info og ændringshistorik
//  Ændringer:
//    - v3.3.0: Hybrid HW/SW Counter Engine (hw-mode selection, DYNAMIC GPIO,
//              improved CLI display, fixed SAVE overflow), EEPROM schema v10.
//    - v3.2.0: CounterEngine frequency measurement (freq-reg), raw register
//              (raw-reg), consistent parameter naming (index-reg, overload-reg,
//              ctrl-reg, input-dis), EEPROM schema v9.
//    - v3.1.4-patch2: CounterEngine v3 – debounce pr. counter-kanal
//                     (debounceEnable + debounceTimeMs), CLI-udvidelse
//                     med debounce:on|off og debounce-ms:<n>.
//    - v3.1.4: CounterEngine v3 (direction + scale), EEPROM schema v5,
//              CLI counter-udvidelser og CLEAN persistens.
// ============================================================================

#define VERSION_MAJOR    3
#define VERSION_MINOR    3
#define VERSION_PATCH    0
#ifndef VERSION_STRING
#define VERSION_STRING  "v3.3.0"
#endif
#ifndef VERSION_BUILD
#define VERSION_BUILD   "20251111"
#endif

// ============================================================================
//  CHANGELOG (uddrag)
// ============================================================================
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
