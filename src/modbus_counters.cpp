// ============================================================================
//  Filnavn : modbus_counters.cpp
//  Projekt  : Modbus RTU Server / CLI
//  Version  : v3.3.0 (2025-11-11)
//  Forfatter: JanG at modbus_slave@laces.dk
//  Formål   : CounterEngine v4 med 4 uafhængige input-tællere (hybrid HW/SW).
//             - HW mode: Interrupt-baseret ved hardware timer (Timer1/3/4/5)
//             - SW mode: Software polling med edge-detektering
//             - Prescaler (SW) eller prescale/external clock (HW)
//             - Op/ned-tælling (direction)
//             - BitWidth-maskning (8/16/32/64)
//             - Overflow/auto-reset til startValue
//             - Soft-control via controlReg (bit0=reset,1=start,2=stop)
//             - Skaleret udlæsning til holdingRegs med float scale
//             - Debounce pr. kanal (SW mode)
//  Ændringer:
//    - v3.3.0: Hybrid HW/SW counter engine
//              HW mode: interrupt-driven, deterministic frequency
//              SW mode: legacy polling-based
//    - v3.2.0: Frequency measurement, raw register, consistent naming
// ============================================================================

#include "modbus_counters.h"
#include "modbus_counters_hw.h"
#include "modbus_core.h"
#include <string.h>
#include <math.h>


// Globalt array
CounterConfig counters[4];

// ============================================================================
// Global control arrays (individuelt pr. counter)
// ============================================================================
uint8_t counterResetOnReadEnable[4] = {0, 0, 0, 0};  // counter 1..4 (index 0..3)
uint8_t counterAutoStartEnable[4]   = {0, 0, 0, 0};  // auto-start ved load/reboot

// ============================================================================
//  Interne helpers
// ============================================================================

static inline bool di_read(uint16_t idx) {
  if (idx >= NUM_DISCRETE) return false;
  return bitReadArray(discreteInputs, idx);
}

uint8_t sanitizeBitWidth(uint8_t bw) {
  switch (bw) {
    case 8: case 16: case 32: case 64: return bw;
    default: return 32;
  }
}

uint64_t maskToBitWidth(uint64_t v, uint8_t bw) {
  switch (bw) {
    case 8:  return v & 0xFFULL;
    case 16: return v & 0xFFFFULL;
    case 32: return v & 0xFFFFFFFFULL;
    case 64: default: return v;
  }
}

static uint8_t sanitizeDirection(uint8_t d) {
  return (d == CNT_DIR_DOWN) ? CNT_DIR_DOWN : CNT_DIR_UP;
}

static uint8_t sanitizeEdge(uint8_t e) {
  if (e == CNT_EDGE_RISING || e == CNT_EDGE_FALLING || e == CNT_EDGE_BOTH)
    return e;
  return CNT_EDGE_RISING;
}

static uint8_t sanitizePrescaler(uint8_t p) {
  if (p == 0) return 1;
  if (p > 255) return 255;
  return p;
}

