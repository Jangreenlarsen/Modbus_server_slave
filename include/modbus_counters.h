// ============================================================================
//  Filnavn : modbus_counters.h
//  Projekt  : Modbus RTU Server / CLI
//  Version  : v3.2.0 (2025-11-09)
//  Forfatter: JanG at modbus_slave@laces.dk
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
//  CounterConfig v5 (schema v11) - v3.3.0
// ============================================================================
//
//  Felter:
//   id             : 1..4 (logisk id)
//   enabled        : 0/1
//   hwMode         : 0=SW, 1=HW (NEW in v3.3.0) - bruger hardware timer
//   edgeMode       : CNT_EDGE_*
//   direction      : CNT_DIR_UP / CNT_DIR_DOWN
//   bitWidth       : 8 / 16 / 32 / 64 (intern maske / antal ud-regs)
//   prescaler      : antal edges pr. tællerskridt (1..256) eller HW prescale mode
//   inputIndex     : discrete input index (0..NUM_DISCRETE-1) [SW polling mode]
//   interruptPin   : GPIO pin for interrupt mode (0=polling, 2/3/18/19/20/21=interrupt) [SW mode only]
//   regIndex       : base holding register for skaleret værdi
//   controlReg     : holding-reg med bitmask (bit0=reset,1=start,2=stop)
//   overflowReg    : holding-reg hvor overflowFlag spejles (0/1)
//   startValue     : initial værdi ved reset/overflow
//   scale          : float faktor (1.0 = ingen skalering)
//   counterValue   : rå tællerværdi (før scaling) [SW mode]
//   running        : 0/1 (kun aktiv når enabled && running)
//   overflowFlag   : 0/1 (sættes ved overflow / underflow)
//   lastLevel      : sidste input-niveau (edge detect) [SW polling mode]
//   edgeCount      : rå edge-counter til prescaler [SW mode]
//   debounceEnable : 0/1 – aktiverer debounce på input [SW mode]
//   debounceTimeMs : debounce-vindue i millisekunder [SW mode]
//   lastEdgeMs     : tidspunkt (millis) for sidste accepterede edge [SW mode]
//
struct CounterConfig {
  // Konfiguration
  uint8_t   id;          // 1..4
  uint8_t   enabled;     // 0/1
  uint8_t   hwMode;      // 0=SW polling, 1=HW interrupt (NEW v3.3.0)
  uint8_t   edgeMode;    // CounterEdge
  uint8_t   direction;   // CounterDirection (0=UP,1=DOWN)

  uint8_t   bitWidth;    // 8/16/32/64
  uint16_t  prescaler;   // SW/SW-ISR: 1|4|16|64|256|1024 | HW: 0=off, 1=ext-clock, 2-6=prescale
  uint16_t  inputIndex;  // discrete input index (SW polling mode only)
  uint8_t   interruptPin; // GPIO pin for SW interrupt mode (0=polling, 2/3/18/19/20/21=interrupt)

  uint16_t  regIndex;    // base holding register for skaleret værdi (index-reg)
  uint16_t  rawReg;      // holding register for rå værdi (raw-reg)
  uint16_t  freqReg;     // holding register for målt frekvens i Hz (freq-reg)
  uint16_t  controlReg;  // bitmask: start/stop/reset (ctrl-reg)
  uint16_t  overflowReg; // holding reg for overflow-flag (overload-reg)

  uint32_t  startValue;  // init-værdi ved reset
  float     scale;       // skaleringsfaktor (1.0 = ingen skalering)

  // Runtime-felter (SW mode primarily)
  uint64_t  counterValue;   // rå tællerværdi (SW mode)
  uint8_t   running;        // 0/1
  uint8_t   overflowFlag;   // 0/1
  uint8_t   lastLevel;      // 0/1 til edge-detektering (SW mode)
  uint32_t  edgeCount;      // raw edge count (før prescaler) (SW mode)

  // Debounce (v3.1.4-patch2, SW mode only)
  uint8_t       debounceEnable;  // 0=off, 1=on
  uint16_t      debounceTimeMs;  // debounce-tid i ms
  unsigned long lastEdgeMs;      // timestamp for sidste accepterede edge

  // Frekvens-måling (v3.2.0, both modes)
  uint64_t      lastCountForFreq;   // sidste count ved frekvens-beregning (SW)
  unsigned long lastFreqCalcMs;     // timestamp for sidste frekvens-beregning (SW)
  uint16_t      currentFreqHz;      // senest målte frekvens i Hz

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
