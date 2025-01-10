#include "HX711.h"
#include <Wire.h>
#include <Pushbutton.h>
#include <WiFi.h>
#include <ArduinoMqttClient.h>

// HX711 circuit wiring
const int LOADCELL_DOUT_PIN = 2;
const int LOADCELL_SCK_PIN = 3;

HX711 scale;
int reading;
int lastReading;
// REPLACE WITH YOUR CALIBRATION FACTOR
#define CALIBRATION_FACTOR -476.592

// Button
#define BUTTON_PIN 4
Pushbutton button(BUTTON_PIN);

// Stepper motor pins
int int1 = 10;
int int2 = 11;
int int3 = 12;
int int4 = 13;

int counterClockwise = 8;
int clockwise = 7;

int pole1[] = {0, 0, 0, 0, 0, 1, 1, 1, 0};
int pole2[] = {0, 0, 0, 1, 1, 1, 0, 0, 0};
int pole3[] = {0, 1, 1, 1, 0, 0, 0, 0, 0};
int pole4[] = {1, 1, 0, 0, 0, 0, 0, 1, 0};

int counter1 = 0;
int counter2 = 0;

int dir = 3;
int poleStep = 0;

// WiFi and MQTT settings
char ssid[] = "IoTatelierF2144";    // 22 tot 44 
char pass[] = "IoTatelier";
WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);
const char broker[] = "192.168.144.1";
const int port = 1883;
const char publishTopic[] = "petfeeder/rick";
const char subscribeTopic[] = "RubenHandl/afstandsSensor";
long count = 0;
const long interval = 1000; // analog read interval
unsigned long previousMillis = 0;

void connectToMQTT() {
  Serial.println("Wifi connecting");
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("MQTT connecting");
  bool MQTTconnected = false;
  while (!MQTTconnected) {
    if (!mqttClient.connect(broker, port))
      delay(1000);
    else
      MQTTconnected = true;
  }
  mqttClient.onMessage(onMqttMessage);
  mqttClient.subscribe(subscribeTopic);
  Serial.println("Setup complete");
}

void onMqttMessage(int messageSize) {
  // Handle incoming messages here
}

void setup() {
  Serial.begin(9600);

  // Initialize the scale
  Serial.println("Initializing the scale");
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(CALIBRATION_FACTOR); // this value is obtained by calibrating the scale with known weights
  scale.tare(); // reset the scale to 0

  // Initialize the button
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Initialize the stepper motor pins
  pinMode(int1, OUTPUT);
  pinMode(int2, OUTPUT);
  pinMode(int3, OUTPUT);
  pinMode(int4, OUTPUT);

  pinMode(counterClockwise, INPUT_PULLUP);
  pinMode(clockwise, INPUT_PULLUP);

  // Connect to WiFi and MQTT
  connectToMQTT();
}

void loop() {
  // Handle button press for taring the scale
  if (button.getSingleDebouncedPress()) {
    Serial.print("tare...");
    scale.tare();
  }

  // Read and print the weight from the scale
  if (scale.wait_ready_timeout(200)) {
    reading = round(scale.get_units());
    Serial.print("Weight: ");
    Serial.println(reading);
    if (reading != lastReading) {
      // Publish the weight to the MQTT server
      mqttClient.beginMessage(publishTopic);
      mqttClient.print(reading);
      mqttClient.endMessage();
    }
    lastReading = reading;
  } else {
    Serial.println("HX711 not found.");
  }

  // Stop the stepper motor if the scale reading is 50
  if (reading == 50) {
    dir = 3; // Stop the stepper motor
  }

  // Handle stepper motor control
  int btn1State = digitalRead(counterClockwise);
  int btn2State = digitalRead(clockwise);

  if (btn1State == LOW) {
    counter1++;
    delay(500);

    if (counter1 != 2) {
      counter2 = 0;
      dir = 1;
    } else {
      counter1 = 0;
      dir = 3;
    }
  }

  if (btn2State == LOW) {
    counter2++;
    delay(500);

    if (counter2 != 2) {
      counter1 = 0;
      dir = 2;
    } else {
      counter2 = 0;
      dir = 3;
    }
  }

  if (dir == 1) {
    poleStep++;
    driverStepper(poleStep);
  } else if (dir == 2) {
    poleStep--;
    driverStepper(poleStep);
  } else {
    driverStepper(8);
  }

  if (poleStep > 7) {
    poleStep = 0;
  } else if (poleStep < 0) {
    poleStep = 7;
  }
  delay(1);
}

void driverStepper(int step){
  digitalWrite(int1, pole1[step]);
  digitalWrite(int2, pole2[step]);
  digitalWrite(int3, pole3[step]);
  digitalWrite(int4, pole4[step]);
}
