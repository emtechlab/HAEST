/* Read sensors, timestamp them and send over to the internet*/

// includes
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <soc/rtc.h>
#include <ESP.h>
#include <esp_task_wdt.h>

// definitions
#define USE_SERIAL Serial
#define MIC_DATA 2

// Global variables
// wifi connection handle
WiFiMulti wifiMulti;
const uint16_t port = 8091;
const char * host = "10.42.0.1";

//Mic struct
// microphone data
WiFiClient clientMic;
uint64_t baseTimeMic;
uint8_t countAudioFrame=0;
uint16_t countMic=0;
uint16_t mic_data[16640];
unsigned char mic_http[33280];

//Motion struct
// motion data
WiFiClient clientMotion;
uint64_t baseTimeMotion;
uint8_t countMotionFrame=0;
uint16_t countMotion=0;
uint16_t motion_data[4160];
unsigned char motion_http[8320];

// define timers
hw_timer_t* timer0 = NULL;
hw_timer_t* timer1 = NULL;

// defining semaphores
// for notifications handling
BaseType_t pxMicTaskWoken;
BaseType_t pxMotionTaskWoken;

// defining task handles
// interrupt tasks for core 0
TaskHandle_t intMic;
TaskHandle_t intMotion;

// http tasks for posting data
TaskHandle_t httpMic;
TaskHandle_t httpMotion;


void IRAM_ATTR onTimerMic() {    

    // mic data
    uint16_t audio = analogRead(A6);
    // time stamp
    uint64_t tim64 = esp_timer_get_time();
    
    // copy data
    if(countMic%256 == 0){
      baseTimeMic = tim64;
      if(countMic%256 <= 32){
        mic_data[(countMic%256)*3+MIC_DATA*countMic] = audio;
        mic_data[(countMic%256)*3+MIC_DATA*countMic+4] = tim64 & 0xFFFF;
        mic_data[(countMic%256)*3+MIC_DATA*countMic+3] = (tim64>>16) & 0xFFFF;
        mic_data[(countMic%256)*3+MIC_DATA*countMic+2] = (tim64>>16) & 0xFFFF;
        mic_data[(countMic%256)*3+MIC_DATA*countMic+1] = (tim64>>16) & 0xFFFF;
      }

    }else{
      tim64 = baseTimeMic - tim64;
      if(countMic%256 <= 32){
        mic_data[((countMic%256)+1)*3+MIC_DATA*countMic] = audio;
        mic_data[((countMic%256)+1)*3+MIC_DATA*countMic+1] = (tim64) & 0xFFFF;
      }
    }
    countMic ++;

    if(countMic%8000 == 0){
        USE_SERIAL.printf("Audio: %d\n", audio);
        memcpy(mic_http, mic_data, 33280);        
        mic_http[33279] = countAudioFrame;
        countMic = 0;
        if(countAudioFrame == 255){
            countAudioFrame = 0;  
        }else{
          countAudioFrame ++;
        }

        pxMicTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(httpMic, &pxMicTaskWoken);
        if (pxMicTaskWoken == pdTRUE){
            portYIELD_FROM_ISR();
        }
    }

}

void IRAM_ATTR onTimerMotion() {    

    // mic data
    uint16_t motion = analogRead(A7);
    // time stamp
    uint64_t tim64 = esp_timer_get_time();
    
    // copy data
    if(countMotion%256 == 0){
      baseTimeMotion = tim64;
      if(countMotion%256 <= 8){
        motion_data[(countMotion%256)*3+MIC_DATA*countMotion] = motion;
        motion_data[(countMotion%256)*3+MIC_DATA*countMotion+4] = tim64 & 0xFFFF;
        motion_data[(countMotion%256)*3+MIC_DATA*countMotion+3] = (tim64>>16) & 0xFFFF;
        motion_data[(countMotion%256)*3+MIC_DATA*countMotion+2] = (tim64>>16) & 0xFFFF;
        motion_data[(countMotion%256)*3+MIC_DATA*countMotion+1] = (tim64>>16) & 0xFFFF;
      }

    }else{
      tim64 = baseTimeMotion - tim64;
      if(countMotion%256 <= 8){
        motion_data[((countMotion%256)+1)*3+MIC_DATA*countMotion] = motion;
        motion_data[((countMotion%256)+1)*3+MIC_DATA*countMotion+1] = (tim64) & 0xFFFF;
      }
    }
    countMotion ++;

    if(countMotion%2000 == 0){
        USE_SERIAL.printf("Motion: %d\n", motion);
        memcpy(motion_http, motion_data, 8320);        
        motion_http[8319] = countMotionFrame;
        countMotion = 0;
        if(countMotionFrame == 255){
            countMotionFrame = 0;  
        }else{
          countMotionFrame ++;
        }
        
        pxMotionTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(httpMotion, &pxMotionTaskWoken);
        if (pxMotionTaskWoken == pdTRUE){
            portYIELD_FROM_ISR();
        }
    }
}

