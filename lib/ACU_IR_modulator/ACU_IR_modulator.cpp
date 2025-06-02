#include "ACU_IR_modulator.h"

void parseBinaryToDurations(uint64_t binaryInput, uint16_t *durations, size_t &len)
{
    len = 0;

    durations[len++] = kHdrMark;
    durations[len++] = kHdrSpace;

    for (int i = 63; i >= 0; --i)
    {
        if (len + 2 > rawDataLength)
        {
            Serial.println("Error: Durations array overflow!");
            return;
        }

        bool bit = (binaryInput >> i) & 1;
        durations[len++] = kBitMark;
        durations[len++] = bit ? kOneSpace : kZeroSpace;
    }

    // Optional trailing bits (depends on protocol)
    if (len + 2 <= rawDataLength)
    {
        durations[len++] = kBitMark;
        durations[len++] = kHdrSpace;
        durations[len++] = kBitMark;
    } else {
    Serial.println("Error: Unable to add ending sequence due to array size.");
  }
}

// Legacy method for testing/debug input
void parseBinaryToDurations(const String &binaryInput, uint16_t *durations, size_t &len) {
  len = 0;

  durations[len++] = kHdrMark;
  durations[len++] = kHdrSpace;

  for (size_t i = 0; i < binaryInput.length(); i++) {
    if (len + 2 > rawDataLength) {
      Serial.println("Error: Durations array overflow!");
      return;
    }

    char bit = binaryInput[i];
    if (bit == '1') {
      durations[len++] = kBitMark;
      durations[len++] = kOneSpace;
    } else if (bit == '0') {
      durations[len++] = kBitMark;
      durations[len++] = kZeroSpace;
    }
  }

  if (len + 2 <= rawDataLength) {
    durations[len++] = kBitMark;
    durations[len++] = kHdrSpace;
    durations[len++] = kBitMark;
  } else {
    Serial.println("Error: Unable to add ending sequence due to array size.");
  }
}

void debugIRInput() {
  if (Serial.available()) {
    String binaryInput = Serial.readStringUntil('\n');
    binaryInput.trim();
    binaryInput.replace(" ", "");

    if (binaryInput.length() != 64) {
      Serial.println("Invalid input! Please enter exactly 64 bits.");
      return;
    }

    uint16_t durations[rawDataLength];
    size_t len = 0;
    parseBinaryToDurations(binaryInput, durations, len);
    irsend.sendRaw(durations, len, 38);
    Serial.println("IR sent!");
  }
}