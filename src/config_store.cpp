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
  if (cfg.schema != 9 && cfg.schema != 10) {
    Serial.print(F("! EEPROM schema unknown (got "));
    Serial.print(cfg.schema);
    Serial.println(F(")"));
    configDefaults(cfg);
    return false;  // Let main.cpp handle save
  }

  // For schema 9->10 migration: just report and use defaults
  if (cfg.schema == 9) {
    Serial.println(F("! EEPROM schema 9 (old) - upgrading to 10"));
    // Save the old settings we want to keep
    uint8_t oldSlaveId = cfg.slaveId;
    uint32_t oldBaud = cfg.baud;
    uint8_t oldServerFlag = cfg.serverFlag;

    // Reset to defaults (this will set schema=10 and initialize hwMode=0)
    configDefaults(cfg);

    // Restore old settings
    cfg.slaveId = oldSlaveId;
    cfg.baud = oldBaud;
    cfg.serverFlag = oldServerFlag;

    return false;  // Let main.cpp handle save with migration settings
  }

  // Schema 10 validation
  if (cfg.schema == 10) {
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
  cfg.schema     = 10;  // Updated to v10 for hw-mode support
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

  for (uint8_t p=0; p<NUM_GPIO; ++p) {
    cfg.gpioToCoil[p]  = -1;
    cfg.gpioToInput[p] = -1;
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

  cfg.schema = 10;  // Fixed: Must match schema check in configLoad() - v10 for hw-mode
  computeFillCrc(cfg);

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

  for (uint8_t i = 0; i < cfg.timerCount && i < 4; i++) {
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

  CounterConfig tmp[4];
  for (uint8_t i = 0; i < cfg.counterCount && i < 4; i++) {
    tmp[i] = cfg.counter[i];
  }

  for (uint8_t i = 0; i < cfg.counterCount && i < 4; i++)
    counters_config_set(tmp[i].id, tmp[i]);

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

  // --- GPIO mapping ---
  for (uint8_t pin=0; pin<NUM_GPIO; ++pin) {
    gpioToCoil[pin]  = cfg.gpioToCoil[pin];
    gpioToInput[pin] = cfg.gpioToInput[pin];
    if (gpioToCoil[pin] != -1 || gpioToInput[pin] != -1)
      pinMode(pin, INPUT);
  }
}
