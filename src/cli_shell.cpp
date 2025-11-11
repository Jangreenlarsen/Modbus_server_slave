// ============================================================================
//  Filnavn : cli_shell.cpp
//  Projekt  : Modbus RTU Server / CLI
//  Version  : v3.1.9 (2025-11-09)
//  Forfatter: JanG at modbus_slave@laces.dk
//  Formål   : Interaktiv CLI til Modbus RTU-serveren, inkl. timers, GPIO,
//             EEPROM-persistens og CounterEngine med HW/SW input-tællere.
//  Ændringer:
//    - v3.1.9:
//        - Counter reset-on-read er nu individuel pr. counter (synkroniseres med controlReg bit 3)
//        - CLI-kommando: "set counter <id> reset-on-read ENABLE|DISABLE"
//        - CLI-kommando: "set counter <id> start ENABLE|DISABLE" (auto-start ved boot)
//        - "show config" viser nu "counter<n> reset-on-read ENABLE/DISABLE"
//        - "show counters" viser reset-on-read status
//        - Control register (alle bits) kan skrives direkte via Modbus FC6
//        - Kun bit 3 (reset-on-read) gemmes til EEPROM og persisterer ved reboot
//    - v3.1.8:
//        - Counter control register vises nu i "show config" med fortolkning:
//            bit0=RESET, bit1=START, bit2=STOP, bit3=Reset-On-Read ENABLE
//        - "show counters" viser nu også control register status med fortolket værdi
//        - BUGFIX: Counter controlFlags bit 3 persisterer nu korrekt efter genstart
//    - v3.1.7:
//        - Tilføjet globalt timer-status og control-register (fælles for 4 timere):
//            bit0..bit3 = timer1..4 aktive flag (sticky latch)
//            reset-on-read styret via control-register bitmask (bit0..3)
//        - CLI-kommandoudvidelse: "set timers status-reg:<n> control-reg:<n>"
//        - show config → viser nu "timer-status" og "timer-control" registre
//        - print_timers_config_block() opdateret til nyt udseende:
//              timers
//                timer-status  reg =150 (bit0..3 = timer1..4)
//                timer-control reg =151 (bit0..3 = timer1..4)
//        - show config → dynamiske reg/coil/input-links vises kun for ENABLED timers/counters
//        - Rettet config-restore for STATIC reg/coil arrays (EEPROM load)
//    - v3.1.4-patch3: CounterEngine debug-udvidelse:
//                     - "no set counter <id>" til at disable/slette counter
//                     - show counters → tabelformat via counters_print_status()
//    - v3.1.4-patch2: CounterEngine debounce-udvidelse:
//                     set counter <id> ... debounce:on|off debounce-ms:<n>,
//                     visning i show config / show counters.
//    - v3.1.4: CLEAN-config, CounterEngine v3 (direction + scale) mv.
//    - v3.1.1-patch1: Opgave 6 – static reg/coil config (set reg/coil static),
//                     udvidet 'show config' med DYNAMIC/STATIC regs/coils/
//                     inputs samt counters-konfigurationsblok.
//    - v3.1.0-patch1: Tilføjet CounterEngine-kommandosæt
//                     (set/show/reset/clear counter), help-tekst for
//                     interrupt-pins og controlReg bitmask samt
//                     linjesektions-kommentarer.
//    - v3.0.7-patch2: Timer-exclusive coil control i CLI write coil, cleanup.
//    - v3.0.7: Oprindelig CLI med timers, GPIO og persistens
// ============================================================================
//
//  CLI SHELL v3.1.9
//  - Timer syntaks:
//      set timer <id> mode <1|2|3|4> parameter P1:<high|low> P2:<high|low> [P3:<high|low>]
//                      T1 <ms> [T2 <ms>] [T3 <ms>] coil <idx>
//                      [trigger <di_idx> edge rising|falling|both sub <1|2|3>]
//      set timers status-reg:<n> control-reg:<n>
//  - Counter syntaks:
//      set counter <id> mode 1 parameter
//           count-on:<rising|falling|both>
//           start-value:<n>
//           res:<8|16|32|64>
//           prescaler:<1|2|4|8|16|32|64|128|256>
//           overload:<reg>
//           input:<di_index>
//           reg:<reg_index>
//           control:<ctrlReg>
//           direction:<up|down>
//           scale:<float>
//           debounce:<on|off> [debounce-ms:<n>]
//    Implicit enable på "set counter"
//  - Andre counter kommandoer:
//      show counters
//      reset counter <id>
//      clear counters
//      no set counter <id>   (disable/slet counter-konfiguration)
//  - Static maps:
//      set reg static <addr> value <val>
//      set coil static <idx> <ON|OFF|0|1>
// ============================================================================



#include "modbus_counters.h"
#include "modbus_core.h"
#include "modbus_globals.h"
#include "modbus_timers.h"
#include "version.h"
#include <avr/wdt.h>

#ifndef MAX_GPIO_PINS
#define MAX_GPIO_PINS 54
#endif

#define CLI_VERSION "v3.1.7-patch6"

// Case-insensitive string compare helper
static bool iequals(const char* a, const char* b) {
  while (*a && *b) {
    char ca = *a;
    char cb = *b;
    if (ca >= 'A' && ca <= 'Z') ca += 32;
    if (cb >= 'A' && cb <= 'Z') cb += 32;
    if (ca != cb) return false;
    ++a; ++b;
  }
  return *a == *b;
}

static void cli_show_gpio();
static void cmd_gpio(uint8_t ntok, char* tok[]);

static bool s_cli = false;
bool cli_active() { return s_cli; }

static const bool CLI_DEBUG_ECHO = false; // set true for RX echo debug

// ---------- Command History (last 3 commands, optimized for RAM) ----------
#define CMD_HISTORY_SIZE 3
#define CMD_LINE_MAX 256
static char cmdHistory[CMD_HISTORY_SIZE][CMD_LINE_MAX];
static uint8_t cmdHistoryCount = 0;    // Total commands stored (0..3)
static uint8_t cmdHistoryWrite = 0;    // Next write position (circular)
static int8_t cmdHistoryNav = -1;      // Current navigation position (-1 = not navigating)

// ---------- Prompt ----------
static void print_prompt() {  Serial.print(F("\r\n"));
                              Serial.print(cliHostname);
                              Serial.print(F("# ")); }

// ---------- Command History Functions ----------
static void addToHistory(const char* cmd) {
  if (cmd == NULL || cmd[0] == '\0') return;

  // Don't add if same as last command
  if (cmdHistoryCount > 0) {
    uint8_t lastIdx = (cmdHistoryWrite + CMD_HISTORY_SIZE - 1) % CMD_HISTORY_SIZE;
    if (strcmp(cmdHistory[lastIdx], cmd) == 0) return;
  }

  strncpy(cmdHistory[cmdHistoryWrite], cmd, sizeof(cmdHistory[0]) - 1);
  cmdHistory[cmdHistoryWrite][sizeof(cmdHistory[0]) - 1] = '\0';

  cmdHistoryWrite = (cmdHistoryWrite + 1) % CMD_HISTORY_SIZE;
  if (cmdHistoryCount < CMD_HISTORY_SIZE) cmdHistoryCount++;

  cmdHistoryNav = -1;  // Reset navigation
}

static const char* getHistoryCmd(int8_t offset) {
  if (cmdHistoryCount == 0) return NULL;
  if (offset < 0 || offset >= cmdHistoryCount) return NULL;

  // Calculate index: most recent is at (write - 1), going backwards
  int idx = (cmdHistoryWrite - 1 - offset + CMD_HISTORY_SIZE) % CMD_HISTORY_SIZE;
  return cmdHistory[idx];
}

// -------------------- [101–200] --------------------

// ---------- Helpers ----------
static bool is_ws(char c) {
  return (c==' '||c=='\t'||c=='\r'||c=='\n'||c=='\f'||c=='\v'||(unsigned char)c==0xA0);
}

static uint8_t tokenize(char* line, char* tok[], uint8_t maxTok) {
  uint8_t n = 0;
  char* p = line;
  while (*p && n < maxTok) {
    while (*p && is_ws(*p)) p++;
    if (!*p) break;
    tok[n++] = p;
    while (*p && !is_ws(*p)) p++;
    if (*p) { *p = '\0'; p++; }
  }
  return n;
}

static const char* alias_norm(const char* s) {
  // verbs
  if (!strcmp(s,"SH")) return "SHOW";
  if (!strcmp(s,"RD")) return "READ";
  if (!strcmp(s,"WR")) return "WRITE";
  if (!strcmp(s,"DP")) return "DUMP";
  if (!strcmp(s,"CONF") || !strcmp(s,"CFG")) return "SET";
  if (!strcmp(s,"QUIT") || !strcmp(s,"END")) return "EXIT";
  if (!strcmp(s,"?")) return "HELP";
  if (!strcmp(s,"SV")) return "SAVE";
  if (!strcmp(s,"LD")) return "LOAD";
  if (!strcmp(s,"DEF") || !strcmp(s,"DEFAULT") || !strcmp(s,"DEFAULTS")) return "DEFAULTS";

  // nouns
  if (!strcmp(s,"COILS"))    return "COILS";
  if (!strcmp(s,"TIMERS"))   return "TIMERS";
  if (!strcmp(s,"COUNTERS")) return "COUNTERS";
  return s;
}

static void normalize_tokens(uint8_t& ntok, char* tok[]) {
  for (uint8_t i=0; i<ntok; i++) {
    tok[i] = (char*)alias_norm(tok[i]);
  }
}

static void toupper_ascii(char* s) {
  while (*s) {
    if (*s>='a' && *s<='z') *s = (char)(*s - 'a' + 'A');
    s++;
  }
}

// -------------------- [201–300] --------------------

// ---------- Support Printers ----------
static void cli_dump_regs() {
  Serial.println(F("=== HOLDING REGISTERS (0..159) ==="));
  for (uint16_t base = 0; base < NUM_REGS && base < 160; base += 8) {
    Serial.print(base); Serial.print(F(": "));
    for (uint8_t i=0; i<8 && (base+i)<NUM_REGS; i++) {
      Serial.print(holdingRegs[base+i]); Serial.print('\t');
    }
    Serial.println();
  }
}

static void cli_dump_coils() {
  Serial.println(F("=== COILS (0..63) ==="));
  for (uint16_t base = 0; base < NUM_COILS && base < 64; base += 16) {
    Serial.print(base); Serial.print(F(": "));
    for (uint8_t i=0; i<16 && (base+i)<NUM_COILS; i++) {
      bool v = bitReadArray(coils, base+i);
      Serial.print(v ? '1' : '0'); Serial.print(' ');
    }
    Serial.println();
  }
}

