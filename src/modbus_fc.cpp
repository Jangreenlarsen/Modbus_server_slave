// ============================================================================
//  Filnavn : modbus_fc.cpp
//  Projekt  : Modbus RTU Server / CLI
//  Version  : v3.1.3-patch1 (2025-11-04)
//  Forfatter: JanG at modbus_slave@laces.dk
//  Formål   : Modbus function-code-håndtering med CLEAN init
//  Ændringer:
//    - v3.1.3-patch1: Fjernet demo-init i initModbus(); kalder nu modbus_init_globals()
//                      for CLEAN build uden testdata.
// ============================================================================
#include "modbus_core.h"
#include "modbus_timers.h"
#include "modbus_counters.h"

// ---------------------------------------------------------------------------
// READ HANDLERS
// ---------------------------------------------------------------------------
static void fc_read_coils(uint8_t rxSlave, uint8_t *f) {
  uint16_t s=(f[2]<<8)|f[3], q=(f[4]<<8)|f[5];
  if(q<1||q>2000){ sendException(rxSlave, FC_READ_COILS, EX_ILLEGAL_DATA_VALUE); return; }
  if((uint32_t)s+q>NUM_COILS){ sendException(rxSlave, FC_READ_COILS, EX_ILLEGAL_DATA_ADDRESS); return; }
  uint8_t bc=(q+7)/8, resp[MAX_RESP];
  resp[0]=rxSlave; resp[1]=FC_READ_COILS; resp[2]=bc;
  packBits(coils,s,q,&resp[3]);
  sendResponse(resp,3+bc,rxSlave);
}

static void fc_read_discrete(uint8_t rxSlave,uint8_t* f){
  uint16_t s=(f[2]<<8)|f[3], q=(f[4]<<8)|f[5];
  if(q<1||q>2000){ sendException(rxSlave, FC_READ_DISCRETE_INPUTS, EX_ILLEGAL_DATA_VALUE); return; }
  if((uint32_t)s+q>NUM_DISCRETE){ sendException(rxSlave, FC_READ_DISCRETE_INPUTS, EX_ILLEGAL_DATA_ADDRESS); return; }
  uint8_t bc=(q+7)/8, resp[MAX_RESP];
  resp[0]=rxSlave; resp[1]=FC_READ_DISCRETE_INPUTS; resp[2]=bc;
  packBits(discreteInputs,s,q,&resp[3]);
  sendResponse(resp,3+bc,rxSlave);
}

static void fc_read_hregs(uint8_t rxSlave,uint8_t* f){
  uint16_t s=(f[2]<<8)|f[3], q=(f[4]<<8)|f[5];
  if(q<1||q>125){ sendException(rxSlave,FC_READ_HOLDING_REGS,EX_ILLEGAL_DATA_VALUE);return; }
  if((uint32_t)s+q>NUM_REGS){ sendException(rxSlave,FC_READ_HOLDING_REGS,EX_ILLEGAL_DATA_ADDRESS);return; }
  uint8_t resp[MAX_RESP]; resp[0]=rxSlave; resp[1]=FC_READ_HOLDING_REGS; resp[2]=q*2;
  uint16_t idx=3; 
  for(uint16_t i=0;i<q;i++){
    uint16_t v=holdingRegs[s+i];
    resp[idx++]=v>>8;
    resp[idx++]=v;
  }
  
  // --- Reset-on-Read håndtering for CounterEngine ---
  for (uint8_t ci = 0; ci < 4; ++ci) {
    CounterConfig& c = counters[ci];
    if (!c.enabled) continue;
    if (!counterResetOnReadEnable[ci]) continue;  // reset-on-read ikke enabled for denne counter

    // Bestem tællerens registerområde (skaleret værdi)
    uint8_t words = (c.bitWidth == 64) ? 4 : (c.bitWidth == 32 ? 2 : 1);
    uint16_t regStart = c.regIndex;
    uint16_t regEnd   = regStart + words - 1;

    // Er læseområdet (s..s+q-1) overlappende med tællerens område?
    if (!((s + q - 1) < regStart || s > regEnd)) {
      uint8_t bw = sanitizeBitWidth(c.bitWidth);
      uint64_t sv = maskToBitWidth(c.startValue, bw);
      c.counterValue = sv;
      c.edgeCount = 0;
      c.overflowFlag = 0;
      if (c.overflowReg < NUM_REGS) holdingRegs[c.overflowReg] = 0;
    }
  }

  // --- TimerEngine: reset-on-read af statusreg ---
  if (timerStatusRegIndex < NUM_REGS && timerStatusCtrlRegIndex < NUM_REGS) {
    uint16_t end = s + q - 1;
    if (!(end < timerStatusRegIndex || s > timerStatusRegIndex)) {
      uint16_t ctrlMask = holdingRegs[timerStatusCtrlRegIndex] & 0x000F;
      if (ctrlMask) {
        holdingRegs[timerStatusRegIndex] &= ~ctrlMask;
      }
    }
  }

  sendResponse(resp,idx,rxSlave);
}

