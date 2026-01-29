#include "ACU_IR_modulator.h"

// Convert a 64-bit binary command into IR durations for sending via IR LED
bool parseBinaryToDurations(uint64_t binaryInput, uint16_t *durations, size_t &len)
{
    len = 0;  // Reset length for durations array

    // Add header mark and space signals at the start of the IR transmission
    durations[len++] = kHdrMark;
    durations[len++] = kHdrSpace;

    // Loop through each bit from MSB to LSB
    for (int i = 63; i >= 0; --i)
    {
        // Prevent buffer overflow before adding new durations
        if (len + 2 > rawDataLength)
        {
            // Error: Durations array overflow!
            return false;
        }

        // Extract the current bit value (0 or 1)
        bool bit = (binaryInput >> i) & 1;
        durations[len++] = kBitMark;          // Add mark duration for bit start
        durations[len++] = bit ? kOneSpace : kZeroSpace;  // Add space duration depending on bit value
    }

    // Add trailing sequence if buffer space allows (protocol-dependent)
    if (len + 2 <= rawDataLength)
    {
        durations[len++] = kBitMark;
        durations[len++] = kHdrSpace;
        durations[len++] = kBitMark;
    } 
    else 
    {
        // Error: Unable to add ending sequence due to array size.
        return false; // Not a critical failure, but indicates truncation.
    }
    return true;
}

// Legacy method to convert a binary string (e.g. "110010...") into IR durations
bool parseBinaryToDurations(const String &binaryInput, uint16_t *durations, size_t &len) {
    len = 0;  // Reset durations length

    // Add header mark and space durations
    durations[len++] = kHdrMark;
    durations[len++] = kHdrSpace;

    // Iterate over each character (bit) in the string
    for (size_t i = 0; i < binaryInput.length(); i++) {
        // Check buffer size before adding durations
        if (len + 2 > rawDataLength) {
            // Error: Durations array overflow!
            return false;
        }

        char bit = binaryInput[i];
        durations[len++] = kBitMark;  // Add mark duration

        // Add space duration depending on bit value
        if (bit == '1') {
            durations[len++] = kOneSpace;
        } else if (bit == '0') {
            durations[len++] = kZeroSpace;
        }
    }

    // Add trailing sequence if there is space
    if (len + 2 <= rawDataLength) {
        durations[len++] = kBitMark;
        durations[len++] = kHdrSpace;
        durations[len++] = kBitMark;
    } else {
        // Error: Unable to add ending sequence due to array size.
        return false; // Not a critical failure, but indicates truncation.
    }
    return true;
}

// Debug helper function: reads 64-bit binary input from Serial, converts, and sends IR
void debugIRInput() {
    if (Serial.available()) {
        // Read input line from Serial
        String binaryInput = Serial.readStringUntil('\n');
        binaryInput.trim();                // Remove whitespace
        binaryInput.replace(" ", "");     // Remove spaces

        // Validate input length (must be exactly 64 bits)
        if (binaryInput.length() != 64) {
            Serial.println("Invalid input! Please enter exactly 64 bits.");
            return;
        }

        uint16_t durations[rawDataLength];
        size_t len = 0;

        // Convert binary string to durations array
        if (parseBinaryToDurations(binaryInput, durations, len)) {
            // Send IR signal at 38 kHz carrier frequency
            irsend.sendRaw(durations, len, 38);

            Serial.println("IR sent!");
        } else {
            Serial.println("Error: Failed to parse binary string into IR durations.");
        }
    }
}