static void cli_dump_inputs() {
  Serial.println(F("=== DISCRETE INPUTS (0..255) ==="));
  for (uint16_t base = 0; base < NUM_DISCRETE && base < 256; base += 16) {
    Serial.print(base); Serial.print(F(": "));
    for (uint8_t i = 0; i < 16 && (base + i) < NUM_DISCRETE; i++) {
      bool v = bitReadArray(discreteInputs, base + i);
      Serial.print(v ? '1' : '0'); Serial.print(' ');
    }
    Serial.println();
  }
}

static const char* edgeToStr(uint8_t e) {
  switch (e) {
    case 1: return "rising";
    case 2: return "falling";
    case 3: return "both";
    default: return "n/a";
  }
}

// ---------- Static + dynamic config printer ----------
static void print_static_config() {
  // --- REGS ---
  Serial.println(F("regs"));

  // Dynamic regs fra CounterEngine (value-registre)
  for (uint8_t i = 0; i < 4; ++i) {
    const CounterConfig& c = counters[i];
    if (!c.enabled) continue;                 // KUN hvis counter er enabled
    if (c.regIndex >= NUM_REGS) continue;
    Serial.print(F("  reg DYNAMIC "));
    Serial.print(c.regIndex);
    Serial.print(F(" value counter"));
    Serial.println(c.id);
  }

  // Static regs
  for (uint8_t i = 0; i < regStaticCount; i++) {
    Serial.print(F("  reg STATIC "));
    Serial.print(regStaticAddr[i]);
    Serial.print(F(" value "));
    Serial.println(regStaticVal[i]);
  }

  // --- COILS ---
  Serial.println(F("coils"));

  // Dynamic coils fra TimerEngine
  for (uint8_t i = 0; i < 4; ++i) {
    const TimerConfig& t = timers[i];
    if (!t.enabled) continue;                // KUN hvis timer er enabled
    if (t.coil >= NUM_COILS) continue;
    Serial.print(F("  coil DYNAMIC "));
    Serial.print(t.coil);
    Serial.print(F(" value timer"));
    Serial.println(t.id);
  }

  // Static coils
  for (uint8_t i = 0; i < coilStaticCount; i++) {
    Serial.print(F("  coil STATIC "));
    Serial.print(coilStaticIdx[i]);
    Serial.print(F(" value "));
    Serial.println(coilStaticVal[i] ? F("ON") : F("OFF"));
  }

  // --- INPUTS ---
  Serial.println(F("inputs"));
  bool anyInput = false;

  // Dynamic inputs fra TimerEngine (trigger mode)
  for (uint8_t i = 0; i < 4; ++i) {
    const TimerConfig& t = timers[i];
    if (!t.enabled) continue;                // KUN hvis timer er enabled
    if (t.mode != TM_TRIGGER) continue;
    if (t.trigIndex >= NUM_DISCRETE) continue;
    anyInput = true;
    Serial.print(F("  input DYNAMIC "));
    Serial.print(t.trigIndex);
    Serial.print(F(" value timer"));
    Serial.println(t.id);
  }

  // Dynamic inputs fra CounterEngine
  for (uint8_t i = 0; i < 4; ++i) {
    const CounterConfig& c = counters[i];
    if (!c.enabled) continue;                // KUN hvis counter er enabled
    if (c.inputIndex >= NUM_DISCRETE) continue;
    anyInput = true;
    Serial.print(F("  input DYNAMIC "));
    Serial.print(c.inputIndex);
    Serial.print(F(" value counter"));
    Serial.println(c.id);
  }

  if (!anyInput) {
    Serial.println(F("  (no dynamic inputs)"));
  }
}


static void print_timers_config_block(bool onlyEnabled) {
  bool any = false;
  for (uint8_t i=0; i<4; i++) {
    const TimerConfig& t = timers[i];
    if (onlyEnabled && !t.enabled) continue;
    if (!any) { Serial.println(F("timers")); any = true; }
    Serial.print(F("  timer ")); Serial.print(t.id);
    Serial.print(t.enabled ? F(" ENABLED") : F(" DISABLED"));
    Serial.print(F(" mode=")); Serial.print((int)t.mode);
    if (t.mode == 4) {
      Serial.print(F(" sub=")); Serial.print((int)t.subMode);
    }
    Serial.print(F(" P1:")); Serial.print(t.p1High?"high":"low");
    Serial.print(F(" P2:")); Serial.print(t.p2High?"high":"low");
    Serial.print(F(" P3:")); Serial.print(t.p3High?"high":"low");
    Serial.print(F(" T1=")); Serial.print(t.T1);
    Serial.print(F(" T2=")); Serial.print(t.T2);
    Serial.print(F(" T3=")); Serial.print(t.T3);
    Serial.print(F(" coil=")); Serial.print(t.coil);
    if (t.mode == 4) {
      Serial.print(F(" trig=")); Serial.print(t.trigIndex);
      Serial.print(F(" edge=")); Serial.print(edgeToStr(t.trigEdge));
    }
    Serial.println();
  }
  if (!any) Serial.println(F("timers (none enabled)"));

  // Vis global timer-status og control registre pænt opdelt
  bool showHeader = (timerStatusRegIndex < NUM_REGS || timerStatusCtrlRegIndex < NUM_REGS);
  if (showHeader) {
    Serial.println(F("timers control"));
  }

  if (timerStatusRegIndex < NUM_REGS) {
    Serial.print(F("  timer-status  reg="));
    Serial.print(timerStatusRegIndex);
    Serial.println(F(" (bit0..3 = timer1..4)"));
  }

  if (timerStatusCtrlRegIndex < NUM_REGS) {
    Serial.print(F("  timer-control reg="));
    Serial.print(timerStatusCtrlRegIndex);
    Serial.println(F(" (bit0..3 = timer1..4)"));
  }
}

// Counter konfig-blok til show config (tekstlig)
static void print_counters_config_block(bool onlyEnabled) {
  bool any = false;
  for (uint8_t i = 0; i < 4; ++i) {
    const CounterConfig& c = counters[i];
    if (onlyEnabled && !c.enabled) continue;
    if (!any) { Serial.println(F("counters")); any = true; }

    Serial.print(F("  counter ")); Serial.print(c.id);
    Serial.print(c.enabled ? F(" ENABLED") : F(" DISABLED"));
    Serial.print(F(" edge=")); Serial.print(edgeToStr(c.edgeMode));
    Serial.print(F(" prescaler=")); Serial.print((int)c.prescaler);
    Serial.print(F(" res=")); Serial.print((int)c.bitWidth);
    Serial.print(F(" dir=")); Serial.print(c.direction == CNT_DIR_DOWN ? F("down") : F("up"));

    // Format scale float manually (Serial.print float not always supported)
    Serial.print(F(" scale="));
    int scaleInt = (int)c.scale;
    int scaleDec = (int)((c.scale - scaleInt) * 1000);
    if (scaleDec < 0) scaleDec = -scaleDec;
    Serial.print(scaleInt); Serial.print('.');
    if (scaleDec < 100) Serial.print('0');
    if (scaleDec < 10) Serial.print('0');
    Serial.print(scaleDec);

    // Display input-dis (discrete input index)
    Serial.print(F(" input-dis=")); Serial.print(c.inputIndex);
    Serial.print(F(" index-reg=")); Serial.print(c.regIndex);
    if (c.rawReg > 0 && c.rawReg < NUM_REGS) {
      Serial.print(F(" raw-reg=")); Serial.print(c.rawReg);
    }
    if (c.freqReg > 0 && c.freqReg < NUM_REGS) {
      Serial.print(F(" freq-reg=")); Serial.print(c.freqReg);
    }
    Serial.print(F(" overload-reg="));
    if (c.overflowReg < NUM_REGS) Serial.print(c.overflowReg); else Serial.print(F("n/a"));
    Serial.print(F(" ctrl-reg="));
    if (c.controlReg < NUM_REGS) Serial.print(c.controlReg); else Serial.print(F("n/a"));
    Serial.print(F(" start="));
    Serial.print((unsigned long)(c.startValue & 0xFFFFFFFFULL));

    Serial.print(F(" debounce="));
    if (c.debounceEnable && c.debounceTimeMs > 0) {
      Serial.print(F("on/"));
      Serial.print(c.debounceTimeMs);
      Serial.print(F("ms"));
    } else {
      Serial.print(F("off"));
    }

    Serial.print(F(" hw-mode="));
    if (c.hwMode == 0) {
      Serial.print(F("SW"));
    } else if (c.hwMode == 1) {
      Serial.print(F("HW-T1"));
    } else if (c.hwMode == 3) {
      Serial.print(F("HW-T3"));
    } else if (c.hwMode == 4) {
      Serial.print(F("HW-T4"));
    } else if (c.hwMode == 5) {
      Serial.print(F("HW-T5"));
    } else {
      Serial.print(F("HW-UNKNOWN"));
    }

    Serial.println();
  }
  if (!any) {
    Serial.println(F("counters (none enabled)"));
  }

  // Vis counter reset-on-read control (individuelt pr. counter)
  bool showResetOnRead = false;
  for (uint8_t i = 0; i < 4; ++i) {
    if (onlyEnabled && !counters[i].enabled) continue;
    if (counters[i].enabled) {
      showResetOnRead = true;
      break;
    }
  }

  if (showResetOnRead) {
    Serial.println(F("counters control"));
    for (uint8_t i = 0; i < 4; ++i) {
      const CounterConfig& c = counters[i];
      if (onlyEnabled && !c.enabled) continue;
      if (!c.enabled) continue;

      Serial.print(F(" counter")); Serial.print(c.id);
      Serial.print(F(" reset-on-read "));
      if (counterResetOnReadEnable[i]) {
        Serial.print(F("ENABLE"));
      } else {
        Serial.print(F("DISABLE"));
      }
      Serial.print(F(" auto-start "));
      if (counterAutoStartEnable[i]) {
        Serial.println(F("ENABLE"));
      } else {
        Serial.println(F("DISABLE"));
      }
    }
  }
}