// Skaleret værdi -> holdingRegs[regIndex..] afhængig af bitWidth
static void store_value_to_regs(uint8_t idx) {
  if (idx >= 4) return;
  CounterConfig& c = counters[idx];

  if (!c.enabled) return;
  if (c.regIndex >= NUM_REGS) return;

  uint8_t bw = sanitizeBitWidth(c.bitWidth);

  // Skaleret værdi (float scale anvendes kun ved udlæsning)
  double scale = (c.scale > 0.0f) ? (double)c.scale : 1.0;
  double dVal  = (double)c.counterValue * scale;
  if (dVal < 0.0) dVal = 0.0;

  // Begræns til valgt bitWidth
  double maxD;
  if (bw == 64)      maxD = (double)0xFFFFFFFFFFFFFFFFULL;
  else if (bw == 32) maxD = (double)0xFFFFFFFFUL;
  else if (bw == 16) maxD = (double)0xFFFFU;
  else               maxD = (double)0xFFU;

  if (dVal > maxD) dVal = maxD;

  uint64_t u = (uint64_t)(dVal + 0.5);   // afrund til nærmeste
  u = maskToBitWidth(u, bw);

  uint8_t words = 1;
  if (bw == 32) words = 2;
  else if (bw == 64) words = 4;

  // Skriv skaleret værdi til regIndex .. afhængig af bitWidth
  if ((uint32_t)c.regIndex + words > NUM_REGS) return;

  uint16_t base = c.regIndex;
  for (uint8_t w = 0; w < words; ++w) {
    holdingRegs[base + w] = (uint16_t)((u >> (16 * w)) & 0xFFFF);
  }

  // Skriv rå, u-skaleret tællerværdi til rawReg (hvis konfigureret)
  // Layout baseret på bitWidth:
  //   8/16 bit : 1 word
  //   32 bit   : 2 words
  //   64 bit   : 4 words
  uint16_t rawBase = 0;
  bool writeRaw = false;

  if (c.rawReg > 0 && c.rawReg < NUM_REGS) {
    // Brug konfigurerbar rawReg
    rawBase = c.rawReg;
    writeRaw = true;
  } else if (c.regIndex > 0) {
    // Fallback til gammel metode: regIndex + 4
    rawBase = c.regIndex + 4;
    writeRaw = true;
  }

  if (writeRaw) {
    uint8_t rawWords = words;
    if ((uint32_t)rawBase + rawWords <= NUM_REGS) {
      uint64_t raw = maskToBitWidth((uint64_t)c.counterValue, bw);
      for (uint8_t w = 0; w < rawWords; ++w) {
        holdingRegs[rawBase + w] = (uint16_t)((raw >> (16 * w)) & 0xFFFF);
      }
    }
  }
}


// Håndter controlReg-kommandoer (bit0=reset, bit1=start, bit2=stop)
static void handle_control(CounterConfig& c) {
  if (c.controlReg >= NUM_REGS) return;

  uint16_t val = holdingRegs[c.controlReg];
  uint16_t newVal = val;

  uint8_t bw = sanitizeBitWidth(c.bitWidth);

  // bit0: reset
  if (val & 0x0001) {
    uint64_t sv = c.startValue;
    sv = maskToBitWidth(sv, bw);
    c.counterValue = sv;
    c.edgeCount = 0;
    c.overflowFlag = 0;

    // Reset frequency tracking
    c.lastFreqCalcMs = 0;
    c.lastCountForFreq = 0;
    c.currentFreqHz = 0;
    if (c.freqReg > 0 && c.freqReg < NUM_REGS) {
      holdingRegs[c.freqReg] = 0;
    }

    // Reset HW timer if in HW mode
    if (c.hwMode != 0) {
      // Map hwMode to hw_counter_id
      uint8_t hw_id = 0;
      if (c.hwMode == 1) hw_id = 1;       // Timer1
      else if (c.hwMode == 3) hw_id = 2;  // Timer3
      else if (c.hwMode == 4) hw_id = 3;  // Timer4
      else if (c.hwMode == 5) hw_id = 4;  // Timer5

      if (hw_id != 0) {
        hw_counter_reset(hw_id);
      }
    }

    if (c.overflowReg < NUM_REGS) holdingRegs[c.overflowReg] = 0;

    newVal &= ~0x0001;
  }

  // bit1: start
  if (val & 0x0002) {
    c.running = 1;
    newVal &= ~0x0002;
  }

  // bit2: stop
  if (val & 0x0004) {
    c.running = 0;
    newVal &= ~0x0004;
  }

  // bit3: reset-on-read mode (bliver stående)
  // Når sat til 1, vil tælleren automatisk nulstilles til startValue hver gang
  // dens værdi aflæses via Modbus (store_value_to_regs).  Bit3 bliver stående
  // i controlReg indtil den manuelt ændres til 0.
  if (val & 0x0008) {
    // behold bit3 – den nulstilles ikke automatisk
  } else {
    // bit3 er 0 -> funktionen er inaktiv
    newVal &= ~0x0008;
  }

  if (newVal != val) {
    holdingRegs[c.controlReg] = newVal;
  }
}

// ============================================================================
//  Init / loop
// ============================================================================