static void fc_read_iregs(uint8_t rxSlave,uint8_t* f){
  uint16_t s=(f[2]<<8)|f[3], q=(f[4]<<8)|f[5];
  if(q<1||q>125){ sendException(rxSlave,FC_READ_INPUT_REGS,EX_ILLEGAL_DATA_VALUE);return; }
  if((uint32_t)s+q>NUM_INPUTS){ sendException(rxSlave,FC_READ_INPUT_REGS,EX_ILLEGAL_DATA_ADDRESS);return; }
  uint8_t resp[MAX_RESP]; resp[0]=rxSlave; resp[1]=FC_READ_INPUT_REGS; resp[2]=q*2;
  uint16_t idx=3; 
  for(uint16_t i=0;i<q;i++){
    uint16_t v=inputRegs[s+i];
    resp[idx++]=v>>8;
    resp[idx++]=v;
  }
  sendResponse(resp,idx,rxSlave);
}

// ---------------------------------------------------------------------------
// WRITE HANDLERS
// ---------------------------------------------------------------------------
static void fc_write_single_coil(uint8_t rxSlave,uint8_t* f){
  uint16_t a=(f[2]<<8)|f[3],v=(f[4]<<8)|f[5];
  if(a>=NUM_COILS){sendException(rxSlave,FC_WRITE_SINGLE_COIL,EX_ILLEGAL_DATA_ADDRESS);return;}
  if(v!=0xFF00&&v!=0x0000){sendException(rxSlave,FC_WRITE_SINGLE_COIL,EX_ILLEGAL_DATA_VALUE);return;}

  bool val=(v==0xFF00);
  if(!timers_hasCoil(a)) bitWriteArray(coils,a,val);
  timers_onCoilWrite(a,(uint8_t)(val?1:0));

  uint8_t resp[6]={rxSlave,FC_WRITE_SINGLE_COIL,f[2],f[3],f[4],f[5]};
  sendResponse(resp,6,rxSlave);
}

