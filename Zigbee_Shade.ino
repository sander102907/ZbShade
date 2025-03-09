// Copyright 2024 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * @brief This example demonstrates simple Zigbee light bulb.
 *
 * The example demonstrates how to use Zigbee library to create a end device light bulb.
 * The light bulb is a Zigbee end device, which is controlled by a Zigbee coordinator.
 *
 * Proper Zigbee mode must be selected in Tools->Zigbee mode
 * and also the correct partition scheme must be selected in Tools->Partition Scheme.
 *
 * Please check the README.md for instructions and more detailed description.
 *
 * Created by Jan Procházka (https://github.com/P-R-O-C-H-Y/)
 */

#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

#include <ZigbeeShade.h>
// #include "esp_pm.h"

/* Zigbee shade configuration */
#define ZIGBEE_SHADE_ENDPOINT 10
uint8_t led = LED_BUILTIN;
uint8_t button = BOOT_PIN;
const int forwardPin = D9;
const int backwardPin = D10;
const int upButtonPin = D7;
const int downButtonPin = D8;
const int LdrPin = D2;
const int batteryPin = A0;

const int MS_PER_TILT_PERC_CHANGE = 100;
const float MS_MULT_FACTOR_LAST_20_CLOSE_PERC = 1.5;

// battery settings
const unsigned long reportInterval = 1 * 60 * 60 * 1000;  // 1 hour in milliseconds
unsigned long lastReportTime = 0;
uint8_t lastReportedBattery = 100;  // Store last reported battery percentage

ZigbeeShade zbShade = ZigbeeShade(ZIGBEE_SHADE_ENDPOINT);

/********************* RGB LED functions **************************/
void blink(int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_BUILTIN, LOW);   // turn the LED on (HIGH is the voltage level)
    delay(200);                       // wait for a second
    digitalWrite(LED_BUILTIN, HIGH);  // turn the LED off by making the voltage LOW
    delay(200);
  }
}
void shadeOpen() {
  Serial.println("Shade Open");
  zbShade.set_tilt_perc(50);
}

void shadeClose() {
  Serial.println("Shade Close");
  zbShade.set_tilt_perc(100);
}

void shadeStop() {
  Serial.println("Shade Stop");
  motorStop();
}

void motorForward() {
  Serial.println("Motor forward");
  digitalWrite(forwardPin, HIGH);
}

void motorBackward() {
  Serial.println("Motor backward");
  digitalWrite(backwardPin, HIGH);
}

void motorStop() {
  Serial.println("Motor stop");
  digitalWrite(forwardPin, LOW);
  digitalWrite(backwardPin, LOW);
}

void measureAndReportBattery() {
  uint32_t Vbatt = 0;
  for (int i = 0; i < 16; i++) {
    Vbatt = Vbatt + analogReadMilliVolts(batteryPin);  // ADC with correction
  }
  float Vbattf = 2 * Vbatt / 16 / 1000.0;  // attenuation ratio 1/2, mV --> V

  // Define min and max voltage of battery
  const float Vmin = 3.0;  // Minimum battery voltage
  const float Vmax = 4.2;  // Maximum battery voltage

  // Convert voltage to percentage
  float batteryPercentage = ((Vbattf - Vmin) / (Vmax - Vmin)) * 100.0;

  // Constrain the value between 0% and 100%
  batteryPercentage = constrain(batteryPercentage, 0, 100);
  uint8_t batteryPercentageUint8 = static_cast<uint8_t>(batteryPercentage);

  if (abs(lastReportedBattery - batteryPercentageUint8) >= 1) {  // Only report if changed by 1%
    Serial.print("Battery Voltage: ");
    Serial.print(Vbattf, 3);
    Serial.print(" V, Battery Percentage: ");
    Serial.print(batteryPercentage);
    Serial.println(" %");

    // Send battery percentage over Zigbee network here
    zbShade.setBatteryPercentage(batteryPercentageUint8);
    zbShade.reportBatteryPercentage();

    lastReportedBattery = batteryPercentageUint8;
  }
}

