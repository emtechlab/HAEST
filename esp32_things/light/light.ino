

// includes
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <ESP.h>
#include <esp_task_wdt.h>

void setup() {
 
  Serial.begin(115200);
  pinMode(A7, INPUT);
}


// http functions
void loop(){

  float light = analogRead(A7)*3.3/4096.0;
  float microamps = light * 100;
  Serial.print(light);
  Serial.print(" -> ");
  Serial.println(microamps*2.0);
  delay(100);
}
