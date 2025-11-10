// ============================================================================
//  Filnavn : modbus_counters.cpp
//  Projekt  : Modbus RTU Server / CLI
//  Version  : v3.1.4-patch3 (2025-11-05)
//  Forfatter: ChatGPT Automation
//  Formål   : CounterEngine v3 med 4 uafhængige input-tællere.
//             - Edge-detektering (rising/falling/both)
//             - Prescaler
//             - Op/ned-tælling (direction)
//             - BitWidth-maskning (8/16/32/64)
//             - Overflow/auto-reset til startValue
//             - Soft-control via controlReg (bit0=reset,1=start,2=stop)
//             - Skaleret udlæsning til holdingRegs med float scale
//             - Debounce pr. kanal (debounceEnable/debounceTimeMs)
//             - Debug/oversigtsprint via counters_print_status() til CLI.
//  Ændringer:
//    - v3.1.4-patch3: Tilføjet counters_print_status() til tabulær visning
//                     (show counters) og support for "no set counter" i CLI.
//    - v3.1.4-patch2: Tilføjet debounceEnable/debounceTimeMs/lastEdgeMs til
//                     CounterConfig + runtime-debounce i counters_loop().
// ============================================================================

#include "modbus_counters.h"
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

  // Skriv rå, u-skaleret tællerværdi til regIndex + 4 ..
  // Layout:
  //   8/16 bit : regIndex         (skaleret), regIndex+4         (rå)
  //   32 bit   : reg..reg+1       (skaleret), reg+4..reg+5       (rå)
  //   64 bit   : reg..reg+3       (skaleret), reg+4..reg+7       (rå)
  uint16_t rawBase  = c.regIndex + 4;
  uint8_t  rawWords = words;

  if ((uint32_t)rawBase + rawWords > NUM_REGS) {
    // Ingen plads til rå-vinduet → skaleret værdi skrives stadig, rå droppes
    return;
  }

  uint64_t raw = maskToBitWidth((uint64_t)c.counterValue, bw);
  for (uint8_t w = 0; w < rawWords; ++w) {
    holdingRegs[rawBase + w] = (uint16_t)((raw >> (16 * w)) & 0xFFFF);
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
    c.edgeMode  = CNT_EDGE_RISING;
    c.direction = CNT_DIR_UP;
    c.bitWidth  = 32;
    c.prescaler = 1;
    c.inputIndex    = 0;
    c.regIndex      = 0;
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
  }
}

void counters_loop() {
  for (uint8_t idx = 0; idx < 4; ++idx) {
    CounterConfig& c = counters[idx];

    if (!c.enabled) {
      // alligevel spejl overflowFlag og evt. ud-værdi
      if (c.overflowReg < NUM_REGS) {
        holdingRegs[c.overflowReg] = c.overflowFlag ? 1 : 0;
      }
      store_value_to_regs(idx);
      continue;
    }

    // Håndter controlReg-kommandoer
    handle_control(c);

    // Hvis ikke running -> track lastLevel, men tælle ikke
    bool lvl = di_read(c.inputIndex);
    if (!c.running) {
      c.lastLevel = lvl ? 1 : 0;
      // Spejl overflowFlag kun hvis sat, og bevar det indtil reset-bit rydder det
    if (c.overflowReg < NUM_REGS) {
     if (c.overflowFlag)
      holdingRegs[c.overflowReg] = 1;
      }

      store_value_to_regs(idx);
      continue;
    }

    // Edge-detektering
    bool fire = false;
    uint8_t edge = sanitizeEdge(c.edgeMode);
    uint8_t last = c.lastLevel;
    uint8_t now  = lvl ? 1 : 0;

    if      (edge == CNT_EDGE_RISING  && last == 0 && now == 1) fire = true;
    else if (edge == CNT_EDGE_FALLING && last == 1 && now == 0) fire = true;
    else if (edge == CNT_EDGE_BOTH    && last != now)           fire = true;

    c.lastLevel = now;

    // Debounce: hvis aktiveret, filtrér edges der kommer for tæt
    if (fire && c.debounceEnable && c.debounceTimeMs > 0) {
      unsigned long nowMs = millis();
      unsigned long dt = nowMs - c.lastEdgeMs;
      if (dt < c.debounceTimeMs) {
        // Ignorér denne edge som "støj"
        fire = false;
      } else {
        c.lastEdgeMs = nowMs;
      }
    } else if (fire && (!c.debounceEnable || c.debounceTimeMs == 0)) {
      // Uden debounce: opdater blot lastEdgeMs til reference
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

    // Tælle-step
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
      // Auto-reset til startValue (masket til bitWidth)
      uint64_t sv = c.startValue;
      sv = maskToBitWidth(sv, bw);
      c.counterValue = sv;
    }

    // Spejl overflowFlag og skaleret værdi
    if (c.overflowReg < NUM_REGS) {
      holdingRegs[c.overflowReg] = c.overflowFlag ? 1 : 0;
    }

    store_value_to_regs(idx);
  }
}

