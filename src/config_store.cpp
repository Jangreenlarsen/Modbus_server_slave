// ============================================================================
//  Filnavn : config_store.cpp
//  Projekt  : Modbus RTU Server / CLI
//  Version  : v3.3.0 (Hybrid HW/SW Counter Engine)
//  Forfatter: JanG at modbus_slave@laces.dk
//  Formål   : EEPROM persistence – schema v10 (CounterEngine v4 with hw-mode)
// ============================================================================
#include "modbus_core.h"
#include "modbus_globals.h"
#include "modbus_timers.h"
#include "modbus_counters.h"
#include <EEPROM.h>
#include <string.h>

// ============================================================================
//  Hjælpefunktioner
// ============================================================================
static uint16_t crc16_simple(const uint8_t *data, size_t len) {
  uint16_t s = 0;
  for (size_t i=0; i<len; i++) s += data[i];
  return s;
}

static void computeFillCrc(PersistConfig &cfg) {
  const size_t len = sizeof(PersistConfig) - sizeof(cfg.crc);
  cfg.crc = crc16_simple(reinterpret_cast<const uint8_t*>(&cfg), len);
}

static bool checkCrc(const PersistConfig &cfg) {
  const size_t len = sizeof(PersistConfig) - sizeof(cfg.crc);
  uint16_t c = crc16_simple(reinterpret_cast<const uint8_t*>(&cfg), len);
  return (c == cfg.crc);
}

static bool isSupportedBaud(unsigned long nb) __attribute__((unused));
static bool isSupportedBaud(unsigned long nb) {
  const unsigned long rates[] = {
    300,600,1200,2400,4800,9600,14400,19200,38400,57600,115200
  };
  for (uint8_t i=0; i<sizeof(rates)/sizeof(rates[0]); i++)
    if (nb == rates[i]) return true;
  return false;
}

