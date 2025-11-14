// ============================================================================
//  Filnavn : modbus_counters_sw_int.cpp
//  Projekt  : Modbus RTU Server / CLI
//  Version  : v3.3.0 (2025-11-13)
//  Forfatter: JanG at modbus_slave@laces.dk
//  Formål   : External interrupt handling for SW-mode counters.
//             Provides 6 ISRs for INT0-INT5 (pins 2,3,18,19,20,21).
//             Prevents CLI operations from blocking edge detection.
// ============================================================================

#include "modbus_counters_sw_int.h"
#include "modbus_counters.h"
#include "modbus_core.h"
#include <string.h>

// ============================================================================
// Interrupt Pin Mapping
// ============================================================================
// Maps counter IDs (1..4) to attached interrupt pins
static uint8_t counterToInterruptPin[4] = {0, 0, 0, 0};  // 0 = not attached

// Maps interrupt numbers (0..5) to counter IDs
// 0 = not used, 1-4 = counter ID
static uint8_t interruptToCounter[6] = {0, 0, 0, 0, 0, 0};

// Previous pin states for edge detection (per counter)
static uint8_t counterLastState[4] = {0, 0, 0, 0};


// ============================================================================
// Valid Interrupt Pin Mapping
// ============================================================================

// Arduino Mega 2560 interrupt-capable pins
// Use digitalPinToInterrupt() for correct mapping!
static const uint8_t validInterruptPins[] = {2, 3, 18, 19, 20, 21};
static const uint8_t NUM_VALID_PINS = 6;

// ============================================================================
// Helpers
// ============================================================================

bool sw_counter_is_valid_interrupt_pin(uint8_t pin) {
  for (uint8_t i = 0; i < NUM_VALID_PINS; i++) {
    if (validInterruptPins[i] == pin) return true;
  }
  return false;
}

// Get Arduino interrupt number for a pin using digitalPinToInterrupt()
// This correctly handles the pin-to-interrupt mapping for Arduino Mega 2560
int8_t sw_counter_pin_to_interrupt(uint8_t pin) {
  // Verify pin is valid first
  if (!sw_counter_is_valid_interrupt_pin(pin)) {
    return -1;  // Invalid pin
  }
  // Use Arduino's built-in macro for correct mapping
  return digitalPinToInterrupt(pin);
}

// ============================================================================
// Interrupt Handler Core Logic
// ============================================================================

void sw_counter_interrupt_handler(uint8_t counter_id) {
  if (counter_id < 1 || counter_id > 4) return;

  uint8_t idx = counter_id - 1;
  CounterConfig& c = counters[idx];

  if (!c.enabled || c.hwMode != 0) return;  // Only SW mode
  if (!c.running) return;                    // Counter must be running to count

  // Read current pin state directly from the hardware pin associated with this interrupt
  // (ignore GPIO mapping and inputIndex - ISR mode reads directly from the interrupt pin)
  uint8_t pin = counterToInterruptPin[idx];
  if (pin == 0) return;  // Not attached

  uint8_t now = digitalRead(pin) ? 1 : 0;  // Read directly from hardware pin
  uint8_t last = counterLastState[idx];

  bool fire = false;
  uint8_t edge = c.edgeMode;

  if      (edge == CNT_EDGE_RISING  && last == 0 && now == 1) fire = true;
  else if (edge == CNT_EDGE_FALLING && last == 1 && now == 0) fire = true;
  else if (edge == CNT_EDGE_BOTH    && last != now)           fire = true;

  counterLastState[idx] = now;

  if (!fire) return;

  // Debounce check (if enabled)
  unsigned long nowMs = millis();
  if (c.debounceEnable && c.debounceTimeMs > 0) {
    unsigned long dt = nowMs - c.lastEdgeMs;
    if (dt < c.debounceTimeMs) {
      return;  // Ignore this edge as "noise"
    }
    c.lastEdgeMs = nowMs;
  } else if (fire) {
    c.lastEdgeMs = millis();
  }

  // REMOVED: SW-ISR mode prescaler via edgeCount (now handled in store_value_to_regs)
  // SW-ISR mode now counts ALL edges, just like HW mode and SW mode (v3.6.0)
  // Prescaler division happens only at output (raw register)
  // This gives consistent behavior across ALL counter modes

  // Count step
  uint8_t bw = c.bitWidth;
  if (bw != 8 && bw != 16 && bw != 32 && bw != 64) bw = 32;

  uint64_t maxVal = (bw == 64) ? 0xFFFFFFFFFFFFFFFFULL : ((1ULL << bw) - 1);
  bool overflow = false;

  uint8_t dir = c.direction;
  if (dir != 0) dir = 1;  // 0=UP, 1=DOWN

  if (dir == 1) {  // DOWN
    if (c.counterValue == 0) {
      overflow = true;
    } else {
      c.counterValue--;
    }
  } else {  // UP
    if (c.counterValue >= maxVal) {
      overflow = true;
    } else {
      c.counterValue++;
    }
  }

  if (overflow) {
    c.overflowFlag = 1;
    if (c.overflowReg < 160) {
      holdingRegs[c.overflowReg] = 1;
    }
    // Auto-reset to startValue
    uint64_t sv = c.startValue;
    if (bw == 8)  sv &= 0xFFULL;
    else if (bw == 16) sv &= 0xFFFFULL;
    else if (bw == 32) sv &= 0xFFFFFFFFULL;
    c.counterValue = sv;

    // Reset frequency tracking on overflow
    c.lastFreqCalcMs = 0;
    c.lastCountForFreq = 0;
    c.currentFreqHz = 0;
    if (c.freqReg > 0 && c.freqReg < 160) {
      holdingRegs[c.freqReg] = 0;
    }
  }
}

