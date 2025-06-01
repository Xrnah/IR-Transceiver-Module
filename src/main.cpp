#include <Arduino.h>

#include "OTA_config.h"
#include "ACU_remote_encoder.h"
// #include <PubSubClient.h> // MQTT implementation @ ACU_remote::toJSON()

ACU_remote APC_ACU("mitsubishi-heavy");

void setup() {
  Serial.begin(115200);

  // delay(4000);
  Serial.println("\nMCU Status: ON");

  setupOTA();

  APC_ACU.setState(2, 24, ACUMode::COOL, 3, true);  // Fan Speed (1-4), Temperature (18-30), Mode, Louver (1-4), Power (ON = true)
  uint64_t cmd1 = APC_ACU.encodeCommand();
  Serial.println(ACU_remote::toBinaryString(cmd1));
  Serial.println(APC_ACU.toJSON());
  delay(2000);

  APC_ACU.setState(1, 18, ACUMode::DRY, 2, false);
  uint64_t cmd2 = APC_ACU.encodeCommand();
  Serial.println(ACU_remote::toBinaryString(cmd2));
  Serial.println(APC_ACU.toJSON());
  delay(2000);

  APC_ACU.setPowerState(true);
  uint64_t cmd3 = APC_ACU.encodeCommand();
  Serial.println(ACU_remote::toBinaryString(cmd3));
  Serial.println(APC_ACU.toJSON());
  delay(2000);
}

void loop() {
  ArduinoOTA.handle();  
}