static void print_gpio_config_block() {
  Serial.println(F("gpio"));
  bool any = false;

  // Static GPIO mappings (user-configured via "gpio map" command)
  for (uint8_t pin = 0; pin < MAX_GPIO_PINS; ++pin) {
    int16_t ci = gpioToCoil[pin];
    int16_t di = gpioToInput[pin];
    if (ci != -1) {
      any = true;
      Serial.print(F("  gpio ")); Serial.print(pin);
      Serial.print(F(" at coil ")); Serial.println(ci);
    }
    if (di != -1) {
      any = true;
      Serial.print(F("  gpio ")); Serial.print(pin);
      Serial.print(F(" at input ")); Serial.println(di);
    }
  }

  // DYNAMIC GPIO mappings from HW counters
  for (uint8_t i = 0; i < 4; ++i) {
    const CounterConfig& c = counters[i];
    if (!c.enabled || c.hwMode == 0) continue;  // Only HW mode counters

    any = true;
    Serial.print(F("  gpio "));
    if (c.hwMode == 1) Serial.print(F("5"));
    else if (c.hwMode == 3) Serial.print(F("47"));
    else if (c.hwMode == 4) Serial.print(F("6"));
    else if (c.hwMode == 5) Serial.print(F("46"));

    Serial.print(F(" DYNAMIC at input "));
    Serial.print(c.inputIndex);
    Serial.print(F(" (counter"));
    Serial.print(c.id);
    Serial.print(F(" HW-T"));
    if (c.hwMode == 1) Serial.print(F("1"));
    else if (c.hwMode == 3) Serial.print(F("3"));
    else if (c.hwMode == 4) Serial.print(F("4"));
    else if (c.hwMode == 5) Serial.print(F("5"));
    Serial.println(F(")"));
  }

  if (!any) {
    Serial.println(F("  (no gpio mappings)"));
  }
}

static void print_timer_links() {
  bool anyTimer = false;
  Serial.println(F("timers"));
  for (uint8_t i = 0; i < 4; ++i) {
    const TimerConfig& t = timers[i];
    if (!t.enabled) continue;
    anyTimer = true;
    Serial.print(F("  timer ")); Serial.print(t.id);
    Serial.print(F(" ENABLED"));
    Serial.print(F(" mode=")); Serial.print((int)t.mode);
    Serial.print(F(" P1:")); Serial.print(t.p1High ? "high" : "low");
    Serial.print(F(" P2:")); Serial.print(t.p2High ? "high" : "low");
    Serial.print(F(" P3:")); Serial.print(t.p3High ? "high" : "low");
    Serial.print(F(" T1=")); Serial.print(t.T1);
    Serial.print(F(" T2=")); Serial.print(t.T2);
    Serial.print(F(" T3=")); Serial.print(t.T3);
    Serial.print(F(" coil=")); Serial.print(t.coil);
    if (t.mode == 4) {
      Serial.print(F(" trigger ")); Serial.print(t.trigIndex);
      Serial.print(F(" edge ")); Serial.print(edgeToStr(t.trigEdge));
      Serial.print(F(" sub ")); Serial.print((int)t.subMode);
    }
    Serial.println();
  }
  if (!anyTimer) {
    Serial.println(F("  (no timers configured)"));
  }

  bool anyCoil = false;
  Serial.println(F("coil"));
  for (uint8_t i = 0; i < 4; ++i) {
    const TimerConfig& t = timers[i];
    if (!t.enabled) continue;
    anyCoil = true;
    Serial.print(F("  coil dynamic ")); Serial.print(t.coil);
    Serial.print(F(" at timer ")); Serial.println(t.id);
  }
  if (!anyCoil) {
    Serial.println(F("  (no dynamic coil mappings)"));
  }

  bool anyInput = false;
  Serial.println(F("input"));
  for (uint8_t i = 0; i < 4; ++i) {
    const TimerConfig& t = timers[i];
    if (!t.enabled || t.mode != 4) continue;
    if (t.trigIndex >= NUM_DISCRETE) continue;
    anyInput = true;
    Serial.print(F("  input dynamic ")); Serial.print(t.trigIndex);
    Serial.print(F(" at timer ")); Serial.println(t.id);
  }
  if (!anyInput) {
    Serial.println(F("  (no dynamic input mappings)"));
  }
}

// -------------------- [401–500] --------------------

// ---------- Static helpers til reg/coil static opsætning ----------
static bool reg_static_upsert(uint16_t addr, uint16_t val) {
  if (addr >= NUM_REGS) return false;

  // Tjek om vi allerede har et entry på denne adresse
  for (uint8_t i = 0; i < regStaticCount; ++i) {
    if (regStaticAddr[i] == addr) {
      regStaticVal[i] = val;
      holdingRegs[addr] = val;
      return true;
    }
  }

  // Nyt entry
  if (regStaticCount >= MAX_STATIC_REGS) return false;

  regStaticAddr[regStaticCount] = addr;
  regStaticVal [regStaticCount] = val;
  regStaticCount++;

  holdingRegs[addr] = val;
  return true;
}

static bool coil_static_upsert(uint16_t idx, uint8_t val) {
  if (idx >= NUM_COILS) return false;
  uint8_t v = (val ? 1 : 0);

  // Tjek om vi allerede har et entry på denne coil
  for (uint8_t i = 0; i < coilStaticCount; ++i) {
    if (coilStaticIdx[i] == idx) {
      coilStaticVal[i] = v;
      bitWriteArray(coils, idx, v == 1);
      return true;
    }
  }

  // Nyt entry
  if (coilStaticCount >= MAX_STATIC_COILS) return false;

  coilStaticIdx[coilStaticCount] = idx;
  coilStaticVal[coilStaticCount] = v;
  coilStaticCount++;

  bitWriteArray(coils, idx, v == 1);
  return true;
}

// ---------- SHOW ----------
static void cmd_show(uint8_t ntok, char* tok[]) {
  if (ntok == 1) {
    Serial.println(F("Usage: show {config|stats|regs|coils|inputs|timers|counters|version|gpio}"));
    return;
  }

  if (!strcmp(tok[1],"CONFIG")) {
    Serial.println(F("=== CONFIGURATION ==="));
    Serial.print(F("Version: ")); Serial.println(VERSION_STRING);
    Serial.print(F("Build: "));   Serial.println(VERSION_BUILD);
    Serial.print(F("CLI: "));     Serial.println(F(CLI_VERSION));

    Serial.print(F("Unit-ID: "));
    if (listenToAll) Serial.println(F("0 (ALL)"));
    else Serial.println(currentSlaveID);

    Serial.print(F("Baud: "));    Serial.println(currentBaudrate);
    Serial.print(F("Server: "));  Serial.println(serverRunning ? F("RUNNING") : F("STOPPED"));
    Serial.print(F("Mode: "));    Serial.println(monitorMode ? F("MONITOR") : F("SERVER"));

    Serial.print(F("Hostname: "));
    Serial.println(cliHostname);
    Serial.println(F("====================="));
    print_static_config();
    print_timers_config_block(true);     // only enabled timers
    print_counters_config_block(true);   // only enabled counters
    print_gpio_config_block();
    Serial.println(F("====================="));
    return;
  }

  if (!strcmp(tok[1],"STATS"))        { printStatistics(); return; }
  if (!strcmp(tok[1],"REGS"))         { cli_dump_regs();   return; }
  if (!strcmp(tok[1],"COILS"))        { cli_dump_coils();  print_timer_links(); return; }
  if (!strcmp(tok[1],"INPUTS"))       { cli_dump_inputs(); print_timer_links(); return; }
  if (!strcmp(tok[1],"TIMERS"))       { timers_print_status(); return; }
  if (!strcmp(tok[1], "COUNTERS"))    { 
    counters_print_status(); 
    Serial.println();
    
    // Vis reset-on-read enable flags
    bool anyEnabled = false;
    for (uint8_t i = 0; i < 4; ++i) {
      if (counters[i].enabled) {
        anyEnabled = true;
        break;
      }
    }
    
    if (anyEnabled) {
      Serial.println(F("=== COUNTER CONTROL STATUS ==="));
      for (uint8_t i = 0; i < 4; ++i) {
        const CounterConfig& c = counters[i];
        if (!c.enabled) continue;

        Serial.print(F("Counter")); Serial.print(c.id);
        Serial.print(F(" reset-on-read: "));
        Serial.print(counterResetOnReadEnable[i] ? F("ENABLED") : F("DISABLED"));
        Serial.print(F(" | auto-start: "));
        Serial.print(counterAutoStartEnable[i] ? F("ENABLED") : F("DISABLED"));
        Serial.print(F(" | running: "));
        Serial.println(c.running ? F("YES") : F("NO"));
      }
      Serial.println(F("=============================="));
    }
    
    return; 
  }
  if (!strcmp(tok[1],"GPIO"))         { cli_show_gpio();   return; }
  if (!strcmp(tok[1],"VERSION")) {
    Serial.print(F("Version: ")); Serial.println(VERSION_STRING);
    Serial.print(F("Build: "));   Serial.println(VERSION_BUILD);
    return;
  }

  Serial.println(F("% Unknown 'show' parameter"));
}

// -------------------- [501–600] --------------------