static void fc_write_single_reg(uint8_t rxSlave,uint8_t* f){
  uint16_t a=(f[2]<<8)|f[3],v=(f[4]<<8)|f[5];
  if(a>=NUM_REGS){sendException(rxSlave,FC_WRITE_SINGLE_REG,EX_ILLEGAL_DATA_ADDRESS);return;}
  holdingRegs[a]=v;

  // --- Specialkommando: write reg 0 = 0x00FF -> save EEPROM config ---
  if (a == 0 && v == 0x00FF) {
    PersistConfig cfg;
    memset(&cfg, 0, sizeof(cfg));

    // Byg aktuel config (samme som CLI SAVE)
    cfg.magic      = 0xC0DE;
    cfg.schema     = 7;  // Opdateret til schema 7
    cfg.slaveId    = currentSlaveID;
    cfg.serverFlag = serverRunning ? 1 : 0;
    cfg.baud       = currentBaudrate;

    // Statiske registre
    cfg.regStaticCount = regStaticCount;
    for (uint8_t i = 0; i < regStaticCount && i < MAX_STATIC_REGS; i++) {
      cfg.regStaticAddr[i] = regStaticAddr[i];
      cfg.regStaticVal[i]  = regStaticVal[i];
    }

    // Coils
    cfg.coilStaticCount = coilStaticCount;
    for (uint8_t i = 0; i < coilStaticCount && i < MAX_STATIC_COILS; i++) {
      cfg.coilStaticIdx[i] = coilStaticIdx[i];
      cfg.coilStaticVal[i] = coilStaticVal[i];
    }

    // Timere
    cfg.timerCount = 4;
    for (uint8_t i = 0; i < 4; i++) {
      cfg.timer[i] = timers[i];
    }

    // Counters
    cfg.counterCount = 4;
    for (uint8_t i = 0; i < 4; i++) {
      cfg.counter[i] = counters[i];
    }

    // GPIO-mapping
    for (uint8_t i = 0; i < NUM_GPIO; i++) {
      cfg.gpioToCoil[i]  = gpioToCoil[i];
      cfg.gpioToInput[i] = gpioToInput[i];
    }

    if (configSave(cfg)) {
      Serial.println(F("AUTO-SAVE: Konfiguration gemt til EEPROM (via reg0=0xFF)"));
    } else {
      Serial.println(F("AUTO-SAVE FEJL: configSave() returnerede false"));
    }
  }

  // --- TimerEngine: opdater sticky reset-on-read flag (control-reg) ---
  if (a == timerStatusCtrlRegIndex && timerStatusCtrlRegIndex < NUM_REGS) {
    uint16_t mask = v & 0x000F; // bit0..3 = timer1..4
    for (uint8_t ti = 0; ti < 4; ++ti) {
      timers[ti].statusRoEnable = (mask & (1u << ti)) ? 1 : 0;
    }
  }

  // --- CounterEngine: sync bit3 (reset-on-read) to sticky EEPROM flag ---
  for (uint8_t i = 0; i < 4; ++i) {
    if (counters[i].enabled && a == counters[i].controlReg && counters[i].controlReg < NUM_REGS) {
      // bit 3 i control register styrer reset-on-read
      counterResetOnReadEnable[i] = (v & 0x0008) ? 1 : 0;
    }
  }

  uint8_t resp[6]={rxSlave,FC_WRITE_SINGLE_REG,f[2],f[3],f[4],f[5]};
  sendResponse(resp,6,rxSlave);
}

static void fc_write_multiple_coils(uint8_t rxSlave,uint8_t* f){
  uint16_t s=(f[2]<<8)|f[3],q=(f[4]<<8)|f[5];uint8_t bc=f[6];
  if(q<1||q>1968||bc!=(uint8_t)((q+7)/8)){sendException(rxSlave,FC_WRITE_MULTIPLE_COILS,EX_ILLEGAL_DATA_VALUE);return;}
  if((uint32_t)s+q>NUM_COILS){sendException(rxSlave,FC_WRITE_MULTIPLE_COILS,EX_ILLEGAL_DATA_ADDRESS);return;}
  const uint8_t* d=&f[7];
  for(uint16_t i=0;i<q;i++){
    bool bit=(d[i>>3]>>(i&7))&1;
    if(!timers_hasCoil(s+i)) bitWriteArray(coils,s+i,bit);
    timers_onCoilWrite(s+i,(uint8_t)(bit?1:0));
  }


  uint8_t resp[6]={rxSlave,FC_WRITE_MULTIPLE_COILS,f[2],f[3],f[4],f[5]};
  sendResponse(resp,6,rxSlave);
}

