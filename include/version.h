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
#define VERSION_PATCH    7
#define VERSION_STRING  "v3.1.7"
#define VERSION_BUILD   "20251108"

// ============================================================================
//  CHANGELOG (uddrag)
// ============================================================================
//
//  v3.1.4-patch2 (2025-11-05)
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