void counters_init() {
  for (uint8_t i = 0; i < 4; ++i) {
    CounterConfig& c = counters[i];
    memset(&c, 0, sizeof(CounterConfig));
    c.id        = i + 1;
    c.enabled   = 0;
    c.hwMode    = 0;           // Default: SW mode (NEW v3.3.0)
    c.edgeMode  = CNT_EDGE_RISING;
    c.direction = CNT_DIR_UP;
    c.bitWidth  = 32;
    c.prescaler = 1;
    c.inputIndex    = 0;
    c.regIndex      = 0;
    c.rawReg        = 0;
    c.freqReg       = 0;
    c.controlReg    = 0;
    c.overflowReg   = 0;
    c.startValue    = 0;
    c.scale         = 1.0f;
    c.counterValue  = 0;

    // Auto-start counters der er enablet i config
    c.running       = 0;
    c.overflowFlag  = 0;
    c.lastLevel     = 0;
    c.edgeCount     = 0;

    // Debounce defaults: disabled (0 ms)
    c.debounceEnable = 0;
    c.debounceTimeMs = 0;
    c.lastEdgeMs     = 0;

    // Frekvens-måling defaults
    c.lastCountForFreq = 0;
    c.lastFreqCalcMs   = 0;
    c.currentFreqHz    = 0;
  }
}