static void fc_write_multiple_regs(uint8_t rxSlave,uint8_t* f){
  uint16_t s=(f[2]<<8)|f[3],q=(f[4]<<8)|f[5];uint8_t bc=f[6];
  if(q<1||q>123||bc!=q*2){sendException(rxSlave,FC_WRITE_MULTIPLE_REGS,EX_ILLEGAL_DATA_VALUE);return;}
  if((uint32_t)s+q>NUM_REGS){sendException(rxSlave,FC_WRITE_MULTIPLE_REGS,EX_ILLEGAL_DATA_ADDRESS);return;}
  uint8_t idx=7;
  for(uint16_t i=0;i<q;i++){
    uint16_t v=(f[idx]<<8)|f[idx+1];
    holdingRegs[s+i]=v;
    idx+=2;
  }
  uint8_t resp[6]={rxSlave,FC_WRITE_MULTIPLE_REGS,f[2],f[3],f[4],f[5]};

  // --- Specialkommando: hvis reg 0 = 0x00FF blandt de skrevne -> save config ---
  for (uint16_t i = 0; i < q; ++i) {
    uint16_t addr = s + i;
    uint16_t val  = holdingRegs[addr];
    if (addr == 0 && val == 0x00FF) {
      PersistConfig cfg;
      memset(&cfg, 0, sizeof(cfg));

      // Byg aktuel konfiguration (samme som CLI SAVE)
      cfg.magic      = 0xC0DE;
      cfg.schema     = 7;  // Opdateret til schema 7
      cfg.slaveId    = currentSlaveID;
      cfg.serverFlag = serverRunning ? 1 : 0;
      cfg.baud       = currentBaudrate;

      // Statiske holding-registre
      cfg.regStaticCount = regStaticCount;
      for (uint8_t j = 0; j < regStaticCount && j < MAX_STATIC_REGS; j++) {
        cfg.regStaticAddr[j] = regStaticAddr[j];
        cfg.regStaticVal[j]  = regStaticVal[j];
      }

      // Coils
      cfg.coilStaticCount = coilStaticCount;
      for (uint8_t j = 0; j < coilStaticCount && j < MAX_STATIC_COILS; j++) {
        cfg.coilStaticIdx[j] = coilStaticIdx[j];
        cfg.coilStaticVal[j] = coilStaticVal[j];
      }

      // Timere
      cfg.timerCount = 4;
      for (uint8_t j = 0; j < 4; j++) {
        cfg.timer[j] = timers[j];
      }

      // Counters
      cfg.counterCount = 4;
      for (uint8_t j = 0; j < 4; j++) {
        cfg.counter[j] = counters[j];
      }

      // GPIO mapping
      for (uint8_t j = 0; j < NUM_GPIO; j++) {
        cfg.gpioToCoil[j]  = gpioToCoil[j];
        cfg.gpioToInput[j] = gpioToInput[j];
      }

      if (configSave(cfg))
        Serial.println(F("AUTO-SAVE: Konfiguration gemt til EEPROM (via FC16 reg0=0xFF)"));
      else
        Serial.println(F("AUTO-SAVE FEJL: configSave() returnerede false"));
      break;   // udfør kun én gang
    }
  }

  // --- TimerEngine: opdater sticky reset-on-read flag (control-reg) ---
  if (timerStatusCtrlRegIndex < NUM_REGS) {
    uint16_t end = s + q - 1;
    if (!(end < timerStatusCtrlRegIndex || s > timerStatusCtrlRegIndex)) {
      uint16_t vctrl = holdingRegs[timerStatusCtrlRegIndex];
      uint16_t mask  = vctrl & 0x000F;  // bit0..3 = timer1..4
      for (uint8_t ti = 0; ti < 4; ++ti) {
        timers[ti].statusRoEnable = (mask & (1u << ti)) ? 1 : 0;
      }
    }
  }

  sendResponse(resp,6,rxSlave);
}