// ---------- READ ----------
static void cmd_read(uint8_t ntok, char* tok[]) {
  if (ntok < 2) {
    Serial.println(F("Usage: read {reg|coil|input} ..."));
    return;
  }

  if (!strcmp(tok[1],"REG")) {
    if (ntok < 3) {
      Serial.println(F("Usage: read reg <addr> [qty=1]"));
      return;
    }
    uint16_t addr = (uint16_t)strtoul(tok[2], nullptr, 10);
    uint16_t qty  = (ntok >= 4) ? (uint16_t)strtoul(tok[3], nullptr, 10) : 1;
    if (addr >= NUM_REGS) {
      Serial.println(F("% Invalid register address"));
      return;
    }
    if (qty < 1) qty = 1;
    if ((uint32_t)addr + qty > NUM_REGS) {
      qty = NUM_REGS - addr;
    }

    Serial.println(F("=== READ HOLDING REGS ==="));
    for (uint16_t i = 0; i < qty; i++) {
      Serial.print('['); Serial.print(addr + i); Serial.print(F("] = "));
      Serial.println(holdingRegs[addr + i]);
    }
    Serial.println(F("========================="));
    return;
  }

  if (!strcmp(tok[1],"COIL")) {
    if (ntok < 3) {
      Serial.println(F("Usage: read coil <idx> [qty=1]"));
      return;
    }
    uint16_t idx = (uint16_t)strtoul(tok[2], nullptr, 10);
    uint16_t qty = (ntok >= 4) ? (uint16_t)strtoul(tok[3], nullptr, 10) : 1;
    if (idx >= NUM_COILS) {
      Serial.println(F("% Coil index out of range"));
      return;
    }
    if (qty < 1) qty = 1;
    if ((uint32_t)idx + qty > NUM_COILS) {
      qty = NUM_COILS - idx;
    }

    Serial.print(F("COILS[")); Serial.print(idx); Serial.print(F(".."));
    Serial.print(idx + qty - 1); Serial.print(F("] = "));
    for (uint16_t i = 0; i < qty; i++) {
      bool v = bitReadArray(coils, idx + i);
      Serial.print(v ? '1' : '0');
      if (i + 1 < qty) Serial.print(' ');
    }
    Serial.println();
    return;
  }

  if (!strcmp(tok[1],"INPUT")) {
    if (ntok < 3) {
      Serial.println(F("Usage: read input <idx> [qty=1]"));
      return;
    }
    uint16_t idx = (uint16_t)strtoul(tok[2], nullptr, 10);
    uint16_t qty = (ntok >= 4) ? (uint16_t)strtoul(tok[3], nullptr, 10) : 1;
    if (idx >= NUM_DISCRETE) {
      Serial.println(F("% Input index out of range"));
      return;
    }
    if (qty < 1) qty = 1;
    if ((uint32_t)idx + qty > NUM_DISCRETE) {
      qty = NUM_DISCRETE - idx;
    }

    Serial.print(F("INPUTS[")); Serial.print(idx); Serial.print(F(".."));
    Serial.print(idx + qty - 1); Serial.print(F("] = "));
    for (uint16_t i = 0; i < qty; i++) {
      bool v = bitReadArray(discreteInputs, idx + i);
      Serial.print(v ? '1' : '0');
      if (i + 1 < qty) Serial.print(' ');
    }
    Serial.println();
    return;
  }

  Serial.println(F("% Unknown object for 'read' (use reg|coil|input)"));
}

// ---------- WRITE ----------
static void cmd_write(uint8_t ntok, char* tok[]) {
  if (ntok < 3) {
    Serial.println(F("Usage: write {reg|coil} ..."));
    return;
  }

  if (!strcmp(tok[1],"INPUT")) {
    Serial.println(F("% Writing discrete inputs is not allowed"));
    return;
  }

  if (!strcmp(tok[1],"REG")) {
    if (ntok != 4) {
      Serial.println(F("Usage: write reg <addr> <value>"));
      return;
    }
    uint16_t addr = (uint16_t)strtoul(tok[2], nullptr, 10);
    uint16_t val  = (uint16_t)strtoul(tok[3], nullptr, 10);
    if (addr >= NUM_REGS) {
      Serial.println(F("% Invalid register address"));
      return;
    }
    holdingRegs[addr] = val;
    Serial.print(F("OK: REG[")); Serial.print(addr); Serial.print(F("] = "));
    Serial.println(val);
    return;
  }

  if (!strcmp(tok[1],"COIL")) {
    if (ntok != 4) {
      Serial.println(F("Usage: write coil <idx> <0|1>"));
      return;
    }
    uint16_t idx = (uint16_t)strtoul(tok[2], nullptr, 10);
    int v = (int)strtoul(tok[3], nullptr, 10);
    if (idx >= NUM_COILS) {
      Serial.println(F("% Coil index out of range"));
      return;
    }
    if (!(v == 0 || v == 1)) {
      Serial.println(F("% Coil value must be 0 or 1"));
      return;
    }

    // Timer-exclusive coil control
    if (!timers_hasCoil(idx)) {
      bitWriteArray(coils, idx, v == 1);
    } else {
      Serial.print(F("INFO: COIL[")); Serial.print(idx);
      Serial.println(F("] controlled by timer – skipping direct write"));
    }

    // Timer-engine på coil-write
    timers_onCoilWrite(idx, (uint8_t)(v ? 1 : 0));

    Serial.print(F("OK: COIL[")); Serial.print(idx); Serial.print(F("] = "));
    Serial.println(v ? F("1 (ON)") : F("0 (OFF)"));
    return;
  }

  Serial.println(F("% Unknown object for 'write' (use reg|coil)"));
}

// -------------------- [601–700] --------------------

// ---------- DUMP ----------
static void cmd_dump(uint8_t ntok, char* tok[]) {
  if (ntok == 1) {
    Serial.println(F("Usage: dump {regs|coils|inputs}"));
    return;
  }
  if (!strcmp(tok[1],"REGS"))      { cli_dump_regs();     return; }
  if (!strcmp(tok[1],"COILS"))     { cli_dump_coils();    return; }
  if (!strcmp(tok[1],"INPUTS"))    { cli_dump_inputs();   return; }
  Serial.println(F("% Unknown 'dump' parameter"));
}

// ---------- SET (inkl. TIMER & COUNTER & STATIC MAPS) ----------
static bool isSupportedBaud(unsigned long nb) {
  const unsigned long rates[] = { 300,600,1200,2400,4800,9600,14400,19200,38400,57600,115200 };
  for (uint8_t i=0; i < sizeof(rates)/sizeof(rates[0]); i++) {
    if (nb == rates[i]) return true;
  }
  return false;
}

static void cmd_set_counter(uint8_t ntok, char* tok[]) {
  //  - Counter syntaks:
  //      set counter <id> mode 1 parameter
  //           count-on:<rising|falling|both>
  //           start-value:<n>
  //           res:<8|16|32|64>
  //           prescaler:<1|2|4|8|16|32|64|128|256>
  //           overload:<reg>
  //           input:<di_index>
  //           reg:<reg_index>
  //           control:<ctrlReg>
  //           direction:<up|down>
  //           scale:<float>
  //           debounce:<on|off> [debounce-ms:<n>]
  //    Implicit enable på "set counter"

  if (ntok < 5) {
    Serial.println(F("Usage: set counter <id> mode 1 parameter ..."));
    return;
  }

  uint8_t id = (uint8_t)strtoul(tok[2], nullptr, 10);
  if (id < 1 || id > 4) {
    Serial.println(F("% Invalid counter id (1..4)"));
    return;
  }

  if (strcmp(tok[3], "MODE") || strcmp(tok[4], "1")) {
    // Kun mode 1 pt.
    Serial.println(F("% Only mode 1 is supported for counter"));
    return;
  }

  CounterConfig cfg;
  if (!counters_get(id, cfg)) {
    memset(&cfg, 0, sizeof(cfg));
    cfg.id = id;
  } else {
    // Hvis der allerede findes en counter, start med en kopi men nulstil runtime-felter
    cfg.counterValue = cfg.startValue;
  }

  cfg.enabled = 1;

  // --- Apply defaults if parameters missing ---
if (cfg.bitWidth != 8 && cfg.bitWidth != 16 && cfg.bitWidth != 32 && cfg.bitWidth != 64)
  cfg.bitWidth = 32;

if (cfg.prescaler == 0)
  cfg.prescaler = 1;

if (cfg.edgeMode == 0)
  cfg.edgeMode = CNT_EDGE_RISING;

if (cfg.scale <= 0.0f) {
  cfg.scale = 1.0f;
}

if (cfg.direction != CNT_DIR_UP && cfg.direction != CNT_DIR_DOWN) {
  cfg.direction = CNT_DIR_UP;
}

// Find "parameter" token
uint8_t start = 5;
  for (uint8_t i = 5; i < ntok; ++i) {
    if (iequals(tok[i], "parameter")) {
      start = i + 1;
      break;
    }
  }

  for (uint8_t i = start; i < ntok; ++i) {
    char* p = tok[i];

    // count-on:<rising|falling|both>
    if (!strncasecmp(p, "count-on:", 9)) {
      const char* v = p + 9;
      if      (!strcasecmp(v, "rising"))  cfg.edgeMode = CNT_EDGE_RISING;
      else if (!strcasecmp(v, "falling")) cfg.edgeMode = CNT_EDGE_FALLING;
      else if (!strcasecmp(v, "both"))    cfg.edgeMode = CNT_EDGE_BOTH;
      continue;
    }

    // start-value:<n>
    if (!strncasecmp(p, "start-value:", 12)) {
      cfg.startValue = strtoul(p + 12, nullptr, 10);
      cfg.counterValue = cfg.startValue;
      continue;
    }

    // resolution:<8|16|32|64> (også accept res: for backward compatibility)
    if (!strncasecmp(p, "resolution:", 11) || !strncasecmp(p, "res:", 4)) {
      const char* valStr = strchr(p, ':');
      if (valStr) {
        uint8_t bw = (uint8_t)strtoul(valStr + 1, nullptr, 10);
        if (bw == 8 || bw == 16 || bw == 32 || bw == 64)
          cfg.bitWidth = bw;
        else {
          Serial.println(F("% Invalid resolution (use 8|16|32|64)"));
          return;
        }
      }
      continue;
    }

    // prescaler:<1..256>
    if (!strncasecmp(p, "prescaler:", 10)) {
      uint16_t pre = (uint16_t)strtoul(p + 10, nullptr, 10);
      if (pre < 1 || pre > 256) {
        Serial.println(F("% Invalid prescaler (1..256)"));
        return;
      }
      cfg.prescaler = (uint8_t)pre;
      continue;
    }

    // overload-reg:<reg> (også accept overload: for backward compatibility)
    if (!strncasecmp(p, "overload-reg:", 13) || !strncasecmp(p, "overload:", 9)) {
      const char* valStr = strchr(p, ':');
      if (valStr) {
        uint16_t r = (uint16_t)strtoul(valStr + 1, nullptr, 10);
        if (r >= NUM_REGS) {
          Serial.println(F("% overload-reg out of range"));
          return;
        }
        cfg.overflowReg = r;
      }
      continue;
    }

    // input-dis:<di_index> (også accept input: for backward compatibility)
    if (!strncasecmp(p, "input-dis:", 10) || !strncasecmp(p, "input:", 6)) {
      const char* valStr = strchr(p, ':');
      if (valStr) {
        uint16_t di = (uint16_t)strtoul(valStr + 1, nullptr, 10);
        if (di >= NUM_DISCRETE) {
          Serial.println(F("% input-dis index out of range"));
          return;
        }
        cfg.inputIndex = di;
      }
      continue;
    }

    // index-reg:<reg_index> (også accept count-reg: for backward compatibility)
    if (!strncasecmp(p, "index-reg:", 10) || !strncasecmp(p, "count-reg:", 10)) {
      const char* valStr = strchr(p, ':');
      if (valStr) {
        uint16_t r = (uint16_t)strtoul(valStr + 1, nullptr, 10);
        if (r >= NUM_REGS) {
          Serial.println(F("% index-reg out of range"));
          return;
        }
        cfg.regIndex = r;
      }
      continue;
    }

    // raw-reg:<reg_index> (NEW)
    if (!strncasecmp(p, "raw-reg:", 8)) {
      uint16_t r = (uint16_t)strtoul(p + 8, nullptr, 10);
      if (r >= NUM_REGS) {
        Serial.println(F("% raw-reg out of range"));
        return;
      }
      cfg.rawReg = r;
      continue;
    }

    // freq-reg:<reg_index> (NEW)
    if (!strncasecmp(p, "freq-reg:", 9)) {
      uint16_t r = (uint16_t)strtoul(p + 9, nullptr, 10);
      if (r >= NUM_REGS) {
        Serial.println(F("% freq-reg out of range"));
        return;
      }
      cfg.freqReg = r;
      continue;
    }

    // ctrl-reg:<ctrlReg> (også accept control-reg: for backward compatibility)
    if (!strncasecmp(p, "ctrl-reg:", 9) || !strncasecmp(p, "control-reg:", 12)) {
      const char* valStr = strchr(p, ':');
      if (valStr) {
        uint16_t r = (uint16_t)strtoul(valStr + 1, nullptr, 10);
        if (r >= NUM_REGS) {
          Serial.println(F("% ctrl-reg out of range"));
          return;
        }
        cfg.controlReg = r;
      }
      continue;
    }

    // direction:<up|down>
    if (!strncasecmp(p, "direction:", 10)) {
      const char* v = p + 10;
      if      (!strcasecmp(v, "up"))   cfg.direction = CNT_DIR_UP;
      else if (!strcasecmp(v, "down")) cfg.direction = CNT_DIR_DOWN;
      else {
        Serial.println(F("% Invalid direction (use up|down)"));
        return;
      }
      continue;
    }

    // scale:<float>
    if (!strncasecmp(p, "scale:", 6)) {
      float s = (float)atof(p + 6);
      if (s <= 0.0f) {
        Serial.println(F("% Invalid scale (must be >0)"));
        return;
      }
      cfg.scale = s;
      continue;
    }

    // debounce:<on|off>
    if (!strncasecmp(p, "debounce:", 9)) {
      const char* v = p + 9;
      if (!strcasecmp(v, "on")) {
        cfg.debounceEnable = 1;
        if (cfg.debounceTimeMs == 0) cfg.debounceTimeMs = 10;
      } else if (!strcasecmp(v, "off")) {
        cfg.debounceEnable = 0; // bevar dt
      } else {
        Serial.println(F("% Invalid debounce (use on|off)"));
        return;
      }
      continue;
    }

    // debounce-ms:<n>
    if (!strncasecmp(p, "debounce-ms:", 12)) {
      uint32_t ms = strtoul(p + 12, nullptr, 10);
      if (ms > 60000UL) ms = 60000UL;
      cfg.debounceTimeMs = (uint16_t)ms;
      continue;
    }

    // hw-mode:<sw|hw-t1|hw-t3|hw-t4|hw-t5> (NEW v3.3.0 with timer selection)
    if (!strncasecmp(p, "hw-mode:", 8)) {
      const char* v = p + 8;
      if (!strcasecmp(v, "sw") || !strcasecmp(v, "0")) {
        cfg.hwMode = 0;  // Software mode
      } else if (!strcasecmp(v, "hw") || !strcasecmp(v, "1")) {
        cfg.hwMode = 1;  // Hardware mode - legacy (defaults to Timer1)
      } else if (!strcasecmp(v, "hw-t1")) {
        cfg.hwMode = 1;  // Hardware Timer1 (Pin 5/T1)
        // inputIndex is discrete input, NOT GPIO - user must specify via input-dis
      } else if (!strcasecmp(v, "hw-t3")) {
        cfg.hwMode = 3;  // Hardware Timer3 (Pin 47/T3)
      } else if (!strcasecmp(v, "hw-t4")) {
        cfg.hwMode = 4;  // Hardware Timer4 (Pin 6/T4)
      } else if (!strcasecmp(v, "hw-t5")) {
        cfg.hwMode = 5;  // Hardware Timer5 (Pin 46/T5)
      } else {
        Serial.println(F("% Invalid hw-mode (use sw|hw-t1|hw-t3|hw-t4|hw-t5)"));
        return;
      }
      continue;
    }

    // Ukendt parameter
    Serial.print(F("% Unknown parameter: ")); Serial.println(p);
    return;
  }

  if (!counters_config_set(id, cfg)) {
    Serial.println(F("% Could not set counter config"));
    return;
  }

  Serial.print(F("Counter ")); Serial.print(id); Serial.println(F(" configured and enabled"));
}

