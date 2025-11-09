#include "modbus_core.h"

void sendResponse(uint8_t *r,uint8_t len,uint8_t sid){
  if(monitorMode){
    Serial.println("--- MONITOR: TX suppressed ---");
    Serial.print("HEX: "); printHex(r,len); return;
  }
  uint16_t crc=calculateCRC16(r,len);
  r[len++]=crc&0xFF; r[len++]=(crc>>8)&0xFF;
  Serial.println("--- TX ---"); Serial.print("HEX: "); printHex(r,len);
  rs485_tx_enable(); MODBUS_SERIAL.write(r,len); MODBUS_SERIAL.flush(); rs485_rx_enable();
  responsesSent++;
}
void sendException(uint8_t s,uint8_t fc,uint8_t ex){
  uint8_t r[3]={s,(uint8_t)(fc|0x80),ex};
  Serial.print("EXC "); Serial.println(ex);
  sendResponse(r,3,s);
}
