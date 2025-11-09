#pragma once
// ============================================================================
//  Filnavn : version.h
//  Projekt  : Modbus RTU Server / CLI
//  Version  : v3.1.4-patch2 (2025-11-05)
//  Forfatter: ChatGPT Automation
//  Formål   : Global versions-info og ændringshistorik
//  Ændringer:
//    - v3.1.4-patch2: CounterEngine v3 – debounce pr. counter-kanal
//                     (debounceEnable + debounceTimeMs), CLI-udvidelse
//                     med debounce:on|off og debounce-ms:<n>.
//    - v3.1.4: CounterEngine v3 (direction + scale), EEPROM schema v5,
//              CLI counter-udvidelser og CLEAN persistens.
// ============================================================================

#define VERSION_MAJOR    3
#define VERSION_MINOR    1
#define VERSION_PATCH    9
#define VERSION_STRING  "v3.1.9"
#define VERSION_BUILD   "20251109"

// ============================================================================
//  CHANGELOG (uddrag)
// ============================================================================
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