void setup() {
 
  Serial.begin(115200);
  
  //USE_SERIAL.println(b);
  // getting wifi connection
  for(uint8_t t = 4; t > 0; t--) {
      USE_SERIAL.printf("[SETUP] WAIT %d...\n", t);
      USE_SERIAL.flush();
      delay(1000);
  }

  //wifiMulti.addAP("Nokia 5", "87654321");
  //wifiMulti.addAP("rpi4b", "inception");
  wifiMulti.addAP("experiments", "inception");
  // wait for connection here
  USE_SERIAL.printf("Waiting for wifi connection ...\n");
  //while(wifiMulti.run() != WL_CONNECTED);
  wifiMulti.run();
  USE_SERIAL.printf("Connected to wifi successfully ...\n");

  // creating socket connections for streaming
  while (!clientMotion.connect(host, port)) {
        USE_SERIAL.println("Connection to host failed from MOTION");
  }
  while (!clientMic.connect(host, port)) {
        USE_SERIAL.println("Connection to host failed from AUDIO");
  }

  // initializing tasks
  xTaskCreatePinnedToCore(httpMotionTask,   // function to implement task
                            "httpMotion",      // Task name
                            4096,         // stack size in words
                            NULL,           // arguments
                            2,              // priority
                            &httpMotion,       // task handle
                            0);
  delay(1000);
  xTaskCreatePinnedToCore(intMotionTask,   // function to implement task
                            "intMotion",      // Task name
                            1000,           // stack size in words
                            NULL,           // arguments
                            1,              // priority
                            &intMotion,       // task handle
                            1);
  delay(1000);
  xTaskCreatePinnedToCore(httpMicTask,   // function to implement task
                            "httpMic",      // Task name
                            4096,         // stack size in words
                            NULL,           // arguments
                            2,              // priority
                            &httpMic,       // task handle
                            0);
  delay(1000);
  xTaskCreatePinnedToCore(intMicTask,   // function to implement task
                            "intMic",      // Task name
                            1000,           // stack size in words
                            NULL,           // arguments
                            1,              // priority
                            &intMic,       // task handle
                            1);
                            
}


// http functions
void httpMicTask(void * parameter){

  USE_SERIAL.printf("Timer initialized ...\n");
  /*while (!clientMic.connect(host, port)) {
        USE_SERIAL.println("Connection to host failed from AUDIO");
  }*/
  clientMic.print(WiFi.macAddress());
  clientMic.print("AUD");
  USE_SERIAL.println("Connected to server successful!");

  for(;;){ 

    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    USE_SERIAL.printf("AUDIO triggered by interrupt: %d\n", countAudioFrame);
    
    // wait for WiFi connection
    if((WiFi.status() == WL_CONNECTED)) {
      
        uint64_t tim64 = esp_timer_get_time();
        int httpCode = clientMic.write((uint8_t *)(mic_http), 33280);
        // httpCode will be negative on error
        tim64 = esp_timer_get_time() - tim64;
        tim64 /= 1000;
        uint32_t tim32 = tim64 & 0xFFFFFFFF;
        if(httpCode > 0) {
            USE_SERIAL.printf("[HTTP] POST... code: %d in %d ms\n", httpCode, tim32);
        }
        else{
          USE_SERIAL.printf("[HTTP] POST timeout: %d\n", tim32);
        }
        USE_SERIAL.printf("Sent ...\n");
    }
    else{
        USE_SERIAL.print("Exiting HTTP ....\n");
    }
    portYIELD();
  }
}

void httpMotionTask(void * parameter){

  USE_SERIAL.printf("Timer initialized ...\n");
  /*while (!clientMotion.connect(host, port)) {
        USE_SERIAL.println("Connection to host failed from MOTION");
  }*/
  clientMotion.print(WiFi.macAddress());
  clientMotion.print("MOT");
  USE_SERIAL.println("Connected to server successful!");

  for(;;){

    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    USE_SERIAL.printf("MOTIOn triggered by interrupt: %d\n", countMotionFrame);
    
    // wait for WiFi connection
    if((WiFi.status() == WL_CONNECTED)) {
      
        uint64_t tim64 = esp_timer_get_time();
        int httpCode = clientMotion.write((uint8_t *)(motion_http), 8320);
        // httpCode will be negative on error
        tim64 = esp_timer_get_time() - tim64;
        tim64 /= 1000;
        uint32_t tim32 = tim64 & 0xFFFFFFFF;
        if(httpCode > 0) {
            USE_SERIAL.printf("[HTTP] POST... code: %d in %d ms\n", httpCode, tim32);
        }
        else{
          USE_SERIAL.printf("[HTTP] POST timeout: %d\n", tim32);
        }
        USE_SERIAL.printf("Sent ...\n");
    }
    else{
        USE_SERIAL.print("Exiting HTTP ....\n");
    }
    portYIELD();
  }
}


// interrupt task functions
void intMicTask(void * parameter){
    timer0 = timerBegin(0, 40, true);
    timerAttachInterrupt(timer0, &onTimerMic, true);
    timerAlarmWrite(timer0, 125, true);
    timerAlarmEnable(timer0);

    vTaskSuspend(NULL);
}

void intMotionTask(void * parameter){
    timer1 = timerBegin(1, 80, true);
    timerAttachInterrupt(timer1, &onTimerMotion, true);
    timerAlarmWrite(timer1, 1000, true);
    timerAlarmEnable(timer1);

    vTaskSuspend(NULL);
}

void loop(){
    delay(10000);
}
