// ============================================================================
//  Filnavn : modbus_timers.cpp
//  Projekt  : Modbus RTU Server / CLI
//  Version  : v3.0.7-patch4 (2025-11-03)
//  Forfatter: JanG at modbus_slave@laces.dk
//  Formål   : Timerengine for 4 uafhængige timere med coil-styring,
//             alarm- og timeout-overvågning samt CLI statusprint.
//  Ændringer:
//    - v3.0.7-patch4: Tilføjet alarm/timeout-detektion, statusprint og clear_alarms()
//    - v3.0.7-patch2: Tilføjet timers_hasCoil() til exclusive coil control
//    - v3.0.7       : Grundversion af timer-engine v2
// ============================================================================

#include "modbus_timers.h"
#include "modbus_core.h"
#include <string.h>

static inline void timers_flag_active(uint8_t idx) {
  if (timerStatusRegIndex >= NUM_REGS) return;
  if (idx >= 4) return;
  uint16_t mask = (uint16_t)(1u << idx);
  holdingRegs[timerStatusRegIndex] |= mask;
}

TimerConfig timers[4];  // global timer-array

// ============================================================================
// Global timer status/control registre (kan sættes via CLI og gemmes i config)
// ============================================================================
uint16_t timerStatusRegIndex      = 0;   // default = 0 = deaktiveret
uint16_t timerStatusCtrlRegIndex  = 0;

// ------------------------------------------------------
// Internal helpers
// ------------------------------------------------------
static inline bool di_read(uint16_t idx) {
  if (idx >= NUM_DISCRETE) return false;
  return bitReadArray(discreteInputs, idx);
}

static void set_coil_level(uint16_t coilIdx, bool high) {
  if (coilIdx >= NUM_COILS) return;
  bitWriteArray(coils, coilIdx, high);
}

// ------------------------------------------------------
// Timer logic
// ------------------------------------------------------
static void loop_one_shot(TimerConfig& t, unsigned long now) {
  switch (t.phase) {
    case 0:
      set_coil_level(t.coil, t.p1High);
      if (t.T1 == 0 || now - t.phaseStartMs >= t.T1) {
        t.phase++;
        t.phaseStartMs = now;
      }
      break;

    case 1:
      set_coil_level(t.coil, t.p2High);
      if (t.T2 == 0 || now - t.phaseStartMs >= t.T2) {
        t.phase++;
        t.phaseStartMs = now;
      }
      break;

    case 2:
      set_coil_level(t.coil, t.p3High);
      if (t.T3 == 0 || now - t.phaseStartMs >= t.T3) {
        t.phase++;
      }
      break;

    default:
      t.active = 0;
      t.lastDurationMs = now - t.phaseStartMs;
      break;
  }
}

static void loop_monostable(TimerConfig& t, unsigned long now) {
  if (!t.active) {
    set_coil_level(t.coil, t.p1High);
    return;
  }

  if (t.phase == 0) {
    t.phase = 1;
    t.phaseStartMs = now;
    set_coil_level(t.coil, t.p2High);
  } else if (t.phase == 1 && now - t.phaseStartMs >= t.T1) {
    set_coil_level(t.coil, t.p1High);
    t.active = 0;
    t.phase = 0;
    t.lastDurationMs = now - t.phaseStartMs;
  }
}

static void loop_astable(TimerConfig& t, unsigned long now) {
  if (!t.active) return;

  if (t.phase == 0) {
    set_coil_level(t.coil, t.p1High);
    if (t.T1 == 0 || now - t.phaseStartMs >= t.T1) {
      t.phase = 1;
      t.phaseStartMs = now;
    }
  } else {
    set_coil_level(t.coil, t.p2High);
    if (t.T2 == 0 || now - t.phaseStartMs >= t.T2) {
      t.phase = 0;
      t.phaseStartMs = now;
    }
  }
}

