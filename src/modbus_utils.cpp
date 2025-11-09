// ============================================================================
//  Filnavn : modbus_utils.cpp
//  Projekt  : Modbus RTU Server / CLI
//  Version  : v3.1.3-patch1 (2025-11-03)
//  Forfatter: ChatGPT Automation
//  Formål   : Diverse hjælpefunktioner – CRC, bitfelter, RTU-gap m.m.
// ============================================================================

#include "modbus_globals.h"

// ---------------------------------------------------------------------------
//  CRC16 (Modbus-standard, polynomium 0xA001)
// ---------------------------------------------------------------------------
uint16_t calculateCRC16(uint8_t *buf, uint8_t len) {
  uint16_t crc = 0xFFFF;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= buf[i];
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 1)
        crc = (crc >> 1) ^ 0xA001;
      else
        crc >>= 1;
    }
  }
  return crc;
}

// ---------------------------------------------------------------------------
//  Bitfelt-funktioner
// ---------------------------------------------------------------------------
bool bitReadArray(const uint8_t *arr, uint16_t bitIndex) {
  uint16_t byteIndex = bitIndex / 8;
  uint8_t  bitPos    = bitIndex % 8;
  return (arr[byteIndex] >> bitPos) & 0x01;
}

void bitWriteArray(uint8_t *arr, uint16_t bitIndex, bool value) {
  uint16_t byteIndex = bitIndex / 8;
  uint8_t  bitPos    = bitIndex % 8;
  if (value) arr[byteIndex] |= (1 << bitPos);
  else       arr[byteIndex] &= ~(1 << bitPos);
}

void packBits(const uint8_t *src, uint16_t start, uint16_t qty, uint8_t *dst) {
  memset(dst, 0, (qty + 7) / 8);
  for (uint16_t i = 0; i < qty; ++i)
    if (bitReadArray(src, start + i))
      dst[i / 8] |= (1 << (i % 8));
}

// ---------------------------------------------------------------------------
//  Beregning af RTU-gap (inter-frame timeout)
// ---------------------------------------------------------------------------
unsigned long rtuGapUs(void) {
  // Standard 3,5 tegn ved 9600 baud ≈ 3,65 ms
  // Forenklet proportional beregning
  const float charTime = (1.0f / (float)BAUDRATE) * 11.0f; // 1 start + 8 data + 1 parity + 1 stop
  return (unsigned long)(charTime * 3.5f * 1e6f);
}
// ---------------------------------------------------------------------------
//  RS-485 styring og HEX print (debug utilities)
// ---------------------------------------------------------------------------

// Print en bytebuffer som hex
void printHex(uint8_t *b, uint8_t n) {
  for (uint8_t i = 0; i < n; i++) {
    if (b[i] < 0x10) Serial.print('0');
    Serial.print(b[i], HEX);
    Serial.print(' ');
  }
  Serial.println();
}

// RS-485 DIR pin styring (Arduino Mega -> pin 8 som standard)
#ifndef RS485_DIR_PIN
#define RS485_DIR_PIN 8
#endif

void rs485_tx_enable() {
  digitalWrite(RS485_DIR_PIN, HIGH);
  delayMicroseconds(50); // kort forsinkelse for stabilitet
}

void rs485_rx_enable() {
  delayMicroseconds(50);
  digitalWrite(RS485_DIR_PIN, LOW);
}