// ============================================================================
//  Public helpers
// ============================================================================

bool counters_config_set(uint8_t id, const CounterConfig& src) {
  if (id < 1 || id > 4) return false;
  uint8_t idx = id - 1;

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

  if (c.overflowReg < NUM_REGS) {
    holdingRegs[c.overflowReg] = 0;
  }
  store_value_to_regs(idx);
}

void counters_clear_all() {
  for (uint8_t id = 1; id <= 4; ++id) {
    uint8_t idx = id - 1;
    CounterConfig& c = counters[idx];

    uint8_t bw = sanitizeBitWidth(c.bitWidth);
    uint64_t sv = c.startValue;
    sv = maskToBitWidth(sv, bw);

    c.counterValue = sv;
    c.edgeCount    = 0;
    c.overflowFlag = 0;

    if (c.overflowReg < NUM_REGS) {
      holdingRegs[c.overflowReg] = 0;
    }
    store_value_to_regs(idx);
  }
}

// ============================================================================
//  Debug / status print til CLI (show counters)
// ============================================================================

void counters_print_status() {
  Serial.println(F(""));
  Serial.println(F("------------------------------------------------------------------------------------------------------------------------------"));
  Serial.println(F("co = count-on, sv = startValue, res = resolution, ps = prescaler, ol = overload, c = control, dir = direction, sf = scaleFloat"));
  Serial.println(F("cr = count-reg, d = debounce, dt = debounce-time, input = dis input reg, value = scaled value, raw = unscaled value"));
  Serial.println(F("------------------------------------------------------------------------------------------------------------------------------"));
  Serial.println(F("counter | mode| co     | sv       | res | ps   | ol   | c    | dir   | sf     | cr   | d   | dt   | input | value     | raw"));

  char buf[32];

  for (uint8_t i = 0; i < 4; ++i) {
    const CounterConfig& c = counters[i];
    uint8_t mode = c.enabled ? 1 : 0;
    const char* coStr = (c.edgeMode==CNT_EDGE_FALLING)?"falling":(c.edgeMode==CNT_EDGE_BOTH)?"both":"rising";
    const char* dirStr = (c.direction==CNT_DIR_DOWN)?"down":"up";
    const char* dStr = c.debounceEnable?"on":"off";

    // Vis skaleret og rå værdi separat
    unsigned long val  = (unsigned long)((double)c.counterValue * (double)c.scale);
    unsigned long raw  = (unsigned long)(c.counterValue & 0xFFFFFFFFULL);

    // kolonne-for-kolonne print med fast bredde
    Serial.print(' ');
    sprintf(buf, "%-7d| ", i+1);       Serial.print(buf);
    sprintf(buf, "%-4d| ", mode);      Serial.print(buf);
    sprintf(buf, "%-7s| ", coStr);     Serial.print(buf);
    sprintf(buf, "%-9lu| ", (unsigned long)c.startValue); Serial.print(buf);
    sprintf(buf, "%-4d| ", c.bitWidth); Serial.print(buf);
    sprintf(buf, "%-5d| ", c.prescaler); Serial.print(buf);
    sprintf(buf, "%-5d| ", c.overflowReg); Serial.print(buf);
    sprintf(buf, "%-5d| ", c.controlReg); Serial.print(buf);
    sprintf(buf, "%-6s| ", dirStr); Serial.print(buf);

    // Format scale float manually (sprintf %f not supported on all Arduino boards)
    int scaleInt = (int)c.scale;
    int scaleDec = (int)((c.scale - scaleInt) * 1000);
    if (scaleDec < 0) scaleDec = -scaleDec;  // absolute value for decimal part
    sprintf(buf, "%d.%03d  | ", scaleInt, scaleDec);
    Serial.print(buf);

    sprintf(buf, "%-5d| ", c.regIndex); Serial.print(buf);
    sprintf(buf, "%-4s| ", dStr); Serial.print(buf);
    sprintf(buf, "%-5d| ", c.debounceTimeMs); Serial.print(buf);
    sprintf(buf, "%-6d| ", c.inputIndex); Serial.print(buf);
    sprintf(buf, "%-10lu| ", val); Serial.print(buf);
    sprintf(buf, "%-10lu", raw); Serial.println(buf);
  }
}