// ============================================================================
//  LOAD
// ============================================================================
bool configLoad(PersistConfig &cfg) {
  EEPROM.get(0, cfg);

  // Check magic and CRC
  if (cfg.magic != 0xC0DE) {
    Serial.print(F("! EEPROM magic invalid (got 0x"));
    Serial.print(cfg.magic, HEX);
    Serial.println(F(")"));
    configDefaults(cfg);
    return false;  // Let main.cpp handle save
  }

  // Schema check BEFORE CRC (CRC layout may have changed)
  if (cfg.schema != 10 && cfg.schema != 11 && cfg.schema != 12) {
    Serial.print(F("! EEPROM schema unknown (got "));
    Serial.print(cfg.schema);
    Serial.println(F(")"));
    configDefaults(cfg);
    return false;  // Let main.cpp handle save
  }

  // For schema 10->11 migration: GPIO arrays removed from persistence
  if (cfg.schema == 10) {
    Serial.println(F("! EEPROM schema 10 (old) - upgrading to 11 (GPIO no longer persisted)"));

    // CRITICAL: Save ALL timer and counter configs BEFORE calling configDefaults()
    TimerConfig oldTimers[4];
    CounterConfig oldCounters[4];
    uint8_t oldTimerCount = cfg.timerCount;
    uint8_t oldCounterCount = cfg.counterCount;

    for (uint8_t i = 0; i < 4; i++) {
      oldTimers[i] = cfg.timer[i];
      oldCounters[i] = cfg.counter[i];
    }

    // Save the old settings we want to keep
    uint8_t oldSlaveId = cfg.slaveId;
    uint32_t oldBaud = cfg.baud;
    uint8_t oldServerFlag = cfg.serverFlag;
    uint8_t oldRegStaticCount = cfg.regStaticCount;
    uint8_t oldCoilStaticCount = cfg.coilStaticCount;

    uint16_t oldRegAddr[MAX_STATIC_REGS];
    uint16_t oldRegVal[MAX_STATIC_REGS];
    for (uint8_t i = 0; i < oldRegStaticCount && i < MAX_STATIC_REGS; i++) {
      oldRegAddr[i] = cfg.regStaticAddr[i];
      oldRegVal[i] = cfg.regStaticVal[i];
    }

    uint16_t oldCoilIdx[MAX_STATIC_COILS];
    uint8_t oldCoilVal[MAX_STATIC_COILS];
    for (uint8_t i = 0; i < oldCoilStaticCount && i < MAX_STATIC_COILS; i++) {
      oldCoilIdx[i] = cfg.coilStaticIdx[i];
      oldCoilVal[i] = cfg.coilStaticVal[i];
    }

    // Now reset to defaults (this will overwrite everything)
    configDefaults(cfg);

    // Restore old settings
    cfg.slaveId = oldSlaveId;
    cfg.baud = oldBaud;
    cfg.serverFlag = oldServerFlag;
    cfg.timerCount = oldTimerCount;
    cfg.counterCount = oldCounterCount;

    cfg.regStaticCount = oldRegStaticCount;
    for (uint8_t i = 0; i < oldRegStaticCount && i < MAX_STATIC_REGS; i++) {
      cfg.regStaticAddr[i] = oldRegAddr[i];
      cfg.regStaticVal[i] = oldRegVal[i];
    }

    cfg.coilStaticCount = oldCoilStaticCount;
    for (uint8_t i = 0; i < oldCoilStaticCount && i < MAX_STATIC_COILS; i++) {
      cfg.coilStaticIdx[i] = oldCoilIdx[i];
      cfg.coilStaticVal[i] = oldCoilVal[i];
    }

    // Restore timer and counter configs
    for (uint8_t i = 0; i < oldTimerCount && i < 4; i++) {
      cfg.timer[i] = oldTimers[i];
    }
    for (uint8_t i = 0; i < oldCounterCount && i < 4; i++) {
      cfg.counter[i] = oldCounters[i];
    }

    // Recompute CRC after migration restore
    computeFillCrc(cfg);

    // CRITICAL: Return false so main.cpp saves the migrated config back with schema=11
    // But cfg now contains the restored timer/counter data, not defaults!
    // main.cpp will call configSave() which updates schema=11 and re-saves
    return false;
  }

  // For schema 11->12 migration: GPIO mappings added back
  if (cfg.schema == 11) {
    Serial.println(F("! EEPROM schema 11 (old) - upgrading to 12 (GPIO persistence restored)"));

    // Save timer/counter configs before migration
    TimerConfig oldTimers[4];
    CounterConfig oldCounters[4];
    uint8_t oldTimerCount = cfg.timerCount;
    uint8_t oldCounterCount = cfg.counterCount;

    for (uint8_t i = 0; i < 4; i++) {
      oldTimers[i] = cfg.timer[i];
      oldCounters[i] = cfg.counter[i];
    }

    // Initialize GPIO mappings (they were lost in v11)
    for (uint8_t i = 0; i < NUM_GPIO; i++) {
      cfg.gpioToCoil[i]  = -1;
      cfg.gpioToInput[i] = -1;
    }

    // Restore timer/counter configs
    cfg.timerCount = oldTimerCount;
    cfg.counterCount = oldCounterCount;
    for (uint8_t i = 0; i < 4; i++) {
      cfg.timer[i] = oldTimers[i];
      cfg.counter[i] = oldCounters[i];
    }

    // Recompute CRC after migration
    computeFillCrc(cfg);

    // Return false so main.cpp saves with schema=12
    return false;
  }

  // Schema 12 validation
  if (cfg.schema == 12) {
    if (!checkCrc(cfg)) {
      Serial.print(F("! EEPROM CRC invalid (expected 0x"));
      Serial.print(cfg.crc, HEX);
      uint16_t computed = 0;
      // Quick CRC recompute to show difference
      for (uint16_t i = 0; i < sizeof(PersistConfig) - 2; i++) {
        computed += ((uint8_t*)&cfg)[i];
      }
      Serial.print(F(" computed 0x"));
      Serial.print(computed, HEX);
      Serial.println(F(")"));
      configDefaults(cfg);
      return false;  // Let main.cpp handle save
    }

    // DEBUG: Show EVERYTHING we're loading
    Serial.println(F("\n=== DEBUG: configLoad() - FULL CONFIG ==="));
    Serial.print(F("Magic: 0x")); Serial.print(cfg.magic, HEX);
    Serial.print(F(" | Schema: ")); Serial.print(cfg.schema);
    Serial.print(F(" | CRC: 0x")); Serial.println(cfg.crc, HEX);
    Serial.print(F("SlaveID: ")); Serial.print(cfg.slaveId);
    Serial.print(F(" | Baud: ")); Serial.print(cfg.baud);
    Serial.print(F(" | Server: ")); Serial.println(cfg.serverFlag);
    Serial.println(F("------ STATIC REGISTERS ------"));
    Serial.print(F("Count: ")); Serial.println(cfg.regStaticCount);
    for (uint8_t i = 0; i < cfg.regStaticCount; i++) {
      Serial.print(F("  [") ); Serial.print(i); Serial.print(F("] Addr="));
      Serial.print(cfg.regStaticAddr[i]); Serial.print(F(" Val="));
      Serial.println(cfg.regStaticVal[i]);
    }
    Serial.println(F("------ STATIC COILS ------"));
    Serial.print(F("Count: ")); Serial.println(cfg.coilStaticCount);
    for (uint8_t i = 0; i < cfg.coilStaticCount; i++) {
      Serial.print(F("  [") ); Serial.print(i); Serial.print(F("] Idx="));
      Serial.print(cfg.coilStaticIdx[i]); Serial.print(F(" Val="));
      Serial.println(cfg.coilStaticVal[i]);
    }
    Serial.println(F("------ TIMERS ------"));
    Serial.print(F("Count: ")); Serial.println(cfg.timerCount);
    Serial.print(F("Status Reg: ")); Serial.print(cfg.timerStatusReg);
    Serial.print(F(" | Ctrl Reg: ")); Serial.println(cfg.timerStatusCtrlReg);
    for (uint8_t i = 0; i < cfg.timerCount; i++) {
      Serial.print(F("  [") ); Serial.print(i); Serial.print(F("] ID="));
      Serial.print(cfg.timer[i].id); Serial.print(F(" Mode="));
      Serial.print(cfg.timer[i].mode); Serial.print(F(" Enabled="));
      Serial.println(cfg.timer[i].enabled);
    }
    Serial.println(F("------ COUNTERS ------"));
    Serial.print(F("Count: ")); Serial.println(cfg.counterCount);
    for (uint8_t i = 0; i < cfg.counterCount; i++) {
      Serial.print(F("  [") ); Serial.print(i); Serial.print(F("] ID="));
      Serial.print(cfg.counter[i].id); Serial.print(F(" Enabled="));
      Serial.print(cfg.counter[i].enabled); Serial.print(F(" EdgeMode="));
      Serial.print(cfg.counter[i].edgeMode); Serial.print(F(" Dir="));
      Serial.print(cfg.counter[i].direction); Serial.print(F(" Input="));
      Serial.print(cfg.counter[i].inputIndex); Serial.print(F(" Reg="));
      Serial.println(cfg.counter[i].regIndex);
    }
    Serial.println(F("------ GPIO MAPPINGS ------"));
    bool hasGpio = false;
    for (uint8_t i = 0; i < NUM_GPIO; i++) {
      if (cfg.gpioToCoil[i] >= 0 || cfg.gpioToInput[i] >= 0) {
        Serial.print(F("  Pin ")); Serial.print(i);
        if (cfg.gpioToCoil[i] >= 0) {
          Serial.print(F(" -> coil ")); Serial.print(cfg.gpioToCoil[i]);
        }
        if (cfg.gpioToInput[i] >= 0) {
          Serial.print(F(" -> input ")); Serial.print(cfg.gpioToInput[i]);
        }
        Serial.println();
        hasGpio = true;
      }
    }
    if (!hasGpio) Serial.println(F("  (no GPIO mappings)"));
    Serial.println(F("==========================================\n"));

    return true;
  }

  // Fallback
  configDefaults(cfg);
  return false;
}

