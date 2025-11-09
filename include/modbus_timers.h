// ============================================================================
//  Filnavn : modbus_timers.h
//  Projekt  : Modbus RTU Server / CLI
//  Version  : v3.0.7-patch4 (2025-11-03)
//  Forfatter: ChatGPT Automation
//  Formål   : Header for timer-engine med coil-styring og alarm/timeout
//  Ændringer:
//    - v3.0.7-patch4: Tilføjet alarm/timeout-felter og CLI helpers
//    - v3.0.7-patch2: Tilføjet timers_hasCoil() til exclusive coil control
//    - v3.0.7       : Grundlæggende timer-definitioner
// ============================================================================

#pragma once
#include <Arduino.h>
#include "modbus_globals.h"

// ============================================================================
// TimerEngine - Global status/control registre
// ============================================================================
extern uint16_t timerStatusRegIndex;      // global status register (bit0..3)
extern uint16_t timerStatusCtrlRegIndex;  // global control register (bit0..3)

// ================= Timer Engine =================
// Op til 4 uafhængige timere der styrer coils
// Modes:
//   1 = One-shot   : P1(T1)->P2(T2)->P3(T3) én gang
//   2 = Monostable : P2(T1) -> P1, retrigger restarter
//   3 = Astable    : skifter mellem P1(T1) / P2(T2)
//   4 = Trigger    : udløses af diskret input, kører subMode(1..3)
// ============================================================================

enum TimerMode : uint8_t {
  TM_ONE_SHOT = 1,
  TM_MONO     = 2,
  TM_ASTABLE  = 3,
  TM_TRIGGER  = 4
};

enum TriggerEdge : uint8_t {
  TRIG_RISING  = 1,   // 0->1
  TRIG_FALLING = 2,   // 1->0
  TRIG_BOTH    = 3
};

struct TimerConfig {
  uint8_t  id;            // 1..4
  uint8_t  enabled;       // 0/1
  uint8_t  mode;          // 1..4
  uint8_t  subMode;       // kun brugt i mode 4
  // Output levels per period
  uint8_t  p1High;        // 0/1
  uint8_t  p2High;        // 0/1
  uint8_t  p3High;        // 0/1
  // Perioder (ms)
  uint32_t T1;
  uint32_t T2;
  uint32_t T3;
  // Mål-coil
  uint16_t coil;          // 0..NUM_COILS-1
  // Trigger input (mode 4)
  uint16_t trigIndex;     // discrete input index
  uint8_t  trigEdge;      // TriggerEdge

  // Runtime
  uint8_t  active;        // kørende sekvens
  uint8_t  phase;         // 0..n (afhængig af mode)
  unsigned long phaseStartMs;
  uint8_t  lastTrigLevel; // til edge-detect

  // Diagnostik / alarm
  uint8_t  alarm;         // 0/1
  uint8_t  alarmCode;     // 0=ok, 1=timeout, 2=phase stuck (reserveret)
  unsigned long lastDurationMs; // seneste fulde lapse (ms)

    // Reset-on-read status sticky flag
  uint8_t statusRoEnable;   // 0/1 - gemmes i EEPROM, styrer bit i TIMER_STATUS_CTRL_REG_INDEX
};

// ===== API =====
void timers_init();
void timers_loop();
void timers_onCoilWrite(uint16_t coilIdx, uint8_t value);

// Helpers til CLI / config
void timers_disable_all();
bool timers_config_set(uint8_t id, const TimerConfig& src);
bool timers_get(uint8_t id, TimerConfig& out);

// Opgave 1b: tjek om coil styres af en aktiv timer
bool timers_hasCoil(uint16_t idx);

// Opgave 2+3: CLI helpers til status og alarm
void timers_print_status();
void timers_clear_alarms();

// Globalt array deklareres i .cpp
extern TimerConfig timers[4];