/********************* Arduino functions **************************/
void setup() {
  Serial.begin(115200);

  // Init LED and turn it OFF (if LED_PIN == RGB_BUILTIN, the rgbLedWrite() will be used under the hood)
  pinMode(led, OUTPUT);
  // digitalWrite(led, LOW);

  // Init button for factory reset
  pinMode(button, INPUT_PULLUP);

  pinMode(forwardPin, OUTPUT);
  pinMode(backwardPin, OUTPUT);

  pinMode(upButtonPin, INPUT_PULLUP);
  pinMode(downButtonPin, INPUT_PULLUP);

  pinMode(batteryPin, INPUT);

  // pinMode(LdrPin, INPUT);

  //Optional: set Zigbee device name and model
  zbShade.setManufacturerAndModel("DIY", "zbShade");

  zbShade.setPowerSource(ZB_POWER_SOURCE_BATTERY, 100);


  // Set callback function for light change
  zbShade.on_shade_open(shadeOpen);
  zbShade.on_shade_close(shadeClose);
  zbShade.on_shade_stop(shadeStop);
  zbShade.motor_forward(motorForward);
  zbShade.motor_backward(motorBackward);
  zbShade.motor_stop(motorStop);

  zbShade.msPerTiltPerc = MS_PER_TILT_PERC_CHANGE;
  zbShade.msMultFactorLast20ClosePerc = MS_MULT_FACTOR_LAST_20_CLOSE_PERC;

  //Add endpoint to Zigbee Core
  Serial.println("Adding ZbShade endpoint to Zigbee Core");
  Zigbee.addEndpoint(&zbShade);

  // When all EPs are registered, start Zigbee. By default acts as ZIGBEE_END_DEVICE
  Serial.println("Starting Zigbee..");
  if (!Zigbee.begin()) {
    Serial.println("Zigbee failed to start!");
    Serial.println("Rebooting...");
    ESP.restart();
  }

  Serial.println("Connecting to network..");
  while (!Zigbee.connected()) {
    Serial.print(".");
    blink(1);
  }

  // Turn off the LED
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.println();
  Serial.println("Connected to network");
  zbShade.on_connected();
  measureAndReportBattery();

  // int cur_cpu_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;
  // esp_pm_config_esp32_t cfg = {
  //   .max_freq_mhz = cur_cpu_freq_mhz,
  //   .min_freq_mhz = cur_cpu_freq_mhz,
  //   .light_sleep_enable = true
  // };
  // // Serial.flush();
  // esp_pm_configure(&cfg);
}

void loop() {
  // Checking button for factory reset
  if (digitalRead(button) == LOW) {  // Push button pressed
    // Key debounce handling
    delay(100);
    int startTime = millis();
    while (digitalRead(button) == LOW) {
      delay(50);
      if ((millis() - startTime) > 3000) {
        // If key pressed for more than 3secs, factory reset Zigbee and reboot
        Serial.println("Resetting Zigbee to factory and rebooting in 1s.");
        delay(1000);
        Zigbee.factoryReset();
      }
    }
  }

  if (digitalRead(upButtonPin) == LOW) {  // Button is pressed
    Serial.println("Up button Pressed!");
    while (digitalRead(upButtonPin) == LOW) {
      int tilt_perc = zbShade.get_tilt_perc();

      if (tilt_perc < 100) {
        zbShade.set_tilt_perc(tilt_perc + 1);
      }

      motorForward();
      delay(MS_PER_TILT_PERC_CHANGE);
      motorStop();
    }
  }

  if (digitalRead(downButtonPin) == LOW) {  // Button is pressed
    Serial.println("Down button Pressed!");
    while (digitalRead(downButtonPin) == LOW) {
      int tilt_perc = zbShade.get_tilt_perc();

      if (tilt_perc > 0) {
        zbShade.set_tilt_perc(tilt_perc - 1);
      }

      motorBackward();
      delay(MS_PER_TILT_PERC_CHANGE);
      motorStop();
    }
  }

  // int lightValue = analogRead(LdrPin);  // Read analog value
  // Serial.printf("Light value: %d\n", lightValue);  // Print the value (0-4095 on ESP32)

  unsigned long currentMillis = millis();
  if (currentMillis - lastReportTime >= reportInterval) {
    measureAndReportBattery();
    lastReportTime = currentMillis;
  }

  delay(100);
}
