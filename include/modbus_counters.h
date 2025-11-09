// ============================================================================
//  Filnavn : modbus_counters.h
//  Projekt  : Modbus RTU Server / CLI
//  Version  : v3.2.0 (2025-11-09)
//  Forfatter: ChatGPT Automation
//  Formål   : Header for CounterEngine v3 med 4 uafhængige input-tællere,
//             inkl. prescaler, bitwidth, overflow-flag, retning, scaling,
//             soft-control via controlReg (start/stop/reset) og debounce.
//  Ændringer:
//    - v3.2.0: Tilføjet counterAutoStartEnable[4] array for individuel
//              auto-start kontrol ved config load/reboot.
//              CLI-kommando: "set counters start counter<n>:on|off"
//    - v3.1.9: Tilføjet global counter control-struktur med individuelle
//              reset-on-read enable flags (counterResetOnReadEnable[4]).
//              CLI-kommando: "set counters reset-on-read counter<n>:on|off"
//    - v3.1.4-patch3: Tilføjet counters_print_status() til tabelformat
//                     'show counters' i CLI.
//    - v3.1.4-patch2: Tilføjet debounceEnable/debounceTimeMs/lastEdgeMs til
//                     CounterConfig + runtime-debounce i counters_loop().
// ============================================================================

#pragma once
#include <Arduino.h>
#include "modbus_globals.h"

// ============================================================================
// CounterEngine - Global control arrays
// ============================================================================
// Arrays til at styre counter funktioner individuelt for hver counter (1..4)
// 0 = disabled, 1 = enabled
extern uint8_t counterResetOnReadEnable[4];  // index 0..3 = counter 1..4
extern uint8_t counterAutoStartEnable[4];    // auto-start ved config load/reboot

// Interne helpers brugt af CounterEngine og Modbus FC
uint8_t sanitizeBitWidth(uint8_t bw);
uint64_t maskToBitWidth(uint64_t v, uint8_t bw);

// ============================================================================
//  Konstanter / enums
// ============================================================================

enum CounterEdge : uint8_t {
  CNT_EDGE_RISING  = 1,
  CNT_EDGE_FALLING = 2,
  CNT_EDGE_BOTH    = 3
};

enum CounterDirection : uint8_t {
  CNT_DIR_UP   = 0,
  CNT_DIR_DOWN = 1
};

// ============================================================================
//  CounterConfig v3 (schema v5)
// ============================================================================
//
//  Felter:
//   id             : 1..4 (logisk id)
//   enabled        : 0/1
//   edgeMode       : CNT_EDGE_*
//   direction      : CNT_DIR_UP / CNT_DIR_DOWN
//   bitWidth       : 8 / 16 / 32 / 64 (intern maske / antal ud-regs)
//   prescaler      : antal edges pr. tællerskridt (1..256)
//   inputIndex     : discrete input index (0..NUM_DISCRETE-1)
//   regIndex       : base holding register for skaleret værdi
//   controlReg     : holding-reg med bitmask (bit0=reset,1=start,2=stop)
//   overflowReg    : holding-reg hvor overflowFlag spejles (0/1)
//   startValue     : initial værdi ved reset/overflow
//   scale          : float faktor (1.0 = ingen skalering)
//   counterValue   : rå tællerværdi (før scaling)
//   running        : 0/1 (kun aktiv når enabled && running)
//   overflowFlag   : 0/1 (sættes ved overflow / underflow)
//   lastLevel      : sidste input-niveau (edge detect)
//   edgeCount      : rå edge-counter til prescaler
//   debounceEnable : 0/1 – aktiverer debounce på input
//   debounceTimeMs : debounce-vindue i millisekunder
//   lastEdgeMs     : tidspunkt (millis) for sidste accepterede edge
//
struct CounterConfig {
  // Konfiguration
  uint8_t   id;          // 1..4
  uint8_t   enabled;     // 0/1
  uint8_t   edgeMode;    // CounterEdge
  uint8_t   direction;   // CounterDirection (0=UP,1=DOWN)

  uint8_t   bitWidth;    // 8/16/32/64
  uint8_t   prescaler;   // 1..256
  uint16_t  inputIndex;  // discrete input index

  uint16_t  regIndex;    // base holding register for værdi
  uint16_t  controlReg;  // bitmask: start/stop/reset
  uint16_t  overflowReg; // holding reg for overflow-flag (0/1)

  uint32_t  startValue;  // init-værdi ved reset
  float     scale;       // skaleringsfaktor (1.0 = ingen skalering)

  // Runtime-felter
  uint64_t  counterValue;   // rå tællerværdi
  uint8_t   running;        // 0/1
  uint8_t   overflowFlag;   // 0/1
  uint8_t   lastLevel;      // 0/1 til edge-detektering
  uint32_t  edgeCount;      // raw edge count (før prescaler)

  // Debounce (v3.1.4-patch2)
  uint8_t       debounceEnable;  // 0=off, 1=on
  uint16_t      debounceTimeMs;  // debounce-tid i ms
  unsigned long lastEdgeMs;      // timestamp for sidste accepterede edge

  // Persistent control-flags (fx reset-on-read)
  uint16_t controlFlags;   // bitmask, bit3 = reset-on-read
};

// Globalt array
extern CounterConfig counters[4];

// ============================================================================
//  API – init / loop
// ============================================================================

// Initierer CounterEngine til default-tilstand (ingen tællere aktive)
void counters_init();

// Hoved-loop for CounterEngine (kald fra modbusLoop())
void counters_loop();

// ============================================================================
//  API – CLI / config helpers
// ============================================================================

// Sæt konfiguration for en tæller (id = 1..4). Returnerer false hvis id invalid.
bool counters_config_set(uint8_t id, const CounterConfig& cfg);

// Læs konfiguration for en tæller (id = 1..4). Returnerer false hvis id invalid.
bool counters_get(uint8_t id, CounterConfig& out);

// Reset én tæller til startValue og nulstil overflow-flag
void counters_reset(uint8_t id);

// Nulstil alle tællere og overflowFlags
void counters_clear_all();

// Debug/oversigtsprint til CLI ("show counters")
void counters_print_status();
