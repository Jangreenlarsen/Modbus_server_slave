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
#include "modbus_counters_sw_int.h"
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

// REMOVED: sanitizePrescaler() - no longer used after edgeCount removal
// Prescaler validation now happens only in CLI parser and sanitizeHWPrescaler()

// Sanitize prescaler for SW/SW-ISR mode - only 1, 4, 16, 64, 256, 1024 allowed
// These match the HW Timer5 internal prescale values for consistency
static uint16_t sanitizePrescaler_SW(uint16_t p) {
  // Valid prescaler values for SW/SW-ISR modes
  if (p == 1)    return 1;     // No prescale (every edge counts)
  if (p == 4)    return 4;     // Divide by 4
  if (p == 16)   return 16;    // Divide by 16
  if (p == 64)   return 64;    // Divide by 64
  if (p == 256)  return 256;   // Divide by 256
  if (p == 1024) return 1024;  // Divide by 1024
  // If invalid prescaler, default to 1 (no prescale)
  return 1;
}

// Validate prescaler for HW mode (software prescaler implementation)
// All standard values (1, 4, 8, 16, 64, 256, 1024) are supported
// Returns the validated prescaler value
static uint16_t sanitizeHWPrescaler(uint16_t p) {
  // Valid prescaler values for HW mode (software prescaler)
  // All standard values are now supported since prescaler is implemented in software
  if (p == 1) return 1;
  if (p == 4) return 4;
  if (p == 8) return 8;
  if (p == 16) return 16;
  if (p == 64) return 64;
  if (p == 256) return 256;
  if (p == 1024) return 1024;
  // If invalid prescaler, default to 1
  return 1;
}