static void loop_trigger_mode(TimerConfig& t, unsigned long now) {
  uint8_t lvl = di_read(t.trigIndex) ? 1 : 0;
  bool fire = false;

  if      (t.trigEdge == TRIG_RISING  && t.lastTrigLevel == 0 && lvl == 1) fire = true;
  else if (t.trigEdge == TRIG_FALLING && t.lastTrigLevel == 1 && lvl == 0) fire = true;
  else if (t.trigEdge == TRIG_BOTH    && t.lastTrigLevel != lvl)           fire = true;

  t.lastTrigLevel = lvl;

  if (fire) {
    t.active       = 1;
    t.phase        = 0;
    t.phaseStartMs = now;
    t.alarm        = 0;
    t.alarmCode    = 0;

    // Sæt global timer-statusbit for denne timer
    uint8_t idx = (t.id > 0 && t.id <= 4) ? (t.id - 1) : 0;
    timers_flag_active(idx);
  }

  switch (t.subMode) {
    case TM_ONE_SHOT: loop_one_shot(t, now);    break;
    case TM_MONO:     loop_monostable(t, now);  break;
    case TM_ASTABLE:  loop_astable(t, now);     break;
  }
}


// ------------------------------------------------------
// Public API
// ------------------------------------------------------
void timers_init() {
  for (uint8_t i = 0; i < 4; i++) {
    memset(&timers[i], 0, sizeof(TimerConfig));
    timers[i].id       = i + 1;
    timers[i].mode     = TM_ONE_SHOT;
    timers[i].subMode  = TM_ONE_SHOT;
    timers[i].p2High   = 1;
    timers[i].trigEdge = TRIG_RISING;
    timers[i].alarm    = 0;
    timers[i].alarmCode= 0;
    timers[i].statusRoEnable = 0;
  }
}

void timers_loop() {
  unsigned long now = millis();

  for (uint8_t i = 0; i < 4; i++) {
    TimerConfig& t = timers[i];
    if (!t.enabled) continue;

    // Kerne-timerlogik
    switch (t.mode) {
      case TM_ONE_SHOT: loop_one_shot(t, now);    break;
      case TM_MONO:     loop_monostable(t, now);  break;
      case TM_ASTABLE:  loop_astable(t, now);     break;
      case TM_TRIGGER:  loop_trigger_mode(t, now);break;
    }

    // --- Alarm / timeout overvågning ---
    unsigned long total = t.T1 + t.T2 + t.T3;
    if (total == 0) total = 1000;        // fallback hvis alt er 0
    unsigned long timeout = total * 5UL; // 5x "normal" cyklustid

    if (t.active && (now - t.phaseStartMs > timeout)) {
      t.alarm    = 1;
      t.alarmCode= 1;  // timeout
      t.active   = 0;  // stop for sikkerhed
    }
  }
}

// kaldt fra Modbus/CLI ved coil-skrivning
void timers_onCoilWrite(uint16_t coilIdx, uint8_t value) {
  (void)value;
  unsigned long now = millis();

  for (uint8_t i = 0; i < 4; i++) {
    TimerConfig& t = timers[i];
    if (!t.enabled) continue;
    if (t.coil != coilIdx) continue;

    // ASTABLE der allerede kører skal ikke retrigges
    if (t.mode == TM_ASTABLE && t.active) continue;

    t.active        = 1;
        // --- marker denne timer som aktiv i global status-reg ---
    timers_flag_active(i);

    t.phase         = 0;
    t.phaseStartMs  = now;
    t.alarm         = 0;
    t.alarmCode     = 0;

    // --- marker denne timer som aktiv ---
    timers_flag_active(i);

    t.phase         = 0;
    t.phaseStartMs  = now;
    t.alarm         = 0;
    t.alarmCode     = 0;
  }
}
// bruges af Opgave 1b: tjek om coil ejes af timer
bool timers_hasCoil(uint16_t idx) {
  for (uint8_t i = 0; i < 4; i++) {
    if (!timers[i].enabled) continue;
    if (timers[i].coil == idx) return true;
  }
  return false;
}

void timers_disable_all() {
  for (uint8_t i = 0; i < 4; i++) {
    timers[i].enabled = 0;
    timers[i].active  = 0;
  }
}