void counters_loop() {
  for (uint8_t idx = 0; idx < 4; ++idx) {
    CounterConfig& c = counters[idx];

    if (!c.enabled) {
      // Reflect overflow flag and value even when disabled
      if (c.overflowReg < NUM_REGS) {
        holdingRegs[c.overflowReg] = c.overflowFlag ? 1 : 0;
      }
      store_value_to_regs(idx);
      continue;
    }

    // ====================================================================
    // HW MODE (v3.3.0): Interrupt-driven, deterministic
    // ====================================================================
    if (c.hwMode != 0) {
      // Handle control register commands
      handle_control(c);

      if (!c.running) {
        // Not running - just reflect status
        if (c.overflowReg < NUM_REGS) {
          holdingRegs[c.overflowReg] = c.overflowFlag ? 1 : 0;
        }
        store_value_to_regs(idx);
        continue;
      }

      // Map hwMode to hw_counter_id (hw_counter functions use 1=T1, 2=T3, 3=T4, 4=T5)
      uint8_t hw_id = 0;
      if (c.hwMode == 1) hw_id = 1;       // Timer1
      else if (c.hwMode == 3) hw_id = 2;  // Timer3
      else if (c.hwMode == 4) hw_id = 3;  // Timer4
      else if (c.hwMode == 5) hw_id = 4;  // Timer5

      if (hw_id == 0) {
        // Invalid hwMode - skip this counter
        continue;
      }

      // Read HW counter value and update SW representation
      uint32_t hwValue = hw_counter_get_value(hw_id);
      c.counterValue = (uint64_t)hwValue;  // Store for output scaling

      // Update frequency measurement (interrupt-based, deterministic)
      if (c.freqReg > 0 && c.freqReg < NUM_REGS) {
        hw_counter_update_frequency(c.freqReg, hw_id);
      }

      // Reflect outputs
      if (c.overflowReg < NUM_REGS) {
        holdingRegs[c.overflowReg] = c.overflowFlag ? 1 : 0;
      }
      store_value_to_regs(idx);
      continue;
    }

    // ====================================================================
    // SW MODE (v3.2.0): Software polling, legacy behavior
    // ====================================================================

    // Handle control register commands
    handle_control(c);

    // If not running -> track lastLevel, but don't count
    bool lvl = di_read(c.inputIndex);
    if (!c.running) {
      c.lastLevel = lvl ? 1 : 0;
      // Reflect overflow flag only if set, keep until reset-bit clears it
      if (c.overflowReg < NUM_REGS) {
        if (c.overflowFlag)
          holdingRegs[c.overflowReg] = 1;
      }
      store_value_to_regs(idx);
      continue;
    }

    // Edge-detection
    bool fire = false;
    uint8_t edge = sanitizeEdge(c.edgeMode);
    uint8_t last = c.lastLevel;
    uint8_t now  = lvl ? 1 : 0;

    if      (edge == CNT_EDGE_RISING  && last == 0 && now == 1) fire = true;
    else if (edge == CNT_EDGE_FALLING && last == 1 && now == 0) fire = true;
    else if (edge == CNT_EDGE_BOTH    && last != now)           fire = true;

    c.lastLevel = now;

    // Debounce: if enabled, filter edges that come too fast
    if (fire && c.debounceEnable && c.debounceTimeMs > 0) {
      unsigned long nowMs = millis();
      unsigned long dt = nowMs - c.lastEdgeMs;
      if (dt < c.debounceTimeMs) {
        // Ignore this edge as "noise"
        fire = false;
      } else {
        c.lastEdgeMs = nowMs;
      }
    } else if (fire && (!c.debounceEnable || c.debounceTimeMs == 0)) {
      // Without debounce: just update lastEdgeMs for reference
      c.lastEdgeMs = millis();
    }

    if (!fire) {
      if (c.overflowReg < NUM_REGS) {
        holdingRegs[c.overflowReg] = c.overflowFlag ? 1 : 0;
      }
      store_value_to_regs(idx);
      continue;
    }

    // Prescaler
    uint8_t pre = sanitizePrescaler(c.prescaler);
    c.edgeCount++;
    if (c.edgeCount < pre) {
      if (c.overflowReg < NUM_REGS) {
        holdingRegs[c.overflowReg] = c.overflowFlag ? 1 : 0;
      }
      store_value_to_regs(idx);
      continue;
    }
    c.edgeCount = 0;

    // Count step
    uint8_t bw = sanitizeBitWidth(c.bitWidth);
    uint64_t maxVal = (bw == 64) ? 0xFFFFFFFFFFFFFFFFULL : ((1ULL << bw) - 1);
    bool overflow = false;

    uint8_t dir = sanitizeDirection(c.direction);
    if (dir == CNT_DIR_DOWN) {
      if (c.counterValue == 0) {
        overflow = true;
      } else {
        c.counterValue--;
      }
    } else {
      if (c.counterValue >= maxVal) {
        overflow = true;
      } else {
        c.counterValue++;
      }
    }

    if (overflow) {
      c.overflowFlag = 1;
      if (c.overflowReg < NUM_REGS) {
        holdingRegs[c.overflowReg] = 1;
      }
      // Auto-reset to startValue (masked to bitWidth)
      uint64_t sv = c.startValue;
      sv = maskToBitWidth(sv, bw);
      c.counterValue = sv;

      // Reset frequency tracking on overflow
      c.lastFreqCalcMs = 0;
      c.lastCountForFreq = 0;
      c.currentFreqHz = 0;
      if (c.freqReg > 0 && c.freqReg < NUM_REGS) {
        holdingRegs[c.freqReg] = 0;
      }
    }

    // Reflect overflow flag and scaled value
    if (c.overflowReg < NUM_REGS) {
      holdingRegs[c.overflowReg] = c.overflowFlag ? 1 : 0;
    }

    store_value_to_regs(idx);

    // Frequency calculation (every second) - SW mode only
    if (c.freqReg > 0 && c.freqReg < NUM_REGS) {
      unsigned long nowMs = millis();

      // Initialize first time
      if (c.lastFreqCalcMs == 0) {
        c.lastFreqCalcMs = nowMs;
        c.lastCountForFreq = c.counterValue;
        c.currentFreqHz = 0;
        holdingRegs[c.freqReg] = 0;
      }

      // Calculate frequency every second (1000-2000 ms window for stability)
      unsigned long deltaTimeMs = nowMs - c.lastFreqCalcMs;
      if (deltaTimeMs >= 1000 && deltaTimeMs <= 2000) {
        uint64_t deltaCount = 0;
        bool validDelta = true;

        if (c.counterValue >= c.lastCountForFreq) {
          deltaCount = c.counterValue - c.lastCountForFreq;
        } else {
          // Handle overflow wrap-around
          uint8_t bw = sanitizeBitWidth(c.bitWidth);
          uint64_t maxVal = (bw == 64) ? 0xFFFFFFFFFFFFFFFFULL : ((1ULL << bw) - 1);
          deltaCount = (maxVal - c.lastCountForFreq) + c.counterValue + 1;

          // Sanity check: if deltaCount is unreasonably large, skip calculation
          // (probably counter reset or timing error)
          if (deltaCount > maxVal / 2) {
            validDelta = false;
          }
        }

        // Calculate only if deltaCount is reasonable (max 100kHz @ 1sec sample)
        if (validDelta && deltaCount <= 100000UL) {
          // Calculate Hz: pulses per second
          // Use 32-bit for intermediate result to avoid overflow
          uint32_t freqCalc = (uint32_t)((deltaCount * 1000UL) / deltaTimeMs);

          // Clamp to reasonable range (0-20000 Hz)
          if (freqCalc > 20000UL) freqCalc = 20000UL;

          c.currentFreqHz = (uint16_t)freqCalc;
        } else {
          // Invalid delta - keep last valid value
          // (avoid writing garbage data)
        }

        // Write to freqReg (atomic write for Modbus consistency)
        holdingRegs[c.freqReg] = c.currentFreqHz;

        // Update tracking
        c.lastCountForFreq = c.counterValue;
        c.lastFreqCalcMs = nowMs;
      } else if (deltaTimeMs > 5000) {
        // If more than 5 seconds since last update, reset tracking
        // (avoid accumulation of errors on long pauses)
        c.lastFreqCalcMs = nowMs;
        c.lastCountForFreq = c.counterValue;
        c.currentFreqHz = 0;
        holdingRegs[c.freqReg] = 0;
      }
    }
  }
}