// Convert prescaler value to HW Timer5 mode for hw_counter_init()
// ALWAYS returns mode 1 (external clock) - prescaler is handled in software
// Input: prescaler (ignored - kept for backward compatibility)
// Output: mode 1 (external clock, TCCR5B = 0x07)
static uint8_t hwPrescalerToMode(uint16_t prescaler) {
  // ALWAYS return mode 1 (external clock) for HW counters
  // ATmega2560 Timer5 hardware limitation: Cannot use prescaler with external clock
  // Prescaler is now implemented in SOFTWARE (see store_value_to_regs)
  (void)prescaler;  // Unused parameter
  return 1;  // Mode 1 = external clock (TCCR5B = 0x07)
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
  //
  // VIGTIGT: Raw register definition (KONSISTENT for HW og SW mode):
  //   - counterValue = ALLE edges/pulses talt (fuld præcision)
  //   - raw register = counterValue / prescaler (reduceret størrelse)
  //   - value register = counterValue × scale (fuld præcision output)
  //
  // Dette gør SW og HW mode konsistente:
  //   HW mode: Hardware tæller alle pulses → raw divideres ved output
  //   SW mode: Software tæller alle edges → raw divideres ved output
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
      uint64_t raw = (uint64_t)c.counterValue;

      // BÅDE HW og SW mode: divider med prescaler for raw register
      // Dette giver konsistent adfærd mellem HW og SW mode
      if (c.prescaler > 1) {
        raw = raw / c.prescaler;
      }

      raw = maskToBitWidth(raw, bw);
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
    // Only Timer5 (hwMode=5) is supported on Arduino Mega 2560
    if (c.hwMode == 5) {
      uint8_t hw_id = 4;  // Timer5
      hw_counter_reset(hw_id);
    }

    if (c.overflowReg < NUM_REGS) holdingRegs[c.overflowReg] = 0;

    newVal &= ~0x0001;
  }

  // bit1: start
  if (val & 0x0002) {
    c.running = 1;
    // Attach interrupt if in SW mode with interrupt pin configured
    if (c.hwMode == 0 && c.interruptPin > 0) {
      // Find counter ID from address of config struct
      uint8_t cid = 0;
      for (uint8_t i = 0; i < 4; i++) {
        if (&counters[i] == &c) {
          cid = i + 1;
          break;
        }
      }
      if (cid > 0) {
        sw_counter_attach_interrupt(cid, c.interruptPin);
      }
    }
    newVal &= ~0x0002;
  }

  // bit2: stop
  if (val & 0x0004) {
    c.running = 0;
    // Detach interrupt if in SW mode with interrupt pin configured
    if (c.hwMode == 0 && c.interruptPin > 0) {
      uint8_t cid = 0;
      for (uint8_t i = 0; i < 4; i++) {
        if (&counters[i] == &c) {
          cid = i + 1;
          break;
        }
      }
      if (cid > 0) {
        sw_counter_detach_interrupt(cid);
      }
    }
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
  // Initialize HW counter extension registers to 0 FIRST
  // Timer5 only (other timers not accessible on Arduino Mega)
  hwCounter5Extend = 0;
  hwOverflowCount = 0;

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
    c.interruptPin  = 0;   // 0 = polling mode (no interrupt pin)
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

      // Map hwMode to hw_counter_id
      // Only hwMode=5 (Timer5) is supported on Arduino Mega 2560
      if (c.hwMode != 5) {
        // Invalid hwMode - skip this counter
        continue;
      }
      uint8_t hw_id = 4;  // Timer5 only

      // Read HW counter value (direct pulse count from hardware)
      uint32_t hwValue = hw_counter_get_value(hw_id);

      // IMPORTANT: Hardware now ALWAYS uses external clock mode (counts ALL pulses).
      // Prescaler is implemented in SOFTWARE by dividing for raw register.
      // See hw_counter_init() in modbus_counters_hw.cpp for details.
      //
      // Store undivided hardware value:
      //   - counterValue = hwValue (all pulses counted)
      //   - raw register = hwValue / prescaler (software division in store_value_to_regs)
      //   - value register = hwValue × scale (scaled output)
      //   - frequency = actual Hz (no prescaler compensation needed)
      c.counterValue = (uint64_t)hwValue;

      // Update frequency measurement using dedicated function
      // This handles first-time initialization correctly
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
    // SW MODE (v3.3.0): Software polling OR interrupt-driven
    // ====================================================================

    // Handle control register commands
    handle_control(c);

    // If counter has interrupt pin attached, skip polling
    // ISRs will handle edge detection and counting
    if (c.interruptPin > 0) {
      // Interrupt-driven mode: counter updates via ISR
      // Just reflect outputs to Modbus registers
      if (c.overflowReg < NUM_REGS) {
        holdingRegs[c.overflowReg] = c.overflowFlag ? 1 : 0;
      }
      store_value_to_regs(idx);

      // Frequency calculation for SW interrupt mode (similar to polling)
      if (c.freqReg > 0 && c.freqReg < NUM_REGS && c.running) {
        unsigned long nowMs = millis();

        // Initialize first time
        if (c.lastFreqCalcMs == 0) {
          c.lastFreqCalcMs = nowMs;
          c.lastCountForFreq = c.counterValue;
          c.currentFreqHz = 0;
          holdingRegs[c.freqReg] = 0;
        } else {
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
              if (deltaCount > maxVal / 2) {
                validDelta = false;
              }
            }

            if (validDelta && deltaCount <= 100000UL) {
              uint32_t freqCalc = (uint32_t)((deltaCount * 1000UL) / deltaTimeMs);
              if (freqCalc > 20000UL) freqCalc = 20000UL;
              c.currentFreqHz = (uint16_t)freqCalc;
            }

            holdingRegs[c.freqReg] = c.currentFreqHz;
            c.lastCountForFreq = c.counterValue;
            c.lastFreqCalcMs = nowMs;
          } else if (deltaTimeMs > 5000) {
            c.lastFreqCalcMs = nowMs;
            c.lastCountForFreq = c.counterValue;
            c.currentFreqHz = 0;
            holdingRegs[c.freqReg] = 0;
          }
        }
      }
      continue;
    }

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

    // REMOVED: SW mode prescaler via edgeCount (now handled in store_value_to_regs)
    // SW mode now counts ALL edges, just like HW mode
    // Prescaler division happens only at output (raw register)

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

  // Check if HW-mode is being disabled or changed to a different pin
  // If so, remove any GPIO-input mapping for the old HW pin
  CounterConfig& oldC = counters[idx];
  if (oldC.enabled && oldC.hwMode != 0 && (!src.enabled || src.hwMode == 0 || oldC.hwMode != src.hwMode)) {
    // HW-mode being disabled or changed to different timer
    // Only remove if the new config is not HW-mode OR uses a different timer pin
    uint8_t oldPin = 0;
    if (oldC.hwMode == 1) oldPin = 5;
    else if (oldC.hwMode == 3) oldPin = 47;
    else if (oldC.hwMode == 4) oldPin = 6;
    else if (oldC.hwMode == 5) oldPin = 2;

    // Only clear if: old pin was mapped AND (not transitioning to same pin with HW-mode)
    if (oldPin > 0 && gpioToInput[oldPin] == (int16_t)oldC.inputIndex) {
      // Check if new config will keep the same HW pin
      uint8_t newPin = 0;
      if (src.hwMode == 1) newPin = 5;
      else if (src.hwMode == 3) newPin = 47;
      else if (src.hwMode == 4) newPin = 6;
      else if (src.hwMode == 5) newPin = 2;

      // Only clear if pins are different or HW-mode is being disabled
      if (oldPin != newPin || src.hwMode == 0) {
        gpioToInput[oldPin] = -1;  // Clear mapping
      }
    }
  }

  CounterConfig c = src;

  c.controlReg    = src.controlReg;
  c.id        = id;
  c.enabled   = (c.enabled ? 1 : 0);
  c.edgeMode  = sanitizeEdge(c.edgeMode);
  c.direction = sanitizeDirection(c.direction);
  c.bitWidth  = sanitizeBitWidth(c.bitWidth);

  // Validate prescaler based on mode (HW vs SW)
  // HW mode (hwMode=5) only supports: 1 (external), 8, 64, 256, 1024
  // SW/SW-ISR modes now use: 1, 4, 16, 64, 256, 1024 (unified with HW)
  if (c.hwMode == 5) {
    // HW mode Timer5 - only specific prescaler values
    c.prescaler = sanitizeHWPrescaler(c.prescaler);
  } else if (c.hwMode == 0) {
    // SW/SW-ISR mode - unified prescaler values (1, 4, 16, 64, 256, 1024)
    c.prescaler = sanitizePrescaler_SW(c.prescaler);
  } else {
    // Unknown mode - default to 1
    c.prescaler = 1;
  }

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
  // CRITICAL: Arduino Mega 2560 hardware limitation - ONLY Timer5 (pin 47) has external clock input routed to headers!
  // Timer1, Timer3, Timer4 external clock inputs are NOT accessible on the board.
  // See: https://forum.arduino.cc/t/external-input-to-timer-counter-modules-on-arduino-mega-2560-adk/275652
  if (c.enabled && c.hwMode != 0) {
    if (c.hwMode != 5) {
      // Timer1, Timer3, Timer4 are not supported for HW mode on Arduino Mega 2560
      c.hwMode = 0;  // Force to SW mode
      Serial.print(F("WARNING: Counter "));
      Serial.print(c.id);
      Serial.println(F(" HW mode not supported (only Timer5/pin47 available). Using SW mode instead."));
    } else {
      // Only Timer5 (hwMode=5) is supported - Pin 47 (PL2/T5)
      // CRITICAL v3.6.2: Timer5 T5 external clock is Pin 47, NOT Pin 2!
      // See: https://docs.arduino.cc/retired/hacking/hardware/PinMapping2560/
      uint8_t hw_id = 4;  // Timer5
      uint8_t pin = 47;   // Pin 47 (PL2) - Timer5 T5 external clock input

      if (hw_id != 0) {
      // Check for GPIO conflicts and remove STATIC mappings
      if (pin > 0) {
        gpio_handle_dynamic_conflict(pin);
      }

      // v3.6.2 BUGFIX: DO NOT GPIO-map HW timer PIN (PIN 47 for Timer5 T5)
      // Reason: Timer5 HW counter reads pulses directly from hardware register
      // GPIO polling must NOT read PIN 47 (would create duplicate/competing read)
      // The DYNAMIC GPIO display in show config is informational only
      // (managed by counters_print_status in cli_shell.cpp)
      gpioToInput[pin] = -1;  // Ensure no GPIO polling interference

      // Initialize HW timer with prescaler mode and start value
      // Convert prescaler value (1,8,64,256,1024) to mode (1,3,4,5,6) for HW init
      // Pass the startValue (properly masked to bitWidth) as the initial counter value
      uint32_t hw_start_value = (uint32_t)(sv & 0xFFFFFFFFUL);
      uint8_t prescaler_mode = hwPrescalerToMode(c.prescaler);
      hw_counter_init(hw_id, prescaler_mode, hw_start_value);
      }
    }
  }

  // ============================================================================
  // SW-ISR Mode Validation (hwMode=0 + interruptPin > 0)
  // ============================================================================
  // NOTE: SW-ISR mode reads DIRECTLY from the interrupt pin hardware
  // It ignores GPIO mapping and inputIndex parameter - those are only for SW polling mode
  if (c.hwMode == 0 && c.enabled && c.interruptPin > 0) {
    // Validate interrupt pin is supported
    if (!sw_counter_is_valid_interrupt_pin(c.interruptPin)) {
      Serial.print(F("ERROR: Counter "));
      Serial.print(id);
      Serial.print(F(" - invalid interrupt pin "));
      Serial.print(c.interruptPin);
      Serial.println(F(" (must be 2, 3, 18, 19, 20, or 21)"));
      return false;
    }

    // v3.6.2 NEW: Add DYNAMIC GPIO mapping for SW-ISR interrupt pin
    // This shows which interrupt pin this counter uses (informational/documentation)
    // SW-ISR mode reads directly from interrupt hardware, not from GPIO polling
    uint8_t int_pin = c.interruptPin;
    if (int_pin > 0 && int_pin < NUM_GPIO) {
      // Clear any conflicting mappings to the same inputIndex
      for (uint8_t p = 0; p < NUM_GPIO; p++) {
        if (p != int_pin && gpioToInput[p] == (int16_t)c.inputIndex) {
          gpioToInput[p] = -1;
        }
      }
      // Set this interrupt pin's mapping (informational only)
      gpioToInput[int_pin] = (int16_t)c.inputIndex;
    }
  }

  // Attach/Detach SW-mode interrupt
  // Only for SW mode (hwMode == 0) with a valid interrupt pin
  if (c.hwMode == 0) {
    if (c.enabled && c.interruptPin > 0) {
      // Attach interrupt for enabled SW-mode counter with interrupt pin configured
      sw_counter_attach_interrupt(id, c.interruptPin);
    } else {
      // Detach interrupt if: counter disabled, HW mode changed, or polling mode set
      sw_counter_detach_interrupt(id);
    }
  } else {
    // HW-mode or other: always detach any SW interrupt (in case of mode transition)
    sw_counter_detach_interrupt(id);
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
  // Only Timer5 (hwMode=5) is supported on Arduino Mega 2560
  if (c.hwMode == 5) {
    uint8_t hw_id = 4;  // Timer5
    // Reset HW timer to the configured startValue (not just to 0)
    uint32_t hw_start_value = (uint32_t)(sv & 0xFFFFFFFFUL);
    uint8_t prescaler_mode = hwPrescalerToMode(c.prescaler);
    hw_counter_init(hw_id, prescaler_mode, hw_start_value);
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
  Serial.println(F("hw = HW/SW mode (SW|ISR|T1|T3|T4|T5), pin = GPIO pin (actual hardware pin), hz = measured freq (Hz)"));
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

    // HW mode display: SW, ISR, T1, T3, T4, T5 (v3.4.0 refactored)
    const char* hwStr = "SW";
    if (c.hwMode == 0 && c.interruptPin > 0) {
      hwStr = "ISR";  // Software Interrupt mode
    } else if (c.hwMode == 1) hwStr = "T1";
    else if (c.hwMode == 3) hwStr = "T3";
    else if (c.hwMode == 4) hwStr = "T4";
    else if (c.hwMode == 5) hwStr = "T5";

    // Vis skaleret og rå værdi separat
    // VIGTIGT: Læs fra REGISTRENE, ikke fra c.counterValue
    // (registrene har korrekt prescaler division for raw, og korrekt scale for value)

    // Læs value fra index register (kan være multi-word for 32-bit)
    unsigned long val = 0;
    if (c.regIndex > 0 && c.regIndex < NUM_REGS) {
      if (c.bitWidth <= 16) {
        val = holdingRegs[c.regIndex];
      } else {
        // 32-bit: læs 2 words
        val = holdingRegs[c.regIndex] | ((unsigned long)holdingRegs[c.regIndex+1] << 16);
      }
    }

    // Læs raw fra raw register (kan være multi-word for 32-bit)
    unsigned long raw = 0;
    uint16_t rawRegBase = (c.rawReg > 0) ? c.rawReg : (c.regIndex + 4);
    if (rawRegBase > 0 && rawRegBase < NUM_REGS) {
      if (c.bitWidth <= 16) {
        raw = holdingRegs[rawRegBase];
      } else {
        // 32-bit: læs 2 words
        raw = holdingRegs[rawRegBase] | ((unsigned long)holdingRegs[rawRegBase+1] << 16);
      }
    }

    // kolonne-for-kolonne print med fast bredde
    Serial.print(' ');
    sprintf(buf, "%-7d| ", i+1);       Serial.print(buf);
    sprintf(buf, "%-4d| ", mode);      Serial.print(buf);
    sprintf(buf, "%-4s| ", hwStr);     Serial.print(buf);  // HW mode: SW|ISR|T1|T3|T4|T5

    // Pin display: show actual GPIO pin
    // IMPORTANT (v3.6.1): Respect GPIO mapping config for ALL counter modes
    // First check if inputIndex is mapped to a GPIO pin
    int16_t gpioPin = -1;
    for (uint8_t pin = 0; pin < NUM_GPIO; pin++) {
      if (gpioToInput[pin] == (int16_t)c.inputIndex) {
        gpioPin = pin;
        break;
      }
    }

    // Display mapped pin if found, otherwise show default/nothing
    if (gpioPin != -1) {
      // GPIO mapping found - show actual mapped pin
      sprintf(buf, "%-5d| ", gpioPin);
    } else if (c.hwMode == 0 && c.interruptPin > 0) {
      // SW-ISR mode: show interrupt pin directly (not GPIO mapped)
      sprintf(buf, "%-5d| ", c.interruptPin);
    } else if (c.hwMode != 0) {
      // HW mode without GPIO mapping: show hardcoded default for reference
      // NOTE (v3.6.2 BUGFIX): Only Timer5 (hwMode=5) is implemented for HW counters
      uint8_t defaultPin = 0;
      if (c.hwMode == 1) defaultPin = 5;      // Timer1 default (not routed)
      else if (c.hwMode == 3) defaultPin = 9;    // Timer3 default (not routed)
      else if (c.hwMode == 4) defaultPin = 28;   // Timer4 default (not routed)
      else if (c.hwMode == 5) defaultPin = 2;   // Timer5 default (Pin 2 / PE4 / T5) - FIXED v3.6.2
      sprintf(buf, "%-5d| ", defaultPin);
    } else {
      // No pin mapping found
      sprintf(buf, "%-5s| ", "-");
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

    // Read frequency from correct source (HW mode reads from register, SW mode from variable)
    uint16_t displayFreq = 0;
    if (c.hwMode != 0 && c.freqReg > 0 && c.freqReg < NUM_REGS) {
      // HW mode: read from holdingRegs (written by hw_counter_update_frequency)
      displayFreq = holdingRegs[c.freqReg];
    } else {
      // SW mode: read from counter variable
      displayFreq = c.currentFreqHz;
    }
    sprintf(buf, "%-6d| ", displayFreq); Serial.print(buf);  // hz = measured frequency

    sprintf(buf, "%-10lu| ", val); Serial.print(buf);
    sprintf(buf, "%-10lu", raw); Serial.println(buf);
  }
}