bool timers_config_set(uint8_t id, const TimerConfig& src) {
  if (id < 1 || id > 4) return false;
  timers[id-1] = src;
  TimerConfig& t = timers[id-1];

  t.id          = id;
  t.active      = 0;
  t.phase       = 0;
  t.phaseStartMs= millis();
  t.lastTrigLevel = di_read(t.trigIndex) ? 1 : 0;
  t.alarm       = 0;
  t.alarmCode   = 0;

  // Check for GPIO conflicts on coil
  // If timer controls a coil, check if any GPIO pin is STATIC mapped to that coil
  if (t.enabled && t.coil < NUM_COILS) {
    for (uint8_t pin = 0; pin < NUM_GPIO; ++pin) {
      if (gpioToCoil[pin] == (int16_t)t.coil) {
        // Found a GPIO pin mapped to this coil
        Serial.print(F("⚠ GPIO-KONFLIKT: pin "));
        Serial.print(pin);
        Serial.print(F(" var STATIC mapped til coil "));
        Serial.print(t.coil);
        Serial.print(F(" – fjernet da timer "));
        Serial.print(t.id);
        Serial.println(F(" nu har kontrol (DYNAMIC)"));
        Serial.println(F("% Du skal opdatere din config-fil!"));
        gpioToCoil[pin] = -1;
      }
    }
  }

  // Check for GPIO conflicts on trigger input
  // If timer uses trigger input, check if any GPIO pin is STATIC mapped to that input
  if (t.enabled && t.mode == 4 && t.trigIndex < NUM_DISCRETE) {
    for (uint8_t pin = 0; pin < NUM_GPIO; ++pin) {
      if (gpioToInput[pin] == (int16_t)t.trigIndex) {
        // Found a GPIO pin mapped to this input
        Serial.print(F("⚠ GPIO-KONFLIKT: pin "));
        Serial.print(pin);
        Serial.print(F(" var STATIC mapped til input "));
        Serial.print(t.trigIndex);
        Serial.print(F(" – fjernet da timer "));
        Serial.print(t.id);
        Serial.println(F(" nu har kontrol (DYNAMIC)"));
        Serial.println(F("% Du skal opdatere din config-fil!"));
        gpioToInput[pin] = -1;
      }
    }
  }

  // Update timer status register if configured
  if (timerStatusRegIndex < NUM_REGS && t.statusRoEnable) {
    holdingRegs[timerStatusRegIndex] |= (1u << (t.id - 1));
  }

  return true;
}

bool timers_get(uint8_t id, TimerConfig& out) {
  if (id < 1 || id > 4) return false;
  out = timers[id-1];
  return true;
}

// ------------------------------------------------------
// CLI helpers
// ------------------------------------------------------
void timers_print_status() {
  Serial.println(F("------------------------------------------------------------------------------------------------------------------------------"));
  Serial.println(F("timer | mode | sub | P1 | P2 | P3 | T1(ms) | T2(ms) | T3(ms) | coil | trig | edge | act | ph | alarm | code | en"));
  Serial.println(F("------------------------------------------------------------------------------------------------------------------------------"));

  for (uint8_t i = 0; i < 4; i++) {
    const TimerConfig& t = timers[i];

    // Edge som tekst
    const char* edgeStr = "-";
    if (t.trigEdge == TRIG_RISING)  edgeStr = "rise";
    else if (t.trigEdge == TRIG_FALLING) edgeStr = "fall";
    else if (t.trigEdge == TRIG_BOTH)    edgeStr = "both";

    // P-levels
    const char* p1 = t.p1High ? "hi" : "lo";
    const char* p2 = t.p2High ? "hi" : "lo";
    const char* p3 = t.p3High ? "hi" : "lo";

    // State
    const char* act  = t.active ? "run" : "idle";
    const char* en   = t.enabled ? "on" : "off";

    char buf[200];
    snprintf(buf, sizeof(buf)," %u     | %u    | %u   | %-3s| %-3s| %-3s| %-7lu| %-7lu| %-7lu| %-5u| %-5u| %-5s| %-4s| %-2u | %-5u | %-5u | %-3s",
      t.id,
      t.mode,
      t.subMode,
      p1, p2, p3,
      (unsigned long)t.T1,
      (unsigned long)t.T2,
      (unsigned long)t.T3,
      t.coil,
      t.trigIndex,
      edgeStr,
      act,
      t.phase,
      t.alarm,
      t.alarmCode,
      en
    );
    Serial.println(buf);
  }

  Serial.println(F("------------------------------------------------------------------------------------------------------------------------------"));
}

void timers_clear_alarms() {
  for (uint8_t i = 0; i < 4; i++) {
    timers[i].alarm    = 0;
    timers[i].alarmCode= 0;
  }
  Serial.println(F("All timer alarms cleared."));
}