// ============================================================================
//  DEFAULTS
// ============================================================================
void configDefaults(PersistConfig &cfg) {
  memset(&cfg, 0, sizeof(cfg));
  cfg.magic      = 0xC0DE;
  cfg.schema     = 12;  // v12: GPIO mappings restored to persistence
  cfg.slaveId    = SLAVE_ID;
  cfg.serverFlag = 1;
  cfg.baud       = BAUDRATE;

  cfg.regStaticCount  = 0;
  cfg.coilStaticCount = 0;
  cfg.timerCount      = 0;
  cfg.counterCount    = 0;

  cfg.timerStatusReg     = 140;
  cfg.timerStatusCtrlReg = 141;

  for (uint8_t i=0; i<4; i++) {
    memset(&cfg.timer[i],   0, sizeof(TimerConfig));
    memset(&cfg.counter[i], 0, sizeof(CounterConfig));
    cfg.counterResetOnReadEnable[i] = 0;  // default: disabled
    cfg.counterAutoStartEnable[i]   = 0;  // default: disabled (manual start)
  }

  // Initialize GPIO mappings (all unmapped)
  for (uint8_t i=0; i<NUM_GPIO; i++) {
    cfg.gpioToCoil[i]  = -1;
    cfg.gpioToInput[i] = -1;
  }

  computeFillCrc(cfg);
}

