// ============================================================================
//  Filnavn : modbus_counters_hw.cpp
//  Projekt  : Modbus RTU Server / CLI
//  Version  : v3.3.0 (2025-11-11)
//  Forfatter: JanG at modbus_slave@laces.dk
//  Formål   : Hardware Counter Engine implementation.
//             ISRs for Timer1, Timer3, Timer4, Timer5 overflow events.
//             Supports external clock (pulse input) mode for deterministic counting.
// ============================================================================

#include "modbus_counters_hw.h"

// ============================================================================
// Global HW Counter State
// ============================================================================

volatile uint32_t hwCounter1Extend = 0;
volatile uint32_t hwCounter3Extend = 0;
volatile uint32_t hwCounter4Extend = 0;
volatile uint32_t hwCounter5Extend = 0;
volatile uint16_t hwOverflowCount[4] = {0, 0, 0, 0};

// Store prescaler mode for each counter (for frequency calculation)
// Values: 1=external clock (no prescale), 3=/8, 4=/64, 5=/256, 6=/1024
static uint8_t hwCounterPrescalerMode[4] = {1, 1, 1, 1};

// ============================================================================
// ISR - Timer1 Overflow (Counter 1, Pin D5/T1)
// ============================================================================
// IMPORTANT: ISRs must NEVER call micros() or millis() - causes corruption!
ISR(TIMER1_OVF_vect) {
  hwCounter1Extend++;
  hwOverflowCount[0]++;
}

// ============================================================================
// ISR - Timer3 Overflow (Counter 2, Pin D47/T3)
// ============================================================================
ISR(TIMER3_OVF_vect) {
  hwCounter3Extend++;
  hwOverflowCount[1]++;
}

// ============================================================================
// ISR - Timer4 Overflow (Counter 3, Pin D6/T4)
// ============================================================================
ISR(TIMER4_OVF_vect) {
  hwCounter4Extend++;
  hwOverflowCount[2]++;
}

// ============================================================================
// ISR - Timer5 Overflow (Counter 4, Pin D46/T5)
// ============================================================================
ISR(TIMER5_OVF_vect) {
  hwCounter5Extend++;
  hwOverflowCount[3]++;
}

// ============================================================================
// Initialize HW Counter
// ============================================================================
// counter_id: 1..4 (maps to Timer1/3/4/5)
// mode: 0 = off, 1 = external clock (T pin), 2/3/4/5 = internal prescale
// start_value: initial value to set the counter to (default 0)
bool hw_counter_init(uint8_t counter_id, uint8_t mode, uint32_t start_value) {
  // CRITICAL: Arduino Mega 2560 only supports Timer5 (pin 47) for external clock input!
  // Timer1, Timer3, Timer4 external clock inputs are NOT routed to board headers.
  // Only counter_id=4 (Timer5) is supported.

  if (counter_id != 4) return false;  // Only Timer5 (counter_id=4) is supported

  uint8_t tccrb_clksel = 0;

  // Map mode to TCCR5B clock select bits
  // Mode values come from sanitizeHWPrescaler(): 1, 3, 4, 5, or 6
  switch (mode) {
    case 0: tccrb_clksel = 0x00; break;           // Stop timer
    case 1: tccrb_clksel = 0x07; break;           // External clock, rising edge (T5 pin)
    case 3: tccrb_clksel = 0x02; break;           // /8 prescale
    case 4: tccrb_clksel = 0x03; break;           // /64 prescale
    case 5: tccrb_clksel = 0x04; break;           // /256 prescale
    case 6: tccrb_clksel = 0x05; break;           // /1024 prescale
    default: tccrb_clksel = 0x07; break;          // Default to external clock
  }

  // Extract 16-bit TCNT and 16-bit extend parts from 32-bit start_value
  uint16_t tcnt_val = (uint16_t)(start_value & 0xFFFF);
  uint16_t extend_val = (uint16_t)(start_value >> 16);

  // Store prescaler mode for frequency calculation (counter_id 4 = index 3)
  hwCounterPrescalerMode[3] = mode;

  // Only Timer5 case (counter_id=4) is implemented
  // Timer5 uses Pin 2 (PE4/T5) - verified working on Arduino Mega 2560
  if (mode != 0) pinMode(2, INPUT);
  cli();  // Disable interrupts during setup
  TCCR5B = 0x00;                    // STOP timer first (no clock source)
  TCCR5A = 0x00;
  TCNT5 = tcnt_val;                 // Set initial counter value
  hwOverflowCount[3] = 0;
  hwCounter5Extend = extend_val;    // Set extension registers
  TIFR5 = 0x01;                     // CLEAR overflow flag (bit 0 = TOV5) BEFORE enabling interrupt
  TIMSK5 = 0x00;                    // Disable interrupt temporarily
  TCCR5B = tccrb_clksel;
  TIMSK5 = (mode != 0) ? 0x01 : 0x00;
  sei();  // Re-enable interrupts
  return true;
}