// ============================================================================
//  Public helpers
// ============================================================================

bool counters_config_set(uint8_t id, const CounterConfig& src) {
  if (id < 1 || id > 4) return false;
  uint8_t idx = id - 1;

  // Check if HW-mode is being disabled or changed
  // If so, remove any GPIO-input mapping for the old HW pin
  CounterConfig& oldC = counters[idx];
  if (oldC.enabled && oldC.hwMode != 0 && (!src.enabled || src.hwMode == 0)) {
    // HW-mode being disabled - remove GPIO mapping
    uint8_t oldPin = 0;
    if (oldC.hwMode == 1) oldPin = 5;
    else if (oldC.hwMode == 3) oldPin = 47;
    else if (oldC.hwMode == 4) oldPin = 6;
    else if (oldC.hwMode == 5) oldPin = 46;

    if (oldPin > 0 && gpioToInput[oldPin] == (int16_t)oldC.inputIndex) {
      gpioToInput[oldPin] = -1;  // Clear mapping
    }
  }

  CounterConfig c = src;

  c.controlReg    = src.controlReg;
  c.id        = id;
  c.enabled   = (c.enabled ? 1 : 0);
  c.edgeMode  = sanitizeEdge(c.edgeMode);
  c.direction = sanitizeDirection(c.direction);
  c.bitWidth  = sanitizeBitWidth(c.bitWidth);
  c.prescaler = sanitizePrescaler(c.prescaler);

  if (c.inputIndex >= NUM_DISCRETE) c.inputIndex = 0;

  if (isnan(c.scale) || c.scale <= 0.0f || c.scale > 100000.0f) c.scale = 1.0f;


  // --- Debounce-sanitizing (fix toggle ON/OFF issue) ---
  // --- Debounce handling: tillad toggle uden at miste tid ---
if (c.debounceEnable) {
  if (c.debounceTimeMs < 1)  c.debounceTimeMs = 10;    // default minimum
  if (c.debounceTimeMs > 60000) c.debounceTimeMs = 60000;
} else {
  // OFF: behold debounceTimeMs, men sæt enable=0
  c.debounceEnable = 0;
}
c.lastEdgeMs = 0;

  // Auto-start counters baseret på counterAutoStartEnable array
  c.running       = (c.enabled && counterAutoStartEnable[idx]) ? 1 : 0;
  c.overflowFlag  = 0;
  c.edgeCount     = 0;

  // Init counterValue fra startValue (maske til bitWidth)
  uint64_t sv = c.startValue;
  sv = maskToBitWidth(sv, c.bitWidth);
  c.counterValue = sv;

  // Sync lastLevel til aktuel input
  c.lastLevel = di_read(c.inputIndex) ? 1 : 0;

  counters[idx] = c;

  // Initialize HW timer if in HW mode
  if (c.enabled && c.hwMode != 0) {
    // Map hwMode to hw_counter_id (hw_counter functions use 1=T1, 2=T3, 3=T4, 4=T5)
    uint8_t hw_id = 0;
    uint8_t pin = 0;

    if (c.hwMode == 1) { hw_id = 1; pin = 5; }       // Timer1 = pin 5 (D5/T1)
    else if (c.hwMode == 3) { hw_id = 2; pin = 47; } // Timer3 = pin 47 (D47/T3)
    else if (c.hwMode == 4) { hw_id = 3; pin = 6; }  // Timer4 = pin 6 (D6/T4)
    else if (c.hwMode == 5) { hw_id = 4; pin = 46; } // Timer5 = pin 46 (D46/T5)

    if (hw_id != 0) {
      // Check for GPIO conflicts and remove STATIC mappings
      if (pin > 0) {
        gpio_handle_dynamic_conflict(pin);
      }

      // Auto-map GPIO pin to discrete input for HW-mode counters
      // This enables GPIO-polling to read the physical pin and write to discrete input
      if (c.inputIndex < NUM_DISCRETE) {
        gpioToInput[pin] = (int16_t)c.inputIndex;
        pinMode(pin, INPUT);
      }

      // Initialize HW timer with prescaler mode
      // prescaler field is used as mode for HW: 1=external clock, 2-5=internal prescale
      hw_counter_init(hw_id, c.prescaler);
    }
  }

  // Nulstil overflowReg & skriv initial værdi
  if (c.overflowReg < NUM_REGS) {
    holdingRegs[c.overflowReg] = 0;
  }
  store_value_to_regs(idx);

  return true;
}