// ============================================================================
//  SAVE
// ============================================================================
// Use globalConfig directly to save RAM (passed by reference)
// Only need one static temp for verification
static PersistConfig verifyTempConfig;

bool configSave(const PersistConfig &cfgIn) {
  // Cast away const to work with cfgIn directly (saves 1KB RAM)
  PersistConfig &cfg = const_cast<PersistConfig&>(cfgIn);

  // Gem globale timer status/control registre
  cfg.timerStatusReg     = timerStatusRegIndex;
  cfg.timerStatusCtrlReg = timerStatusCtrlRegIndex;

  strncpy(cfg.hostname, cliHostname, sizeof(cfg.hostname));

  // Gem counter reset-on-read enable flags
  for (uint8_t i = 0; i < 4; ++i) {
    cfg.counterResetOnReadEnable[i] = counterResetOnReadEnable[i];
    cfg.counterAutoStartEnable[i] = counterAutoStartEnable[i];
  }

  // Gem GPIO mappings
  for (uint8_t i = 0; i < NUM_GPIO; ++i) {
    cfg.gpioToCoil[i]  = gpioToCoil[i];
    cfg.gpioToInput[i] = gpioToInput[i];
  }

  cfg.schema = 12;  // v12: GPIO mappings restored to persistence
  computeFillCrc(cfg);

  // DEBUG: Show EVERYTHING we're saving
  Serial.println(F("\n=== DEBUG: configSave() - FULL CONFIG ==="));
  Serial.print(F("Magic: 0x")); Serial.print(cfg.magic, HEX);
  Serial.print(F(" | Schema: ")); Serial.print(cfg.schema);
  Serial.print(F(" | CRC: 0x")); Serial.println(cfg.crc, HEX);
  Serial.print(F("SlaveID: ")); Serial.print(cfg.slaveId);
  Serial.print(F(" | Baud: ")); Serial.print(cfg.baud);
  Serial.print(F(" | Server: ")); Serial.println(cfg.serverFlag);
  Serial.println(F("------ STATIC REGISTERS ------"));
  Serial.print(F("Count: ")); Serial.println(cfg.regStaticCount);
  for (uint8_t i = 0; i < cfg.regStaticCount; i++) {
    Serial.print(F("  [") ); Serial.print(i); Serial.print(F("] Addr="));
    Serial.print(cfg.regStaticAddr[i]); Serial.print(F(" Val="));
    Serial.println(cfg.regStaticVal[i]);
  }
  Serial.println(F("------ STATIC COILS ------"));
  Serial.print(F("Count: ")); Serial.println(cfg.coilStaticCount);
  for (uint8_t i = 0; i < cfg.coilStaticCount; i++) {
    Serial.print(F("  [") ); Serial.print(i); Serial.print(F("] Idx="));
    Serial.print(cfg.coilStaticIdx[i]); Serial.print(F(" Val="));
    Serial.println(cfg.coilStaticVal[i]);
  }
  Serial.println(F("------ TIMERS ------"));
  Serial.print(F("Count: ")); Serial.println(cfg.timerCount);
  Serial.print(F("Status Reg: ")); Serial.print(cfg.timerStatusReg);
  Serial.print(F(" | Ctrl Reg: ")); Serial.println(cfg.timerStatusCtrlReg);
  for (uint8_t i = 0; i < cfg.timerCount; i++) {
    Serial.print(F("  [") ); Serial.print(i); Serial.print(F("] ID="));
    Serial.print(cfg.timer[i].id); Serial.print(F(" Mode="));
    Serial.print(cfg.timer[i].mode); Serial.print(F(" Enabled="));
    Serial.println(cfg.timer[i].enabled);
  }
  Serial.println(F("------ COUNTERS ------"));
  Serial.print(F("Count: ")); Serial.println(cfg.counterCount);
  for (uint8_t i = 0; i < cfg.counterCount; i++) {
    Serial.print(F("  [") ); Serial.print(i); Serial.print(F("] ID="));
    Serial.print(cfg.counter[i].id); Serial.print(F(" Enabled="));
    Serial.print(cfg.counter[i].enabled); Serial.print(F(" EdgeMode="));
    Serial.print(cfg.counter[i].edgeMode); Serial.print(F(" Dir="));
    Serial.print(cfg.counter[i].direction); Serial.print(F(" Input="));
    Serial.print(cfg.counter[i].inputIndex); Serial.print(F(" Reg="));
    Serial.println(cfg.counter[i].regIndex);
  }
  Serial.println(F("------ GPIO MAPPINGS ------"));
  bool hasGpio = false;
  for (uint8_t i = 0; i < NUM_GPIO; ++i) {
    if (cfg.gpioToCoil[i] >= 0 || cfg.gpioToInput[i] >= 0) {
      Serial.print(F("  Pin ")); Serial.print(i);
      if (cfg.gpioToCoil[i] >= 0) {
        Serial.print(F(" -> coil ")); Serial.print(cfg.gpioToCoil[i]);
      }
      if (cfg.gpioToInput[i] >= 0) {
        Serial.print(F(" -> input ")); Serial.print(cfg.gpioToInput[i]);
      }
      Serial.println();
      hasGpio = true;
    }
  }
  if (!hasGpio) Serial.println(F("  (no GPIO mappings)"));
  Serial.println(F("==========================================\n"));

  EEPROM.put(0, cfg);

  EEPROM.get(0, verifyTempConfig);
  return (verifyTempConfig.magic == cfg.magic &&
          verifyTempConfig.schema == cfg.schema &&
          verifyTempConfig.crc == cfg.crc);
}