static void cmd_set(uint8_t ntok, char* tok[]) {
  if (ntok < 2) {
    Serial.println(F("Usage: set {id|baud|server|mode|timer|counter|reg|coil|timers} ..."));
    return;
  }

  if (!strcmp(tok[1],"HOSTNAME")) {
    if (ntok < 3) {
      Serial.println(F("Usage: set hostname <name>"));
      return;
    }
    strncpy(cliHostname, tok[2], sizeof(cliHostname));
    cliHostname[sizeof(cliHostname)-1] = '\0';
    Serial.print(F("OK: hostname set to "));
    Serial.println(cliHostname);
    return;
  }

  // --- Globale timer-status/control registre ---
  // Syntax: set timers status-reg:<n> control-reg:<n>
  if (!strcmp(tok[1], "TIMERS")) {
    if (ntok < 3) {
      Serial.println(F("Usage: set timers status-reg:<n> control-reg:<n>"));
      return;
    }
    for (uint8_t i = 2; i < ntok; i++) {
      char* p = tok[i];

      if (!strncasecmp(p, "status-reg:", 11)) {
        timerStatusRegIndex = (uint16_t)strtoul(p + 11, nullptr, 10);
        Serial.print(F("Timer status-reg = "));
        Serial.println(timerStatusRegIndex);
        continue;
      }
      if (!strncasecmp(p, "control-reg:", 12)) {
        timerStatusCtrlRegIndex = (uint16_t)strtoul(p + 12, nullptr, 10);
        Serial.print(F("Timer control-reg = "));
        Serial.println(timerStatusCtrlRegIndex);
        continue;
      }

      Serial.print(F("% Unknown parameter: "));
      Serial.println(p);
    }
    return;
  }

// --- Fjern et statisk register: "no set reg static <addr>" ---
if (!strcmp(tok[0], "NO") && !strcmp(tok[1], "SET") && !strcmp(tok[2], "REG") && !strcmp(tok[3], "STATIC")) {
  if (ntok < 5) {
    Serial.println(F("Usage: no set reg static <addr>"));
    return;
  }
  uint16_t addr = (uint16_t)strtoul(tok[4], nullptr, 10);
  bool found = false;
  for (uint8_t i = 0; i < regStaticCount; i++) {
    if (regStaticAddr[i] == addr) {
      found = true;
      for (uint8_t j = i; j < regStaticCount - 1; j++) {
        regStaticAddr[j] = regStaticAddr[j + 1];
        regStaticVal[j]  = regStaticVal[j + 1];
      }
      regStaticCount--;
      Serial.print(F("Static reg ")); Serial.print(addr); Serial.println(F(" removed"));
      return;
    }
  }
  if (!found) Serial.println(F("% Static reg not found"));
  return;
}

  // --- Static reg config: set reg static <addr> value <val> ---
  if (!strcmp(tok[1], "REG") && ntok >= 5 && !strcmp(tok[2], "STATIC")) {
    if (ntok < 6) {
      Serial.println(F("Usage: set reg static <addr> value <val>"));
      return;
    }
    uint16_t addr = (uint16_t)strtoul(tok[3], nullptr, 10);
    if (addr >= NUM_REGS) {
      Serial.println(F("% Invalid register address"));
      return;
    }
    uint16_t val = (uint16_t)strtoul(tok[5], nullptr, 10);

    if (!reg_static_upsert(addr, val)) {
      Serial.println(F("% Could not store static reg (limit reached?)"));
      return;
    }
    Serial.print(F("OK: reg STATIC "));
    Serial.print(addr);
    Serial.print(F(" value "));
    Serial.println(val);
    return;
  }
  
// --- Fjern en statisk coil: "no set coil static <idx>" ---
if (!strcmp(tok[0], "NO") && !strcmp(tok[1], "SET") && !strcmp(tok[2], "COIL") && !strcmp(tok[3], "STATIC")) {
  if (ntok < 5) {
    Serial.println(F("Usage: no set coil static <idx>"));
    return;
  }
  uint16_t idx = (uint16_t)strtoul(tok[4], nullptr, 10);
  bool found = false;
  for (uint8_t i = 0; i < coilStaticCount; i++) {
    if (coilStaticIdx[i] == idx) {
      found = true;
      for (uint8_t j = i; j < coilStaticCount - 1; j++) {
        coilStaticIdx[j] = coilStaticIdx[j + 1];
        coilStaticVal[j] = coilStaticVal[j + 1];
      }
      coilStaticCount--;
      Serial.print(F("Static coil ")); Serial.print(idx); Serial.println(F(" removed"));
      return;
    }
  }
  if (!found) Serial.println(F("% Static coil not found"));
  return;
}

  // --- Static coil config: set coil static <idx> <ON|OFF|0|1> ---
  if (!strcmp(tok[1], "COIL") && ntok >= 5 && !strcmp(tok[2], "STATIC")) {
    uint16_t idx = (uint16_t)strtoul(tok[3], nullptr, 10);
    if (idx >= NUM_COILS) {
      Serial.println(F("% Coil index out of range"));
      return;
    }
    const char* v = tok[4];
    uint8_t val;
    if (!strcmp(v, "ON") || !strcmp(v, "1")) {
      val = 1;
    } else if (!strcmp(v, "OFF") || !strcmp(v, "0")) {
      val = 0;
    } else {
      Serial.println(F("% Coil static value must be ON|OFF|0|1"));
      return;
    }

    if (!coil_static_upsert(idx, val)) {
      Serial.println(F("% Could not store static coil (limit reached?)"));
      return;
    }
    Serial.print(F("OK: coil STATIC "));
    Serial.print(idx);
    Serial.print(F(" value "));
    Serial.println(val ? F("ON") : F("OFF"));
    return;
  }

  // ---------------- TIMER ----------------
  if (!strcmp(tok[1],"TIMER")) {
    // Fjern en timer: "no set timer <id>"
    if (!strcmp(tok[0],"NO")) {
      if (ntok < 3) {
        Serial.println(F("Usage: no set timer <id>"));
        return;
      }
      uint8_t id = (uint8_t)strtoul(tok[2], nullptr, 10);
      if (id < 1 || id > 4) {
        Serial.println(F("% Invalid timer id (1..4)"));
        return;
      }
      TimerConfig t;
      if (!timers_get(id, t)) {
        Serial.println(F("% Timer read error"));
        return;
      }
      t.enabled = 0;
      timers_config_set(id, t);
      Serial.print(F("Timer ")); Serial.print(id); Serial.println(F(" removed from configuration"));
      return;
    }

    // Normal "set timer <id> ..."
    if (ntok < 5) {
      Serial.println(F("Usage: set timer <id> mode <n> parameter ..."));
      return;
    }
    uint8_t id = (uint8_t)strtoul(tok[2], nullptr, 10);
    if (id < 1 || id > 4) {
      Serial.println(F("% Invalid timer id (1..4)"));
      return;
    }

    TimerConfig cfg;
    if (!timers_get(id, cfg)) {
      Serial.println(F("% Timer access error"));
      return;
    }

    cfg.enabled = 1;  // implicit enable
    if (cfg.mode < 1 || cfg.mode > 4) cfg.mode = 1;
    if (cfg.subMode < 1 || cfg.subMode > 3) cfg.subMode = 1;

    // parse mode + parameter block
    for (uint8_t i = 3; i < ntok; i++) {
      if (!strcmp(tok[i],"MODE") && (i+1) < ntok) {
        cfg.mode = (uint8_t)strtoul(tok[++i], nullptr, 10);
      }
      else if (!strcmp(tok[i],"PARAMETER")) {
        for (uint8_t j = i+1; j < ntok; j++) {
          char* p = tok[j];
          // Pn:high/low
          if ((p[0]=='P'||p[0]=='p') && (p[1]>='1'&&p[1]<='3') && p[2]==':') {
            bool high = !strcasecmp(p+3,"HIGH");
            if (p[1]=='1') cfg.p1High = high ? 1 : 0;
            if (p[1]=='2') cfg.p2High = high ? 1 : 0;
            if (p[1]=='3') cfg.p3High = high ? 1 : 0;
          }
          // T1/T2/T3
          else if (!strcmp(p,"T1") && (j+1) < ntok) {
            cfg.T1 = (uint32_t)strtoul(tok[++j], nullptr, 10);
          }
          else if (!strcmp(p,"T2") && (j+1) < ntok) {
            cfg.T2 = (uint32_t)strtoul(tok[++j], nullptr, 10);
          }
          else if (!strcmp(p,"T3") && (j+1) < ntok) {
            cfg.T3 = (uint32_t)strtoul(tok[++j], nullptr, 10);
          }
          // coil
          else if (!strcmp(p,"COIL") && (j+1) < ntok) {
            cfg.coil = (uint16_t)strtoul(tok[++j], nullptr, 10);
            if (cfg.coil >= NUM_COILS) {
              Serial.println(F("% Coil index out of range"));
              return;
            }
          }
          // trigger + edge + sub (for mode 4)
          else if (!strcmp(p,"TRIGGER") && (j+1) < ntok) {
            cfg.trigIndex = (uint16_t)strtoul(tok[++j], nullptr, 10);
            if (cfg.trigIndex >= NUM_DISCRETE) {
              Serial.println(F("% Trigger index out of range"));
              return;
            }
          }
          else if (!strcmp(p,"EDGE") && (j+1) < ntok) {
            j++;
            if      (!strcmp(tok[j],"RISING"))  cfg.trigEdge = TRIG_RISING;
            else if (!strcmp(tok[j],"FALLING")) cfg.trigEdge = TRIG_FALLING;
            else if (!strcmp(tok[j],"BOTH"))    cfg.trigEdge = TRIG_BOTH;
          }
          else if (!strcmp(p,"SUB") && (j+1) < ntok) {
            cfg.subMode = (uint8_t)strtoul(tok[++j], nullptr, 10);
            if (cfg.subMode < 1 || cfg.subMode > 3) cfg.subMode = 1;
          }
        }
        break;
      }
    }

    if (!timers_config_set(id, cfg)) {
      Serial.println(F("% Could not set timer config"));
      return;
    }
    Serial.print(F("Timer ")); Serial.print(id); Serial.println(F(" configured and enabled"));
    return;
  }

  // ---------------- COUNTER ----------------
  if (!strcmp(tok[1], "COUNTER")) {
    // "no set counter <id>" for at disable/slette counter
    if (!strcmp(tok[0], "NO")) {
      if (ntok < 3) {
        Serial.println(F("Usage: no set counter <id>"));
        return;
      }
      uint8_t id = (uint8_t)strtoul(tok[2], nullptr, 10);
      if (id < 1 || id > 4) {
        Serial.println(F("% Invalid counter id (1..4)"));
        return;
      }
      CounterConfig c;
      if (!counters_get(id, c)) {
        Serial.println(F("% Counter read error"));
        return;
      }
      c.enabled = 0;
      if (!counters_config_set(id, c)) {
        Serial.println(F("% Could not disable counter"));
        return;
      }
      Serial.print(F("Counter ")); Serial.print(id); Serial.println(F(" removed from configuration"));
      return;
    }

    // "set counter <id> reset-on-read ENABLE/DISABLE"
    if (ntok >= 4 && !strcasecmp(tok[3], "reset-on-read")) {
      if (ntok < 5) {
        Serial.println(F("Usage: set counter <id> reset-on-read ENABLE|DISABLE"));
        return;
      }
      uint8_t id = (uint8_t)strtoul(tok[2], nullptr, 10);
      if (id < 1 || id > 4) {
        Serial.println(F("% Invalid counter id (1..4)"));
        return;
      }
      uint8_t idx = id - 1;

      if (!strcasecmp(tok[4], "ENABLE")) {
        counterResetOnReadEnable[idx] = 1;
        // Sync bit 3 in control register
        if (counters[idx].enabled && counters[idx].controlReg < NUM_REGS) {
          holdingRegs[counters[idx].controlReg] |= 0x0008;
        }
        Serial.print(F("Counter ")); Serial.print(id);
        Serial.println(F(" reset-on-read ENABLED"));
      } else if (!strcasecmp(tok[4], "DISABLE")) {
        counterResetOnReadEnable[idx] = 0;
        // Clear bit 3 in control register
        if (counters[idx].enabled && counters[idx].controlReg < NUM_REGS) {
          holdingRegs[counters[idx].controlReg] &= ~0x0008;
        }
        Serial.print(F("Counter ")); Serial.print(id);
        Serial.println(F(" reset-on-read DISABLED"));
      } else {
        Serial.println(F("% Use ENABLE or DISABLE"));
      }
      return;
    }

    // "set counter <id> start ENABLE/DISABLE"
    if (ntok >= 4 && !strcasecmp(tok[3], "start")) {
      if (ntok < 5) {
        Serial.println(F("Usage: set counter <id> start ENABLE|DISABLE"));
        return;
      }
      uint8_t id = (uint8_t)strtoul(tok[2], nullptr, 10);
      if (id < 1 || id > 4) {
        Serial.println(F("% Invalid counter id (1..4)"));
        return;
      }
      uint8_t idx = id - 1;

      if (!strcasecmp(tok[4], "ENABLE")) {
        counterAutoStartEnable[idx] = 1;
        // Start counter immediately if enabled
        if (counters[idx].enabled) {
          counters[idx].running = 1;
          // Set bit 1 in control register
          if (counters[idx].controlReg < NUM_REGS) {
            holdingRegs[counters[idx].controlReg] |= 0x0002;
          }
        }
        Serial.print(F("Counter ")); Serial.print(id);
        Serial.println(F(" auto-start ENABLED"));
      } else if (!strcasecmp(tok[4], "DISABLE")) {
        counterAutoStartEnable[idx] = 0;
        // Stop counter immediately if enabled
        if (counters[idx].enabled) {
          counters[idx].running = 0;
          // Set bit 2 in control register
          if (counters[idx].controlReg < NUM_REGS) {
            holdingRegs[counters[idx].controlReg] |= 0x0004;
          }
        }
        Serial.print(F("Counter ")); Serial.print(id);
        Serial.println(F(" auto-start DISABLED"));
      } else {
        Serial.println(F("% Use ENABLE or DISABLE"));
      }
      return;
    }

    // Normal "set counter <id> ..." (implicit enable)
    cmd_set_counter(ntok, tok);
    return;
  }

  // --- Andre 'set' parametre ---
  if (!strcmp(tok[1],"ID")) {
    if (ntok < 3) {
      Serial.println(F("Usage: set id <n> (0=ALL or 1..247)"));
      return;
    }
    long idl = strtol(tok[2], nullptr, 10);
    if (idl == 0) {
      listenToAll = true;
      Serial.println(F("OK: slave-id set to 0 (ALL) - respond-to-all DEBUG mode"));
    } else if (idl >= 1 && idl <= 247) {
      listenToAll = false;
      currentSlaveID = (uint8_t)idl;
      Serial.print(F("OK: slave-id set to ")); Serial.println(currentSlaveID);
    } else {
      Serial.println(F("% Invalid Slave ID (use 0 or 1..247)"));
    }
    return;
  }

  if (!strcmp(tok[1],"BAUD")) {
    if (ntok < 3) {
      Serial.println(F("Usage: set baud <n>"));
      return;
    }
    unsigned long nb = strtoul(tok[2], nullptr, 10);
    if (!isSupportedBaud(nb)) {
      Serial.println(F("% Unsupported baudrate"));
      return;
    }
    currentBaudrate = nb;
    MODBUS_SERIAL.end();
    delay(50);
    MODBUS_SERIAL.begin(nb);
    frameGapUs = rtuGapUs();
    Serial.print(F("OK: baudrate set to ")); Serial.println(nb);
    return;
  }

  if (!strcmp(tok[1],"SERVER")) {
    if (ntok != 3) {
      Serial.println(F("Usage: set server on|off"));
      return;
    }
    if (!strcmp(tok[2],"ON")) {
      serverRunning = true;
      Serial.println(F("OK: server RUNNING"));
    }
    else if (!strcmp(tok[2],"OFF")) {
      serverRunning = false;
      Serial.println(F("OK: server STOPPED"));
    }
    else {
      Serial.println(F("% Invalid value (use on|off)"));
    }
    return;
  }

  if (!strcmp(tok[1],"MODE")) {
    if (ntok != 3) {
      Serial.println(F("Usage: set mode server|monitor"));
      return;
    }
    if (!strcmp(tok[2],"SERVER")) {
      monitorMode = false;
      Serial.println(F("OK: mode set to SERVER (active replies)"));
    }
    else if (!strcmp(tok[2],"MONITOR")) {
      monitorMode = true;
      Serial.println(F("OK: mode set to MONITOR (no replies)"));
    }
    else {
      Serial.println(F("% Invalid mode (use server|monitor)"));
    }
    return;
  }

  Serial.println(F("% Unknown parameter for 'set'"));
}