// ============================================================================
// ISR Handlers - External Interrupts INT0..INT5
// ============================================================================
// CRITICAL: These MUST be static, NOT local variables!
// Arduino's attachInterrupt() stores the function pointer, so it must remain valid

static void isr_int0() {
  if (interruptToCounter[0] > 0) {
    sw_counter_interrupt_handler(interruptToCounter[0]);
  }
}

static void isr_int1() {
  if (interruptToCounter[1] > 0) {
    sw_counter_interrupt_handler(interruptToCounter[1]);
  }
}

static void isr_int2() {
  if (interruptToCounter[2] > 0) {
    sw_counter_interrupt_handler(interruptToCounter[2]);
  }
}

static void isr_int3() {
  if (interruptToCounter[3] > 0) {
    sw_counter_interrupt_handler(interruptToCounter[3]);
  }
}

static void isr_int4() {
  if (interruptToCounter[4] > 0) {
    sw_counter_interrupt_handler(interruptToCounter[4]);
  }
}

static void isr_int5() {
  if (interruptToCounter[5] > 0) {
    sw_counter_interrupt_handler(interruptToCounter[5]);
  }
}

// ============================================================================
// Attach/Detach Interrupt
// ============================================================================

bool sw_counter_attach_interrupt(uint8_t counter_id, uint8_t pin) {
  if (counter_id < 1 || counter_id > 4) return false;
  if (!sw_counter_is_valid_interrupt_pin(pin)) {
    return false;
  }

  int8_t intNum = sw_counter_pin_to_interrupt(pin);
  if (intNum < 0 || intNum > 5) {
    return false;
  }

  // Check if this interrupt number is already in use by another counter
  for (uint8_t i = 0; i < 4; i++) {
    if (i != (counter_id - 1)) {  // Different counter
      uint8_t otherPin = counterToInterruptPin[i];
      if (otherPin > 0) {
        int8_t otherInt = sw_counter_pin_to_interrupt(otherPin);
        if (otherInt >= 0 && otherInt == intNum) {
          return false;  // Already in use
        }
      }
    }
  }

  uint8_t idx = counter_id - 1;

  // Detach any previous interrupt
  if (counterToInterruptPin[idx] > 0) {
    int8_t oldInt = sw_counter_pin_to_interrupt(counterToInterruptPin[idx]);
    if (oldInt >= 0 && oldInt <= 5) {
      detachInterrupt(oldInt);
      interruptToCounter[oldInt] = 0;
    }
  }

  // Initialize counter state
  counterToInterruptPin[idx] = pin;

  // CRITICAL: Configure pin as INPUT before attaching interrupt
  // Arduino Mega 2560 REQUIRES pin to be INPUT for external interrupts to trigger
  pinMode(pin, INPUT);

  // Initialize counterLastState from the hardware pin (ISR mode ignores GPIO mapping)
  counterLastState[idx] = digitalRead(pin) ? 1 : 0;
  interruptToCounter[intNum] = counter_id;

  // Attach the interrupt
  // Use CHANGE mode to detect both rising and falling edges
  // The edge type (rising/falling/both) is handled in the handler
  // CRITICAL: Use static array to ensure function pointers remain valid!
  static void (*isrs[6])() = {isr_int0, isr_int1, isr_int2, isr_int3, isr_int4, isr_int5};

  attachInterrupt(intNum, isrs[intNum], CHANGE);

  return true;
}

void sw_counter_detach_interrupt(uint8_t counter_id) {
  if (counter_id < 1 || counter_id > 4) return;

  uint8_t idx = counter_id - 1;
  uint8_t pin = counterToInterruptPin[idx];

  if (pin == 0) return;  // Not attached

  int8_t intNum = sw_counter_pin_to_interrupt(pin);
  if (intNum >= 0 && intNum <= 5) {
    detachInterrupt(intNum);
    interruptToCounter[intNum] = 0;
  }

  counterToInterruptPin[idx] = 0;
  counterLastState[idx] = 0;
}
