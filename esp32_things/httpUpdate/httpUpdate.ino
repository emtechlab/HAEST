/**
 * BasicHTTPClient.ino
 *
 *  Created on: 24.05.2015
 *
 */

#include <Arduino.h>

#include <WiFi.h>
#include <WiFiMulti.h>

#include <HTTPClient.h>

#include <soc/rtc.h>

// #include "SPIFFS.h"
#include <ESP.h>

#define USE_SERIAL Serial
// time stamping  definintions
#define RTC_CNTL_TIME_UPDATE_REG 0x3FF4800C
#define  RTC_CNTL_CLK_CONF_REG 0x3FF48070
#define mask 0x3FFFFFFF
#define newVal 0x40000000
#define updat 0x80000000

// wifi connection handle
WiFiMulti wifiMulti;
// defining interrupt pin
int intPin = 0;     // setting to onboard button for now
//int intPin = 36;     // setting to onboard button for now

// defining task handles
TaskHandle_t intGpio;

// get semaphores
SemaphoreHandle_t barrierSemaphore = xSemaphoreCreateBinary();

// data write count
int count = 0;

// collecting data
String *timestamp_data = (String *) malloc(10240);

// interrup service routine
void IRAM_ATTR isr() {

    USE_SERIAL.printf("INSIDE interrupt\n");
    // getting timestamp
    uint64_t tim64 = rtc_time_get();
    uint32_t tim32 = tim64 & 0xFFFFFFFF;

    /*USE_SERIAL.printf("INSIDE interrupt 2\n");
    if (!SPIFFS.begin(true)) {
      Serial.println("An Error has occurred while mounting SPIFFS");
      return;
    }

    USE_SERIAL.printf("INSIDE interrupt 3\n");
    File file = SPIFFS.open("/test.txt", FILE_WRITE);
 
    if (!file) {
      Serial.println("There was an error opening the file for writing");
      return;
    }

    USE_SERIAL.printf("INSIDE interrupt 4\n");
    if (file.print(String(tim32))) {
      Serial.println("File was written");
      count++;
    } else {
      Serial.println("File write failed");
    }

    USE_SERIAL.printf("INSIDE interrupt 5\n");
    file.close();*/

    count++;
    USE_SERIAL.println(*timestamp_data);
    *timestamp_data += ","+String(tim32)+",";
    USE_SERIAL.println(*timestamp_data);
    if (count > 0 && count%1 == 0 ){
        USE_SERIAL.printf("Giving up semaphore.\n");
        xSemaphoreGiveFromISR(barrierSemaphore, NULL);
    }
}

void setup() {

    USE_SERIAL.begin(115200);

    USE_SERIAL.println();
    USE_SERIAL.println();
    USE_SERIAL.println();

    for(uint8_t t = 4; t > 0; t--) {
        USE_SERIAL.printf("[SETUP] WAIT %d...\n", t);
        USE_SERIAL.flush();
        delay(1000);
    }

    wifiMulti.addAP("rpi4b", "inception");
    // wait for connection here
    USE_SERIAL.printf("Waiting for wifi connection ...\n");
    while(wifiMulti.run() != WL_CONNECTED);
    USE_SERIAL.printf("Connected to wifi successfully ...\n");

    xTaskCreatePinnedToCore(interruptTask,   // function to implement task
                            "intGpio",      // Task name
                            1000,           // stack size in words
                            NULL,           // arguments
                            0,              // priority
                            &intGpio,       // task handle
                            0);             // core number

     //interruptTask();

}

// intGpio task code, to attach interrupt to core zero
void interruptTask(void * parameter){
//void interruptTask(){
  
    
    // attaching interrupt
    pinMode(intPin, INPUT_PULLUP);
    attachInterrupt(intPin, isr, RISING);
    vTaskSuspend(NULL);
}


//int count = 0;
void loop() {

    long rssi = WiFi.RSSI();
    USE_SERIAL.print("RSSI:");
    USE_SERIAL.println(rssi);
    uint32_t freq =  ESP.getCpuFreqMHz();
    USE_SERIAL.printf("Clock frequency: %d\n", freq);
    xSemaphoreTake(barrierSemaphore, portMAX_DELAY);
    USE_SERIAL.printf("triggered by interrupt: ");
    USE_SERIAL.println(count);

    // enabling timestamping
    uint32_t rtc_src = *((volatile uint32_t *) RTC_CNTL_CLK_CONF_REG);
    *((volatile uint32_t *) RTC_CNTL_CLK_CONF_REG) = (rtc_src & mask) | newVal;
    rtc_clk_32k_enable_external();
    
    // wait for WiFi connection
    if((wifiMulti.run() == WL_CONNECTED)) {
        //count += 1;
        HTTPClient http;

        USE_SERIAL.print("[HTTP] begin...\n");
        // configure traged server and url
        //http.begin("https://www.howsmyssl.com/a/check", ca); //HTTPS
        http.begin("http://192.168.137.254:8080/"); //HTTP
        //http.begin("https://jsonplaceholder.typicode.com/posts/1");
        
        //USE_SERIAL.print("[HTTP] GET...\n");
        // start connection and send HTTP header
        // http.addHeader("Content-Type", "text/plain");
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        //String* cand = (String *) malloc(4096);
        //*cand = "length=";
        //for (int i=0; i<4079; i++){
        //    *cand += 'a';  
        //}
        //USE_SERIAL.printf("another_string=%s \n", (*cand).c_str());
        String data = "length="+*timestamp_data;
        USE_SERIAL.println(data);
        int httpCode = http.POST(data);
        // resetting data buffer
        count=0;
        free(timestamp_data);
        timestamp_data = (String *) malloc(10240);
        //free(cand);

        // httpCode will be negative on error
        if(httpCode > 0) {
            // HTTP header has been send and Server response header has been handled
            USE_SERIAL.printf("[HTTP] GET... code: %d\n", httpCode);

            // file found at server
            //if(httpCode == HTTP_CODE_OK) {
               // String payload = http.getString();
                //USE_SERIAL.println(payload);
            //}
        //} else {
            //USE_SERIAL.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
        }

        http.end();
    }
    //if (count%10==0)
    //    USE_SERIAL.println(count);
    //delay(5000);
}