// ---------- Persistence ----------
static void cmd_persist(const char* verb) {
  if (!strcmp(verb,"SAVE")) {
    // Use global config to avoid stack overflow
    memset(&globalConfig, 0, sizeof(globalConfig));
    globalConfig.magic      = 0xC0DE;
    globalConfig.schema     = 6;   // schema v6 for CounterEngine v3 (+ debounce)
    globalConfig.reserved   = 0;
    globalConfig.slaveId    = currentSlaveID;
    globalConfig.serverFlag = serverRunning ? 1 : 0;
    globalConfig.baud       = currentBaudrate;
    globalConfig.timerStatusReg     = timerStatusRegIndex;
    globalConfig.timerStatusCtrlReg = timerStatusCtrlRegIndex;

    // statiske maps
    globalConfig.regStaticCount  = regStaticCount;
    for (uint8_t i = 0; i < globalConfig.regStaticCount && i < MAX_STATIC_REGS; i++) {
      globalConfig.regStaticAddr[i] = regStaticAddr[i];
      globalConfig.regStaticVal [i] = regStaticVal [i];
    }
    globalConfig.coilStaticCount = coilStaticCount;
    for (uint8_t i = 0; i < globalConfig.coilStaticCount && i < MAX_STATIC_COILS; i++) {
      globalConfig.coilStaticIdx[i] = coilStaticIdx[i];
      globalConfig.coilStaticVal[i] = coilStaticVal[i] ? 1 : 0;
    }

    // timere: gem alle 4
    globalConfig.timerCount = 4;
    for (uint8_t i = 0; i < 4; i++) {
      globalConfig.timer[i] = timers[i];
    }

    // Counters: gem alle 4 (enabled-flag og config afgør brugen)
    globalConfig.counterCount = 4;
    for (uint8_t i = 0; i < 4; i++) {
      globalConfig.counter[i] = counters[i];
    }

    // gem GPIO mapping
    for (uint8_t i = 0; i < MAX_GPIO_PINS; i++) {
      globalConfig.gpioToCoil[i]  = gpioToCoil[i];
      globalConfig.gpioToInput[i] = gpioToInput[i];
    }

    if (configSave(globalConfig)) Serial.println(F("OK: config saved to EEPROM"));
    else                          Serial.println(F("% Save failed"));
    return;
  }

  if (!strcmp(verb,"LOAD")) {
    // Use global config to avoid stack overflow
    if (configLoad(globalConfig)) {
      configApply(globalConfig);
      Serial.println(F("OK: config loaded and applied"));
    } else {
      Serial.println(F("% Invalid EEPROM config (use 'defaults' to reset)"));
    }
    return;
  }

  if (!strcmp(verb,"DEFAULTS")) {
    // Use global config to avoid stack overflow
    configDefaults(globalConfig);
    if (configSave(globalConfig)) {
      configApply(globalConfig);
      Serial.println(F("OK: defaults applied & saved"));
    } else {
      Serial.println(F("% Could not save defaults"));
    }
    return;
  }
}

