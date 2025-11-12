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
bool hw_counter_init(uint8_t counter_id, uint8_t mode) {
  if (counter_id < 1 || counter_id > 4) return false;

  uint8_t tccrb_clksel = 0;

  // Map mode to clock select bits
  switch (mode) {
    case 0: tccrb_clksel = 0x00; break;           // Stop timer
    case 1: tccrb_clksel = 0x07; break;           // External clock, rising edge (T pin)
    case 2: tccrb_clksel = 0x01; break;           // No prescale (clk_io)
    case 3: tccrb_clksel = 0x02; break;           // /8 prescale
    case 4: tccrb_clksel = 0x03; break;           // /64 prescale
    case 5: tccrb_clksel = 0x04; break;           // /256 prescale
    default: tccrb_clksel = 0x05; break;          // /1024 prescale (any other)
  }

  switch (counter_id) {
    case 1:  // Timer1, Pin D5 (T1)
      if (mode != 0) pinMode(5, INPUT);
      TCCR1A = 0x00;                    // Normal mode (WGM = 0000)
      TCCR1B = tccrb_clksel;            // Set clock source
      TCNT1 = 0x0000;                   // Reset counter
      TIMSK1 = (mode != 0) ? 0x01 : 0x00;  // Enable/disable overflow interrupt (TOIE1)
      hwCounter1Extend = 0;
      hwOverflowCount[0] = 0;
      return true;

    case 2:  // Timer3, Pin D47 (T3)
      if (mode != 0) pinMode(47, INPUT);
      TCCR3A = 0x00;
      TCCR3B = tccrb_clksel;
      TCNT3 = 0x0000;
      TIMSK3 = (mode != 0) ? 0x01 : 0x00;
      hwCounter3Extend = 0;
      hwOverflowCount[1] = 0;
      return true;

    case 3:  // Timer4, Pin D6 (T4)
      if (mode != 0) pinMode(6, INPUT);
      TCCR4A = 0x00;
      TCCR4B = tccrb_clksel;
      TCNT4 = 0x0000;
      TIMSK4 = (mode != 0) ? 0x01 : 0x00;
      hwCounter4Extend = 0;
      hwOverflowCount[2] = 0;
      return true;

    case 4:  // Timer5, Pin D46 (T5)
      if (mode != 0) pinMode(46, INPUT);
      TCCR5A = 0x00;
      TCCR5B = tccrb_clksel;
      TCNT5 = 0x0000;
      TIMSK5 = (mode != 0) ? 0x01 : 0x00;
      hwCounter5Extend = 0;
      hwOverflowCount[3] = 0;
      return true;
  }

  return false;
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

  static uint16_t lastOverflowCount[4] = {0, 0, 0, 0};
  static unsigned long lastFreqUpdateMs[4] = {0, 0, 0, 0};
  static bool initialized[4] = {false, false, false, false};

  uint8_t idx = counter_id - 1;
  uint16_t currentOverflows = hwOverflowCount[idx];
  unsigned long nowMs = millis();

  // Initialize on first call
  if (!initialized[idx]) {
    lastOverflowCount[idx] = currentOverflows;
    lastFreqUpdateMs[idx] = nowMs;
    initialized[idx] = true;
    holdingRegs[freq_reg] = 0;
    return;
  }

  unsigned long timeDeltaMs = nowMs - lastFreqUpdateMs[idx];

  // Detect counter reset: overflow count decreased (went backwards)
  // This happens when hw_counter_reset() is called
  if (currentOverflows < lastOverflowCount[idx]) {
    // Counter was reset - reinitialize tracking
    lastOverflowCount[idx] = currentOverflows;
    lastFreqUpdateMs[idx] = nowMs;
    holdingRegs[freq_reg] = 0;
    return;
  }

  // Update frequency every ~1 second (1000-2000 ms window for stability)
  if (timeDeltaMs >= 1000 && timeDeltaMs <= 2000) {
    // Handle uint16_t wraparound correctly
    uint16_t overflowDelta = currentOverflows - lastOverflowCount[idx];

    // Sanity check: if overflowDelta is too large, likely a reset occurred
    // (e.g., overflowDelta > 20000 would mean > 1.3 billion pulses/sec - impossible)
    if (overflowDelta > 20) {
      // Reset tracking and skip this measurement
      lastOverflowCount[idx] = currentOverflows;
      lastFreqUpdateMs[idx] = nowMs;
      holdingRegs[freq_reg] = 0;
      return;
    }

    // Frequency = (overflows × 65536 pulses/overflow) × 1000 ms / timeDeltaMs
    // Result in Hz (pulses per second)
    uint32_t freqHz = (((uint32_t)overflowDelta * 65536UL * 1000UL) / timeDeltaMs);

    // Clamp to reasonable range (0-20000 Hz by design)
    if (freqHz > 20000UL) freqHz = 20000UL;

    // Write to Modbus register
    holdingRegs[freq_reg] = (uint16_t)freqHz;

    // Update tracking
    lastOverflowCount[idx] = currentOverflows;
    lastFreqUpdateMs[idx] = nowMs;
  }
  // If time delta is > 5000ms, reset tracking to avoid stale measurements
  else if (timeDeltaMs > 5000) {
    lastOverflowCount[idx] = currentOverflows;
    lastFreqUpdateMs[idx] = nowMs;
    holdingRegs[freq_reg] = 0;  // Reset frequency to 0 (timeout)
  }
}