// ============================================================================
// Get Combined Counter Value (Extension + Current)
// ============================================================================
uint32_t hw_counter_get_value(uint8_t counter_id) {
  if (counter_id < 1 || counter_id > 4) return 0;

  uint16_t tcnt = 0;
  uint32_t extend = 0;

  // Read with interrupt protection
  cli();  // Disable interrupts

  switch (counter_id) {
    case 1:
      extend = hwCounter1Extend;
      tcnt = TCNT1;
      break;
    case 2:
      extend = hwCounter3Extend;
      tcnt = TCNT3;
      break;
    case 3:
      extend = hwCounter4Extend;
      tcnt = TCNT4;
      break;
    case 4:
      extend = hwCounter5Extend;
      tcnt = TCNT5;
      break;
  }

  sei();  // Enable interrupts

  // Combine: (extend << 16) | tcnt for 32-bit value
  return (extend << 16) | tcnt;
}

// ============================================================================
// Reset HW Counter
// ============================================================================
void hw_counter_reset(uint8_t counter_id) {
  if (counter_id < 1 || counter_id > 4) return;

  cli();  // Disable interrupts

  switch (counter_id) {
    case 1:
      TCNT1 = 0;
      hwCounter1Extend = 0;
      hwOverflowCount[0] = 0;
      break;
    case 2:
      TCNT3 = 0;
      hwCounter3Extend = 0;
      hwOverflowCount[1] = 0;
      break;
    case 3:
      TCNT4 = 0;
      hwCounter4Extend = 0;
      hwOverflowCount[2] = 0;
      break;
    case 4:
      TCNT5 = 0;
      hwCounter5Extend = 0;
      hwOverflowCount[3] = 0;
      break;
  }

  sei();  // Enable interrupts
}

// ============================================================================
// Reset Frequency Measurement Tracking
// ============================================================================
// NOTE: This function accesses static variables from hw_counter_update_frequency
// by declaring extern static arrays here - not ideal but necessary for resetting
// the frequency tracking state when counter is reset.
void hw_counter_reset_frequency(uint8_t counter_id) {
  if (counter_id < 1 || counter_id > 4) return;

  // We need to mark this counter as needing reinitialization
  // This is handled by the initialized flag in hw_counter_update_frequency
  // Since we can't access static variables from another function, we rely on
  // the fact that after reset, hwOverflowCount is 0, and the first frequency
  // update will detect this anomaly and reinitialize automatically
}

// ============================================================================
// Stop HW Counter
// ============================================================================
void hw_counter_stop(uint8_t counter_id) {
  if (counter_id < 1 || counter_id > 4) return;

  switch (counter_id) {
    case 1: TCCR1B = 0x00; TIMSK1 = 0x00; break;
    case 2: TCCR3B = 0x00; TIMSK3 = 0x00; break;
    case 3: TCCR4B = 0x00; TIMSK4 = 0x00; break;
    case 4: TCCR5B = 0x00; TIMSK5 = 0x00; break;
  }
}