// ---------- Help ----------
static void cmd_help() {
  Serial.println(F("=== COMMANDS ==="));
  Serial.println(F(" show config|stats|regs|coils|inputs|timers|counters|version|gpio"));
  Serial.println(F(" save | load | defaults"));
  Serial.println(F(" reboot                      - Restart systemet (software reset)"));
  Serial.println(F(" ---"));
  Serial.println(F(" read reg <addr> [qty=1]"));
  Serial.println(F(" read coil <idx> [qty=1]"));
  Serial.println(F(" read input <idx> [qty=1]"));
  Serial.println(F(" write reg <addr> <value>"));
  Serial.println(F(" write coil <idx> <0|1>"));
  Serial.println(F(" dump regs|coils|inputs"));
  Serial.println(F(" set reg static <addr> value <val>"));
  Serial.println(F(" set coil static <idx> <ON|OFF|0|1>"));
  Serial.println(F(" ---"));
  Serial.println(F(" set id <n> (0 = respond-all, or 1..247)"));
  Serial.println(F(" set baud <n>"));
  Serial.println(F(" set server on|off"));
  Serial.println(F(" set mode server|monitor"));
  Serial.println(F(" ---"));
  Serial.println(F(" set timer <id> mode <1|2|3|4> parameter P1:<high|low> P2:<high|low> [P3:<high|low>]"));
  Serial.println(F("   T1 <ms> [T2 <ms>] [T3 <ms>] coil <idx> [trigger <di_idx> edge rising|falling|both sub <1|2|3>]"));
  Serial.println(F(" set timers status-reg:<n> "));
  Serial.println(F(" set control-reg:<n> "));
  Serial.println(F(" no set timer <id>           - remove timer from configuration"));
  Serial.println(F(" ---"));
  Serial.println(F(" set counter <id> mode 1 parameter count-on:<rising|falling|both>"));
  Serial.println(F("   start-value:<n> res|resolution:<8|16|32|64> prescaler:<1..256>"));
  Serial.println(F("   index-reg:<reg> raw-reg:<reg> freq-reg:<reg> overload-reg:<reg> ctrl-reg:<reg>"));
  Serial.println(F("   input-dis:<di_idx> direction:<up|down> scale:<float>"));
  Serial.println(F("   debounce:<on|off> [debounce-ms:<ms>]"));
  Serial.println(F("   hw-mode:<sw|hw-t1|hw-t3|hw-t4|hw-t5> [v3.3.0 - select HW timer (T1=pin5, T3=pin47, T4=pin6, T5=pin46)]"));
  Serial.println(F(" set counter <id> reset-on-read ENABLE|DISABLE"));
  Serial.println(F("   - Enable/disable counter reset-on-read (bit 3 in control register)"));
  Serial.println(F(" set counter <id> start ENABLE|DISABLE"));
  Serial.println(F("   - Enable/disable counter (starts immediately and on boot)"));
  Serial.println(F(" no set counter <id>         - remove counter from configuration"));
  Serial.println(F(" reset counter <id>          - reset selected counter"));
  Serial.println(F(" clear counters              - reset all counters and overflow flags"));
  Serial.println(F(" -- Bitmask controlReg (counter): --"));
  Serial.println(F("  bit0 = reset  (load start-value, clear overflow)"));
  Serial.println(F("  bit1 = start  (start counting)"));
  Serial.println(F("  bit2 = stop   (stop counting)"));
  Serial.println(F("  bit3 = reset-on-read enable (sticky - saved to EEPROM)"));
  Serial.println(F("  NOTE: All bits writable via Modbus FC6, but only bit3 persists"));
  Serial.println(F(" ---"));
  Serial.println();
  Serial.println(F(" Examples:"));
  Serial.println(F("  set timer 1 mode 1 parameter P1:low P2:high P3:low T1 1000 T2 500 T3 1000 coil 5"));
  Serial.println(F("  set timer 3 mode 3 parameter P1:low P2:high T1 300 T2 700 coil 10"));
  Serial.println(F("  set timer 4 mode 4 parameter P1:low P2:high T1 200 T2 300 coil 15 trigger 12 edge rising sub 1"));
  Serial.println(F("  no set timer 1"));
  Serial.println();
  Serial.println(F("  set counter 1 mode 1 parameter count-on:rising start-value:0 res:32 prescaler:1"));
  Serial.println(F("   index-reg:100 raw-reg:104 freq-reg:108 overload-reg:120 ctrl-reg:130"));
  Serial.println(F("   input-dis:12 direction:down scale:2.500 debounce:on debounce-ms:25"));
  Serial.println();
  Serial.println(F("  set reg static 150 value 1234"));
  Serial.println(F("  set coil static 10 ON"));
  Serial.println(F("  gpio map 20 input 12"));
  Serial.println(F("  gpio unmap 20"));
  Serial.println();
  Serial.println(F("================="));
  Serial.println(F("Hardware Interrupt capable pins (Arduino Mega2560):"));
  Serial.println(F("  INT0 : Pin 2,3,18,19,20,21"));

}

static void cli_print_extended_help() {
  Serial.println(F(" show inputs                         - show all discrete inputs"));
  Serial.println(F(" read input <idx> [qty]              - read one or more discrete inputs"));
  Serial.println(F(" dump inputs                         - dump all discrete inputs"));
  Serial.println(F(" gpio map <pin> coil|input <idx>     - map GPIO pin to coil or input"));
  Serial.println(F(" gpio unmap <pin>                    - unmap GPIO pin"));
  Serial.println(F(" show gpio                           - show active GPIO mappings"));
  Serial.println(F(" show timers                         - show active timer mappings/status"));
  Serial.println(F(" show counters                       - show active counters + control register status"));
}

// ---------- Counter helper commands ----------
static void cmd_reset_counter(uint8_t ntok, char* tok[]) {
  if (ntok != 3) {
    Serial.println(F("Usage: reset counter <id>"));
    return;
  }
  uint8_t id = (uint8_t)strtoul(tok[2], nullptr, 10);
  if (id < 1 || id > 4) {
    Serial.println(F("% Invalid counter id (1..4)"));
    return;
  }
  counters_reset(id);
  Serial.print(F("Counter ")); Serial.print(id); Serial.println(F(" reset"));
}

static void cmd_clear_counters(uint8_t ntok, char* tok[]) {
  if (ntok != 2 || strcmp("COUNTERS", tok[1])) {
    Serial.println(F("Usage: clear counters"));
    return;
  }
  counters_clear_all();
  Serial.println(F("All counters cleared"));
}

static void cli_show_gpio() {
  Serial.println(F("=== GPIO MAPPINGS ==="));
  bool any = false;
  for (uint8_t pin = 0; pin < MAX_GPIO_PINS; ++pin) {
    int16_t ci = gpioToCoil[pin];
    int16_t di = gpioToInput[pin];
    if (ci != -1 || di != -1) {
      any = true;
      Serial.print(F("PIN ")); Serial.print(pin); Serial.print(F(": "));
      if (ci != -1) {
        Serial.print(F("coil ")); Serial.print(ci);
      }
      if (di != -1) {
        if (ci != -1) Serial.print(F(", "));
        Serial.print(F("input ")); Serial.print(di);
      }
      Serial.println();
    }
  }
  if (!any) {
    Serial.println(F("(no GPIO mappings)"));
  }
}