bool counters_get(uint8_t id, CounterConfig& out) {
  if (id < 1 || id > 4) return false;
  out = counters[id - 1];
  return true;
}

void counters_reset(uint8_t id) {
  if (id < 1 || id > 4) return;
  uint8_t idx = id - 1;
  CounterConfig& c = counters[idx];

  uint8_t bw = sanitizeBitWidth(c.bitWidth);
  uint64_t sv = c.startValue;
  sv = maskToBitWidth(sv, bw);

  c.counterValue = sv;
  c.edgeCount    = 0;
  c.overflowFlag = 0;

  // Reset frequency tracking (SW mode)
  c.lastFreqCalcMs = 0;
  c.lastCountForFreq = 0;
  c.currentFreqHz = 0;
  if (c.freqReg > 0 && c.freqReg < NUM_REGS) {
    holdingRegs[c.freqReg] = 0;
  }

  // Reset HW timer if in HW mode
  if (c.hwMode != 0) {
    uint8_t hw_id = 0;
    if (c.hwMode == 1) hw_id = 1;       // Timer1
    else if (c.hwMode == 3) hw_id = 2;  // Timer3
    else if (c.hwMode == 4) hw_id = 3;  // Timer4
    else if (c.hwMode == 5) hw_id = 4;  // Timer5

    if (hw_id != 0) {
      hw_counter_reset(hw_id);
    }
  }

  if (c.overflowReg < NUM_REGS) {
    holdingRegs[c.overflowReg] = 0;
  }
  store_value_to_regs(idx);
}

void counters_clear_all() {
  for (uint8_t id = 1; id <= 4; ++id) {
    counters_reset(id);  // Use common reset function for consistency
  }
}

// ============================================================================
//  Debug / status print til CLI (show counters)
// ============================================================================

