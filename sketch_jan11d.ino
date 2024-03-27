#include <WiFiS3.h>
#include <ArduinoMqttClient.h>
#include "HX711.h"
#include <Wire.h>
#include <Pushbutton.h>

char ssid[] = "IoTatelierF2122";    // 22 tot 44 
char pass[] = "IoTatelier";
WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);
const char broker[] = "192.168.144.1";
const int port = 1883;
const char publishTopic[] = "rickvelting/petfeeder"; 
const char subscribeTopic[] = "Toufik/plantvochtigheid";
long count = 0;
const long interval = 1000;   //analog read interval
unsigned long previousMillis = 0;

#define ROW_NUM     4 // four rows
#define COLUMN_NUM  3 // three columns

#define LOADCELL_DOUT_PIN 2
#define LOADCELL_SCK_PIN 3
HX711 scale;
int reading;
int lastReading;
#define CALIBRATION_FACTOR -476.592
#define BUTTON_PIN 4
Pushbutton button(BUTTON_PIN);

int int1 = 10;
int int2 = 11;
int int3 = 12;
int int4 = 13;

int counterClockwise = 8;
int clockwise = 7;

int pole1[] = {0,0,0,0,0,1,1,1,0};
int pole2[] = {0,0,0,1,1,1,0,0,0};
int pole3[] = {0,1,1,1,0,0,0,0,0};
int pole4[] = {1,1,0,0,0,0,0,1,0};

int counter1 = 0;
int counter2 = 0;

int dir = 3;
int poleStep = 0;

void connecttoMQTT() {
    Serial.println("Wifi connecting");
  
    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
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

void sendMessagetoMQTT() {
    mqttClient.poll();
    
    unsigned long currentMillis = millis();
    
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        
        Serial.print("Sending message to topic: ");
        Serial.println(publishTopic);
        Serial.println(reading); // Assuming 'value' is the weight reading
        
        mqttClient.beginMessage(publishTopic, true, 0);
        mqttClient.print(reading); // Assuming 'value' is the weight reading
        mqttClient.endMessage();
    }
    delay(1);
}

void onMqttMessage(int messageSize) {
    Serial.print("Received a message with topic '");
    Serial.println(mqttClient.messageTopic());

    String message = "";
    while (mqttClient.available()) {
        message.concat((char)mqttClient.read());
    }
    Serial.println(message);
}

void callback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived in topic: ");
    Serial.println(topic);
    Serial.print("Message:");
    for (int i = 0; i < length; i++) {
        Serial.print((char)payload[i]);
    }
    Serial.println();
    // Perform actions based on the received message (if needed)
}

void driverStepper(int step) {
    digitalWrite(int1, pole1[step]);
    digitalWrite(int2, pole2[step]);
    digitalWrite(int3, pole3[step]);
    digitalWrite(int4, pole4[step]);
}

void setup() {
    pinMode(int1, OUTPUT);
    pinMode(int2, OUTPUT);
    pinMode(int3, OUTPUT);
    pinMode(int4, OUTPUT);
    pinMode(counterClockwise, INPUT_PULLUP);
    pinMode(clockwise, INPUT_PULLUP);
    
    Serial.begin(9600);

    Serial.println("Initializing the scale");
    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
    scale.set_scale(CALIBRATION_FACTOR);   // this value is obtained by calibrating the scale with known weights
    scale.tare();               // reset the scale to 0
}

void loop() {
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
  
    if (button.getSingleDebouncedPress()) {
        Serial.println("tare...");
        scale.tare();
    }

    if (scale.wait_ready_timeout(200)) {
        reading = round(scale.get_units());
        Serial.print("Weight: ");
        Serial.println(reading);
        if (reading != lastReading) {
            sendMessagetoMQTT();
        }
        lastReading = reading;
    } else {
        Serial.println("HX711 not found.");
    }
  
    connecttoMQTT();
    delay(1);
}
