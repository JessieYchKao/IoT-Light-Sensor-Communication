#include <ESP8266WiFi.h>
#include <LiquidCrystal_I2C.h>
#include <WiFiUdp.h> // Library for UDP
#include "WiFiCredentials.h"

#define PHOTORES_PIN A0
#define DEMO_BUTTON D7
#define UDP_PORT 5005

WiFiUDP UDP;
char incomingPacket[256];
int blink_interval = 1000;
int prev_led_time;
int sensor_interval = 1000;
int prev_sensor_time;
int send_interval = 2000;
int prev_send_time;
bool LED_STATUS = false;
bool is_init = false;
int buffer[5];
int buffer_idx = 0;
bool interrupt = false;

LiquidCrystal_I2C lcd(0x27,16,2);  // set the LCD address to 0x3F for a 16 chars and 2 line display

void setup() {
  Serial.begin(9600);

  WiFi.begin(WIFI_SSID, WIFI_PWD); //wifi name and password
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("."); 
  }

  Serial.print("Connected to WiFi, IP address: ");
  Serial.println(WiFi.localIP());

  lcd.init();
  lcd.clear();
  lcd.backlight(); 

  lcd.setCursor(0,1);   //Move cursor to character 2 on line 1
  lcd.print(WiFi.localIP());

  Serial.println();
  Serial.print("Connected, setting up UDP...");

  UDP.begin(UDP_PORT);
  Serial.printf("Now listening at IP %s, UDP port %d\n", WiFi.localIP().toString().c_str(), UDP_PORT);

  pinMode(BUILTIN_LED, OUTPUT);
  pinMode(DEMO_BUTTON, INPUT);
  digitalWrite(BUILTIN_LED, HIGH); // turn off LED
}

void loop() {
  // Initial State
  if (!is_init) {
    int packetSize = UDP.parsePacket();
    if (packetSize) {
      // receive incoming UDP packets
      int len = UDP.read(incomingPacket, 255);
      if (len > 0) incomingPacket[len] = 0;
      // Start collecting sensor data
      if (strcmp(incomingPacket, "Start") == 0) {
        is_init = true;
        // Initialized: blink onboard LED every 0.5 seconds
        blink_interval = 500;
        prev_led_time = millis();
        prev_send_time = prev_led_time;
        digitalWrite(BUILTIN_LED, LOW); // turn on LED
      }
    }
  } else { // Sensor Reading State
    unsigned long cur_time = millis();
    // Interval of the LED
    LED_blink(cur_time);
    sensor(cur_time);
    error_detect();
  }
  // Simulate broken connection
  if (digitalRead(DEMO_BUTTON) == HIGH) {
    Serial.println("Interrupt!!!!");
    interrupt = true;
  }
  delay(0.05);
}

void error_detect() {
  int packetSize = UDP.parsePacket();
  if (packetSize) {
    // receive incoming UDP packets
    int len = UDP.read(incomingPacket, 255);
    if (len > 0) incomingPacket[len] = 0;
    // Error occurs, turn off onboard LED, stop collecting data, and go back to the inital state
    if (strcmp(incomingPacket, "Reset") == 0) {
      Serial.println("Error occurs, stop collecting data...");
      // Turn off onboard LED
      LED_STATUS = false;
      digitalWrite(BUILTIN_LED, HIGH);

      interrupt = false;
      is_init = false;
      // Reset buffer
      for(int i=0; i<5; i++) buffer[i] = 0;
      buffer_idx = 0;
    }
  }
}


void LED_blink(unsigned long cur_time) {
  if (cur_time - prev_led_time >= blink_interval/2) {
    LED_STATUS = !LED_STATUS;
    if (LED_STATUS) digitalWrite(BUILTIN_LED, LOW);
    else digitalWrite(BUILTIN_LED, HIGH);
    prev_led_time = cur_time;
  }
}

void sensor(unsigned long cur_time) {
  if (cur_time - prev_sensor_time >= sensor_interval) {
    int value = analogRead(PHOTORES_PIN);
    prev_sensor_time = cur_time;
    buffer[buffer_idx] = value;
    buffer_idx = buffer_idx + 1;
    if (buffer_idx >= 5) buffer_idx = 0;
    calc_n_send(cur_time);
  }
}

void calc_n_send(unsigned long cur_time) {
  if (cur_time - prev_send_time >= send_interval) {
    int sum = 0;
    int i = 0;
    for(; i<5; i++) {
      if (buffer[i] == 0) break;
      sum = sum + buffer[i];
    }
    if (i < 5) return;
    int avg = sum/5;
    char data[20];
    sprintf(data, "%d ", avg);
    prev_send_time = cur_time;
    // Send data through UDP
    if (!interrupt) {
      UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
      UDP.write(data);
      UDP.endPacket();
      Serial.print("Send data to Raspberry Pi: "); Serial.println(data);
    } else {
      Serial.println("Oops, cannot send data to Raspberry Pi... ");
    }
  }
}