// ---------------------------------------------------------------------------
// PROCESSING & INIT
// ---------------------------------------------------------------------------
void processModbusFrame(uint8_t *frame, uint8_t len) {
  totalFrames++;
  Serial.println();
  Serial.print("=== RX Frame #"); Serial.println(totalFrames);
  Serial.print("HEX: "); printHex(frame, len);
  if (len < 4) { Serial.println("ERROR: Too short"); return; }

  uint16_t recCRC = frame[len-2] | (frame[len-1] << 8);
  uint16_t calcCRC = calculateCRC16(frame, len-2);
  if (recCRC != calcCRC) { Serial.println("ERROR: CRC mismatch"); crcErrors++; return; }

  uint8_t rxSlave = frame[0];
  uint8_t fc      = frame[1];
  if (!listenToAll && rxSlave != currentSlaveID) {
    Serial.println("IGNORED: Wrong slave ID"); wrongSlaveID++; return;
  }

  validFrames++;
  Serial.print("FC: 0x"); if(fc<0x10) Serial.print("0"); Serial.print(fc, HEX); Serial.println();

  switch (fc) {
    case FC_READ_COILS:            fc_read_coils(rxSlave, frame); break;
    case FC_READ_DISCRETE_INPUTS:  fc_read_discrete(rxSlave, frame); break;
    case FC_READ_HOLDING_REGS:     fc_read_hregs(rxSlave, frame); break;
    case FC_READ_INPUT_REGS:       fc_read_iregs(rxSlave, frame); break;
    case FC_WRITE_SINGLE_COIL:     fc_write_single_coil(rxSlave, frame); break;
    case FC_WRITE_SINGLE_REG:      fc_write_single_reg(rxSlave, frame); break;
    case FC_WRITE_MULTIPLE_COILS:  fc_write_multiple_coils(rxSlave, frame); break;
    case FC_WRITE_MULTIPLE_REGS:   fc_write_multiple_regs(rxSlave, frame); break;
    default: sendException(rxSlave, fc, EX_ILLEGAL_FUNCTION); break;
  }
}

// CLEAN init version
void initModbus() {
  // Nulstil alle globale buffere og runtime-state
  modbus_init_globals();

  pinMode(RS485_DIR_PIN, OUTPUT);
  rs485_rx_enable();
  MODBUS_SERIAL.begin(currentBaudrate);
  frameGapUs = rtuGapUs();
  Serial.print(F("RTU gap(us): ")); Serial.println(frameGapUs);

  // Init delsystemer
  timers_init();
  counters_init();

  // Nulstil GPIO-mapping
  for (int p = 0; p < NUM_GPIO; ++p) {
    gpioToCoil[p]  = -1;
    gpioToInput[p] = -1;
  }

  // Config loading and apply is done in main.cpp setup() before calling initModbus()
  // This avoids double-loading and prevents stack overflow from local PersistConfig
}

void modbusLoop() {
  static uint8_t  rxBuf[RXBUF_SIZE];
  static uint8_t  rxLen = 0;
  static bool     frameComplete = false;
  static unsigned long lastUs = 0;
  unsigned long nowUs = micros();

  if (!serverRunning) {
    while (MODBUS_SERIAL.available()) MODBUS_SERIAL.read();
    for (uint8_t p = 0; p < NUM_GPIO; ++p) {
      if (gpioToCoil[p] >= 0) {
        bool coilState = bitReadArray(coils, gpioToCoil[p]);
        digitalWrite(p, coilState ? HIGH : LOW);
      }
      if (gpioToInput[p] >= 0) {
        bool pinState = digitalRead(p);
        bitWriteArray(discreteInputs, gpioToInput[p], pinState);
      }
    }
    timers_loop();
    counters_loop();
    return;
  }

  while (MODBUS_SERIAL.available()) {
    uint8_t b = MODBUS_SERIAL.read();
    if (rxLen>0 && (nowUs - lastUs) > frameGapUs) rxLen = 0;
    if (rxLen < RXBUF_SIZE) rxBuf[rxLen++] = b; else rxLen = 0;
    lastUs = nowUs;
    frameComplete = false;
  }

  if (rxLen>0 && !frameComplete && (nowUs - lastUs) > frameGapUs) {
    frameComplete = true;
    processModbusFrame(rxBuf, rxLen);
    rxLen = 0;
  }

  for (uint8_t p = 0; p < NUM_GPIO; ++p) {
    if (gpioToCoil[p] >= 0) {
      bool coilState = bitReadArray(coils, gpioToCoil[p]);
      digitalWrite(p, coilState ? HIGH : LOW);
    }
    if (gpioToInput[p] >= 0) {
      bool pinState = digitalRead(p);
      bitWriteArray(discreteInputs, gpioToInput[p], pinState);
    }
  }

  timers_loop();
  counters_loop();
}