// ============================================================================
//  APPLY
// ============================================================================
void configApply(const PersistConfig &cfg) {
  currentSlaveID  = cfg.slaveId;
  currentBaudrate = cfg.baud;
  serverRunning   = (cfg.serverFlag != 0);

  MODBUS_SERIAL.end();
  delay(50);
  MODBUS_SERIAL.begin(currentBaudrate);
  frameGapUs = rtuGapUs();

  memset(holdingRegs, 0, sizeof(holdingRegs));
  memset(coils,       0, sizeof(coils));

  // --- Initialize GPIO-mappings to -1 (unmapped) FIRST ---
  // This ensures safe defaults if config data is corrupted
  for (uint8_t i = 0; i < NUM_GPIO; i++) {
    gpioToCoil[i]  = -1;
    gpioToInput[i] = -1;
  }

  // --- Restore GPIO-mappings from config ---
  for (uint8_t i = 0; i < NUM_GPIO; i++) {
    // Only restore if value is explicitly valid (>= 0 or == -1)
    // If both coil and input are 0 (from uninitialized EEPROM), treat as -1
    int16_t c = cfg.gpioToCoil[i];
    int16_t d = cfg.gpioToInput[i];

    // If both are 0 (uninitialized), treat as -1
    if (c == 0 && d == 0) {
      gpioToCoil[i]  = -1;
      gpioToInput[i] = -1;
    } else {
      // Restore with validation
      if (c >= 0 && c < (int16_t)NUM_COILS) {
        gpioToCoil[i] = c;
      } else if (c == -1) {
        gpioToCoil[i] = -1;
      }

      if (d >= 0 && d < (int16_t)NUM_DISCRETE) {
        gpioToInput[i] = d;
      } else if (d == -1) {
        gpioToInput[i] = -1;
      }
    }
  }

  // --- Gendan globale timer status/control registre ---
  timerStatusRegIndex     = cfg.timerStatusReg;
  timerStatusCtrlRegIndex = cfg.timerStatusCtrlReg;

  strncpy(cliHostname, cfg.hostname, sizeof(cliHostname));
  cliHostname[sizeof(cliHostname)-1] = '\0'; // sikker terminering

  regStaticCount  = cfg.regStaticCount;
  coilStaticCount = cfg.coilStaticCount;

  // --- Gendan statiske registre (adresser + værdier) ---
for (uint8_t i = 0; i < regStaticCount && i < MAX_STATIC_REGS; i++) {
  regStaticAddr[i] = cfg.regStaticAddr[i];
  regStaticVal[i]  = cfg.regStaticVal[i];
  if (regStaticAddr[i] < NUM_REGS)
    holdingRegs[regStaticAddr[i]] = regStaticVal[i];
}

// --- Gendan statiske coils (indeks + værdier) ---
for (uint8_t i = 0; i < coilStaticCount && i < MAX_STATIC_COILS; i++) {
  coilStaticIdx[i] = cfg.coilStaticIdx[i];
  coilStaticVal[i] = cfg.coilStaticVal[i];
  if (coilStaticIdx[i] < NUM_COILS)
    bitWriteArray(coils, coilStaticIdx[i], (coilStaticVal[i] != 0));
}


  // --- Timers ---
  timers_init();
  if (timerStatusCtrlRegIndex < NUM_REGS)
    holdingRegs[timerStatusCtrlRegIndex] = 0;

  // Load ALL 4 timers from config (enabled flag in struct determines if active)
  for (uint8_t i = 0; i < 4; i++) {
    // Use timers_config_set() to apply config (handles GPIO conflicts)
    timers_config_set(cfg.timer[i].id, cfg.timer[i]);
  }

  // --- Counters ---
  counters_init();

  // Gendan counter reset-on-read enable flags og auto-start enable flags
  for (uint8_t i = 0; i < 4; ++i) {
    counterResetOnReadEnable[i] = cfg.counterResetOnReadEnable[i];
    counterAutoStartEnable[i] = cfg.counterAutoStartEnable[i];
  }

  // Load ALL 4 counters from config (enabled flag in struct determines if active)
  for (uint8_t i = 0; i < 4; i++) {
    counters_config_set(cfg.counter[i].id, cfg.counter[i]);
  }

  // Sync bit 3 (reset-on-read) i controlReg fra counterResetOnReadEnable array
  for (uint8_t i = 0; i < 4; ++i) {
    if (counters[i].enabled && counters[i].controlReg < NUM_REGS) {
      if (counterResetOnReadEnable[i]) {
        holdingRegs[counters[i].controlReg] |= 0x0008;  // set bit 3
      } else {
        holdingRegs[counters[i].controlReg] &= ~0x0008; // clear bit 3
      }
    }
  }

  // --- GPIO mapping already restored at the beginning of configApply() ---
  // No need to restore again here
}
