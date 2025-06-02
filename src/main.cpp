#include <Arduino.h>

#include "OTA_config.h"
#include "ACU_remote_encoder.h"
#include "ACU_IR_modulator.h"

// #include <PubSubClient.h> // MQTT implementation @ ACU_remote::toJSON()

// Debug:
// #define DEBUG_IR_PRINT

// Definitions:
#define ACUsignature "MITSUBISHI_HEAVY_64"
#define kIrLedPin     4
#define rawDataLength 133

IRsend irsend(kIrLedPin);
uint16_t durations[rawDataLength];

ACU_remote APC_ACU(ACUsignature);
const IRProtocolConfig* selectedProtocol = &MITSUBISHI_HEAVY_64;

void setup() {
  Serial.begin(115200);
  irsend.begin();

  // delay(4000);
  Serial.println("\nMCU Status: ON");

  setupOTA();


  APC_ACU.setState(2, 24, ACUMode::COOL, 3, true);  // Fan Speed (1-4), Temperature (18-30), Mode, Louver (1-4), Power (ON = true)
  uint64_t cmd1 = APC_ACU.encodeCommand();
  Serial.println(cmd1);
  String command1 = ACU_remote::toBinaryString(cmd1);
  Serial.println(command1);

  command1.trim();
  command1.replace(" ", "");

  String command1trim = command1;
  Serial.println(command1trim);
  Serial.println(APC_ACU.toJSON());

  size_t len = 0;
  parseBinaryToDurations(command1trim, durations, len);
  irsend.sendRaw(durations, len, 38);  // 38kHz modulation

  delay(2000);

  APC_ACU.setState(1, 18, ACUMode::DRY, 2, false);
  uint64_t cmd2 = APC_ACU.encodeCommand();
  Serial.println(ACU_remote::toBinaryString(cmd2));
  Serial.println(APC_ACU.toJSON());

  len = 0;
  parseBinaryToDurations(cmd2, durations, len);
  irsend.sendRaw(durations, len, 38);  // 38kHz modulation

  delay(2000);


  APC_ACU.setPowerState(true);
  uint64_t cmd3 = APC_ACU.encodeCommand();
  Serial.println(ACU_remote::toBinaryString(cmd3));
  Serial.println(APC_ACU.toJSON());
  
  len = 0;
  parseBinaryToDurations(cmd3, durations, len);
  irsend.sendRaw(durations, len, 38);  // 38kHz modulation

  delay(2000);

  String input = "{\"fanSpeed\":2,\"temperature\":24,\"mode\":\"cool\",\"louver\":3,\"isOn\":true}";
  if (APC_ACU.fromJSON(input)) {
    uint64_t cmdJSON = APC_ACU.encodeCommand();
    Serial.println(ACU_remote::toBinaryString(cmdJSON));
    len = 0;
    parseBinaryToDurations(cmdJSON, durations, len);
    irsend.sendRaw(durations, len, 38);  // 38kHz modulation
    Serial.println(input);
  } else {
    Serial.println("Failed to parse JSON and set state.");
}

}

void loop() {
  ArduinoOTA.handle();  

  #ifdef DEBUG_IR_PRINT
  debugIRInput();
  #endif
}