void counters_print_status() {
  Serial.println(F(""));
  Serial.println(F("----------------------------------------------------------------------------------------------------------------------------------------------"));
  Serial.println(F("co = count-on, sv = startValue, res = resolution, ps = prescaler, ir = index-reg, rr = raw-reg, fr = freq-reg"));
  Serial.println(F("or = overload-reg, cr = ctrl-reg, dir = direction, sf = scaleFloat, dis = input-dis, d = debounce, dt = debounce-ms"));
  Serial.println(F("hw = HW/SW mode (SW|T1|T3|T4|T5), pin = GPIO pin (actual hardware pin), hz = measured freq (Hz)"));
  Serial.println(F("value = scaled value, raw = raw counter value"));
  Serial.println(F("----------------------------------------------------------------------------------------------------------------------------------------------"));
  Serial.println(F("counter | mode| hw  | pin  | co     | sv       | res | ps   | ir   | rr   | fr   | or   | cr   | dir   | sf     | d   | dt   | hz    | value     | raw"));

  char buf[32];

  for (uint8_t i = 0; i < 4; ++i) {
    const CounterConfig& c = counters[i];
    uint8_t mode = c.enabled ? 1 : 0;
    const char* coStr = (c.edgeMode==CNT_EDGE_FALLING)?"falling":(c.edgeMode==CNT_EDGE_BOTH)?"both":"rising";
    const char* dirStr = (c.direction==CNT_DIR_DOWN)?"down":"up";
    const char* dStr = c.debounceEnable?"on":"off";

    // HW mode display: SW, T1, T3, T4, T5
    const char* hwStr = "SW";
    if (c.hwMode == 1) hwStr = "T1";
    else if (c.hwMode == 3) hwStr = "T3";
    else if (c.hwMode == 4) hwStr = "T4";
    else if (c.hwMode == 5) hwStr = "T5";

    // Vis skaleret og rå værdi separat
    unsigned long val  = (unsigned long)((double)c.counterValue * (double)c.scale);
    unsigned long raw  = (unsigned long)(c.counterValue & 0xFFFFFFFFULL);

    // kolonne-for-kolonne print med fast bredde
    Serial.print(' ');
    sprintf(buf, "%-7d| ", i+1);       Serial.print(buf);
    sprintf(buf, "%-4d| ", mode);      Serial.print(buf);
    sprintf(buf, "%-4s| ", hwStr);     Serial.print(buf);  // HW mode: SW|T1|T3|T4|T5

    // Pin display: show actual GPIO pin
    if (c.hwMode != 0) {
      // HW mode: show GPIO pin based on timer
      uint8_t gpioPin = 0;
      if (c.hwMode == 1) gpioPin = 5;
      else if (c.hwMode == 3) gpioPin = 47;
      else if (c.hwMode == 4) gpioPin = 6;
      else if (c.hwMode == 5) gpioPin = 46;
      sprintf(buf, "%-5d| ", gpioPin);
    } else {
      // SW mode: find GPIO pin mapped to this discrete input
      int16_t gpioPin = -1;
      for (uint8_t pin = 0; pin < NUM_GPIO; pin++) {
        if (gpioToInput[pin] == (int16_t)c.inputIndex) {
          gpioPin = pin;
          break;
        }
      }
      if (gpioPin != -1) {
        sprintf(buf, "%-5d| ", gpioPin);
      } else {
        sprintf(buf, "%-5s| ", "-");  // No GPIO mapping
      }
    }
    Serial.print(buf);
    sprintf(buf, "%-7s| ", coStr);     Serial.print(buf);
    sprintf(buf, "%-9lu| ", (unsigned long)c.startValue); Serial.print(buf);
    sprintf(buf, "%-4d| ", c.bitWidth); Serial.print(buf);
    sprintf(buf, "%-5d| ", c.prescaler); Serial.print(buf);
    sprintf(buf, "%-5d| ", c.regIndex); Serial.print(buf);       // ir = index-reg
    sprintf(buf, "%-5d| ", c.rawReg); Serial.print(buf);         // rr = raw-reg
    sprintf(buf, "%-5d| ", c.freqReg); Serial.print(buf);        // fr = freq-reg
    sprintf(buf, "%-5d| ", c.overflowReg); Serial.print(buf);    // or = overload-reg
    sprintf(buf, "%-5d| ", c.controlReg); Serial.print(buf);     // cr = ctrl-reg
    sprintf(buf, "%-6s| ", dirStr); Serial.print(buf);

    // Format scale float manually (sprintf %f not supported on all Arduino boards)
    int scaleInt = (int)c.scale;
    int scaleDec = (int)((c.scale - scaleInt) * 1000);
    if (scaleDec < 0) scaleDec = -scaleDec;  // absolute value for decimal part
    sprintf(buf, "%d.%03d  | ", scaleInt, scaleDec);
    Serial.print(buf);

    sprintf(buf, "%-4s| ", dStr); Serial.print(buf);
    sprintf(buf, "%-5d| ", c.debounceTimeMs); Serial.print(buf);
    sprintf(buf, "%-6d| ", c.currentFreqHz); Serial.print(buf);  // hz = measured frequency
    sprintf(buf, "%-10lu| ", val); Serial.print(buf);
    sprintf(buf, "%-10lu", raw); Serial.println(buf);
  }
}