// ============================================================================
// Update Frequency Measurement (Interrupt-based, Deterministic)
// ============================================================================
void hw_counter_update_frequency(uint16_t freq_reg, uint8_t counter_id) {
  if (freq_reg >= NUM_REGS || counter_id < 1 || counter_id > 4) return;

  static uint32_t lastCounterValue[4] = {0, 0, 0, 0};
  static unsigned long lastFreqUpdateMs[4] = {0, 0, 0, 0};
  static bool initialized[4] = {false, false, false, false};

  uint8_t idx = counter_id - 1;
  unsigned long nowMs = millis();

  // Get current counter value (combines hwCounterXExtend + TCNTX)
  // This works for ALL frequencies (low, high, and with/without overflows)
  uint32_t currentCounterValue = hw_counter_get_value(counter_id);

  // Initialize on first call
  if (!initialized[idx]) {
    lastCounterValue[idx] = currentCounterValue;
    lastFreqUpdateMs[idx] = nowMs;
    initialized[idx] = true;
    holdingRegs[freq_reg] = 0;
    return;
  }

  unsigned long timeDeltaMs = nowMs - lastFreqUpdateMs[idx];

  // Detect counter reset: counter value decreased (went backwards)
  // This happens when hw_counter_reset() is called
  if (currentCounterValue < lastCounterValue[idx]) {
    // Counter was reset - reinitialize tracking
    lastCounterValue[idx] = currentCounterValue;
    lastFreqUpdateMs[idx] = nowMs;
    holdingRegs[freq_reg] = 0;
    return;
  }

  // Update frequency every ~1 second (1000-2000 ms window for stability)
  if (timeDeltaMs >= 1000 && timeDeltaMs <= 2000) {
    // Calculate pulse delta from counter value
    uint32_t counterDelta = currentCounterValue - lastCounterValue[idx];

    // Get prescaler multiplier for this counter
    // Mode 1 = external clock (no prescale, multiplier = 1)
    // Mode 3 = /8 prescale (multiplier = 8)
    // Mode 4 = /64 prescale (multiplier = 64)
    // Mode 5 = /256 prescale (multiplier = 256)
    // Mode 6 = /1024 prescale (multiplier = 1024)
    uint16_t prescaleMultiplier = 1;
    uint8_t mode = hwCounterPrescalerMode[idx];
    switch (mode) {
      case 3: prescaleMultiplier = 8; break;
      case 4: prescaleMultiplier = 64; break;
      case 5: prescaleMultiplier = 256; break;
      case 6: prescaleMultiplier = 1024; break;
      default: prescaleMultiplier = 1; break;  // Mode 1 or invalid = no prescale
    }

    // Calculate actual pulse count (counter delta × prescaler)
    uint32_t pulseDelta = counterDelta * prescaleMultiplier;

    // Sanity check: if pulseDelta is unreasonably large, skip measurement
    // (e.g., pulseDelta > 100000 @ 1sec = >100kHz, max is 20kHz by design)
    if (pulseDelta > 100000UL) {
      // Skip this measurement as anomalous
      lastCounterValue[idx] = currentCounterValue;
      lastFreqUpdateMs[idx] = nowMs;
      return;
    }

    // Frequency = pulses per second
    // pulseDelta / timeDeltaMs * 1000 = Hz
    uint32_t freqHz = (pulseDelta * 1000UL) / timeDeltaMs;

    // Clamp to reasonable range (0-20000 Hz by design)
    if (freqHz > 20000UL) freqHz = 20000UL;

    // Write to Modbus register
    holdingRegs[freq_reg] = (uint16_t)freqHz;

    // Update tracking
    lastCounterValue[idx] = currentCounterValue;
    lastFreqUpdateMs[idx] = nowMs;
  }
  // If time delta is > 5000ms, reset tracking to avoid stale measurements
  else if (timeDeltaMs > 5000) {
    lastCounterValue[idx] = currentCounterValue;
    lastFreqUpdateMs[idx] = nowMs;
    holdingRegs[freq_reg] = 0;  // Reset frequency to 0 (timeout)
  }
}