// ---------- GPIO command ----------
static void cmd_gpio(uint8_t ntok, char* tok[]) {
  if (ntok < 2) {
    Serial.println(F("Usage: gpio {map|unmap} ..."));
    return;
  }

  if (!strcmp(tok[1],"MAP")) {
    if (ntok != 5) {
      Serial.println(F("Usage: gpio map <pin> coil|input <idx>"));
      return;
    }
    uint8_t pin = (uint8_t)strtoul(tok[2], nullptr, 10);
    if (pin >= MAX_GPIO_PINS) {
      Serial.println(F("% GPIO pin out of range (0..53)"));
      return;
    }
    if (!strcmp(tok[3],"COIL")) {
      uint16_t idx = (uint16_t)strtoul(tok[4], nullptr, 10);
      if (idx >= NUM_COILS) {
        Serial.println(F("% Coil index out of range"));
        return;
      }
      gpioToCoil[pin]  = (int16_t)idx;
      gpioToInput[pin] = -1;
      pinMode(pin, INPUT);
      Serial.print(F("OK: pin ")); Serial.print(pin); Serial.print(F(" mapped to COIL "));
      Serial.println(idx);
      return;
    }
    if (!strcmp(tok[3],"INPUT")) {
      uint16_t idx = (uint16_t)strtoul(tok[4], nullptr, 10);
      if (idx >= NUM_DISCRETE) {
        Serial.println(F("% Input index out of range"));
        return;
      }
      gpioToInput[pin] = (int16_t)idx;
      gpioToCoil[pin]  = -1;
      pinMode(pin, INPUT);
      Serial.print(F("OK: pin ")); Serial.print(pin); Serial.print(F(" mapped to INPUT "));
      Serial.println(idx);
      return;
    }
    Serial.println(F("% Unknown target for gpio map (use coil|input)"));
    return;
  }

  if (!strcmp(tok[1],"UNMAP")) {
    if (ntok != 3) {
      Serial.println(F("Usage: gpio unmap <pin>"));
      return;
    }
    uint8_t pin = (uint8_t)strtoul(tok[2], nullptr, 10);
    if (pin >= MAX_GPIO_PINS) {
      Serial.println(F("% GPIO pin out of range (0..53)"));
      return;
    }
    gpioToCoil[pin]  = -1;
    gpioToInput[pin] = -1;
    Serial.print(F("OK: pin ")); Serial.print(pin); Serial.println(F(" unmapped"));
    return;
  }

  Serial.println(F("% Unknown 'gpio' command (use map|unmap)"));
}

// ---------- CLI Loop ----------
void cli_loop() {
  static char line[CMD_LINE_MAX];
  static uint8_t idx = 0;
  static char savedLine[CMD_LINE_MAX];  // Save current input when navigating history
  static uint8_t escState = 0;  // 0=normal, 1=got ESC, 2=got ESC[

  while (Serial.available()) {
    char c = Serial.read();

    if (CLI_DEBUG_ECHO) {
      Serial.print(F("[CLI RX: "));
      if (c >= 32 && c <= 126) Serial.print(c);
      else {
        Serial.print(F("0x"));
        Serial.print((uint8_t)c, HEX);
      }
      Serial.println(F("]"));
    }

    // ANSI escape sequence state machine
    if (escState == 0 && c == 0x1B) {  // ESC
      escState = 1;
      continue;
    }
    if (escState == 1 && c == '[') {   // ESC [
      escState = 2;
      continue;
    }
    if (escState == 2) {
      escState = 0;

      // Arrow Up (ESC [ A)
      if (c == 'A') {
        if (cmdHistoryCount == 0) continue;

        // Save current line on first navigation
        if (cmdHistoryNav == -1) {
          strncpy(savedLine, line, sizeof(savedLine));
          savedLine[sizeof(savedLine) - 1] = '\0';
          cmdHistoryNav = 0;
        } else if (cmdHistoryNav < cmdHistoryCount - 1) {
          cmdHistoryNav++;
        } else {
          continue;  // Already at oldest command
        }

        const char* histCmd = getHistoryCmd(cmdHistoryNav);
        if (histCmd) {
          // Clear current line on terminal
          while (idx > 0) {
            Serial.write(8); Serial.write(' '); Serial.write(8);
            idx--;
          }
          // Copy and display history command
          strncpy(line, histCmd, sizeof(line) - 1);
          line[sizeof(line) - 1] = '\0';
          idx = strlen(line);
          Serial.print(line);
        }
        continue;
      }

      // Arrow Down (ESC [ B)
      if (c == 'B') {
        if (cmdHistoryNav <= -1) continue;  // Not navigating

        if (cmdHistoryNav > 0) {
          cmdHistoryNav--;
          const char* histCmd = getHistoryCmd(cmdHistoryNav);
          if (histCmd) {
            // Clear current line on terminal
            while (idx > 0) {
              Serial.write(8); Serial.write(' '); Serial.write(8);
              idx--;
            }
            // Copy and display history command
            strncpy(line, histCmd, sizeof(line) - 1);
            line[sizeof(line) - 1] = '\0';
            idx = strlen(line);
            Serial.print(line);
          }
        } else {
          // Restore saved line (back to current input)
          cmdHistoryNav = -1;
          while (idx > 0) {
            Serial.write(8); Serial.write(' '); Serial.write(8);
            idx--;
          }
          strncpy(line, savedLine, sizeof(line));
          idx = strlen(line);
          Serial.print(line);
        }
        continue;
      }

      // Ignore other escape sequences
      continue;
    }

    // Reset escape state if we got something else
    if (escState > 0) escState = 0;

    // Handle backspace (ASCII 8 or 127)
    if (c == 8 || c == 127) {
      if (idx > 0) {
        idx--;
        // Send backspace, space, backspace to erase character on terminal
        Serial.write(8);   // move cursor back
        Serial.write(' '); // overwrite with space
        Serial.write(8);   // move cursor back again
      }
      continue;
    }

    if (c == '\r' || c == '\n') {
      if (idx == 0) {
        print_prompt();
        cmdHistoryNav = -1;  // Reset history navigation
        continue;
      }
      line[idx] = '\0';

      // Save original command before uppercasing
      char originalLine[CMD_LINE_MAX];
      strncpy(originalLine, line, sizeof(originalLine) - 1);
      originalLine[sizeof(originalLine) - 1] = '\0';

      idx = 0;
      cmdHistoryNav = -1;  // Reset history navigation

      toupper_ascii(line);
      for (char* s = line; *s; ++s) {
  // Normalize dashes and clean control chars
  if (*s == 0x96 || *s == 0x97) *s = '-';   // UTF-8 dash variants
  if ((unsigned char)*s < 32 || (unsigned char)*s > 126) *s = ' ';
}

      char* tok[40] = {0};
      uint8_t ntok = tokenize(line, tok, 40);
      if (ntok == 0) {
        print_prompt();
        continue;
      }
      normalize_tokens(ntok, tok);

      // Add to history (after normalization, but before execution)
      addToHistory(originalLine);

      if (!strcmp(tok[0],"EXIT")) {
        s_cli = false;
        Serial.println(F("Leaving CLI mode."));
        return;
      }
      else if (!strcmp(tok[0],"HELP")) {
        cmd_help();
        cli_print_extended_help();
      }
      else if (!strcmp(tok[0],"SHOW"))             cmd_show(ntok, tok);
      else if (!strcmp(tok[0],"READ"))             cmd_read(ntok, tok);
      else if (!strcmp(tok[0],"WRITE"))            cmd_write(ntok, tok);
      else if (!strcmp(tok[0],"DUMP"))             cmd_dump(ntok, tok);
      else if (!strcmp(tok[0],"SET") || !strcmp(tok[0],"NO")) cmd_set(ntok, tok);
      else if (!strcmp(tok[0],"SAVE") || !strcmp(tok[0],"LOAD") || !strcmp(tok[0],"DEFAULTS")) {
        cmd_persist(tok[0]);
      }
      else if (!strcmp(tok[0],"GPIO"))             cmd_gpio(ntok, tok);
      else if (!strcmp(tok[0],"RESET") && ntok >= 2 && !strcmp(tok[1],"COUNTER")) {
        cmd_reset_counter(ntok, tok);
      }
      else if (!strcmp(tok[0],"CLEAR") && ntok >= 2 && !strcmp(tok[1],"COUNTERS")) {
        cmd_clear_counters(ntok, tok);
      }
      else if (!strcmp(tok[0],"REBOOT")) {
        Serial.println(F("System rebooting..."));
        delay(100);

        // Aktiver watchdog for at fremtvinge MCU-reset
        wdt_enable(WDTO_15MS);
        while (true) { } // vent på reset
      }

      else {
        Serial.println(F("% Unknown command. Type 'help'"));
      }

      print_prompt();
    }

    else if (c >= 32 && c < 127 && idx < sizeof(line) - 1) {
      line[idx++] = c;
      // Echo character back to terminal (remote echo)
      Serial.write(c);
    }
  }
}

// ---------- Try enter CLI ----------
void cli_try_enter() {
  if (s_cli) return;

  static char buf[24];
  static uint8_t idx = 0;

  while (Serial.available()) {
    char c = Serial.read();

    if (CLI_DEBUG_ECHO) {
      Serial.print(F("[TRY RX: "));
      if (c >= 32 && c <= 126) Serial.print(c);
      else {
        Serial.print(F("0x"));
        Serial.print((uint8_t)c, HEX);
      }
      Serial.println(F("]"));
    }

    // Handle backspace (ASCII 8 or 127)
    if (c == 8 || c == 127) {
      if (idx > 0) {
        idx--;
        // Send backspace, space, backspace to erase character on terminal
        Serial.write(8);   // move cursor back
        Serial.write(' '); // overwrite with space
        Serial.write(8);   // move cursor back again
      }
      continue;
    }

    if (c == '\r' || c == '\n') {
      if (idx == 0) continue;
      buf[idx] = '\0';
      idx = 0;

      toupper_ascii(buf);
      if (!strcmp(buf,"CLI")) {
        s_cli = true;
        Serial.println(F("\r\nEntering CLI mode. Type HELP for commands."));
        print_prompt();
      }
    }
    else if (c >= 32 && c < 127 && idx < sizeof(buf) - 1) {
      buf[idx++] = c;
      // Echo character back to terminal (remote echo)
      Serial.write(c);
    }
  }
}
