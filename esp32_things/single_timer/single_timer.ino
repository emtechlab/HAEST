/* Read sensors, timestamp them and send over to the internet*/

// includes
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <soc/rtc.h>
#include <ESP.h>
#include <esp_task_wdt.h>
//#include <task.h>

// definitions
#define USE_SERIAL Serial
#define RTC_CNTL_TIME_UPDATE_REG 0x3FF4800C
#define  RTC_CNTL_CLK_CONF_REG 0x3FF48070
#define mask 0x3FFFFFFF
#define newVal 0x40000000
#define updat 0x80000000
#define MIC_DATA 2

// Global variables
// wifi connection handle
WiFiMulti wifiMulti;
WiFiClient client;
const uint16_t port = 8090;
const char * host = "10.42.0.1";

//RSSI struct
wifi_ap_record_t wifidata;
// microphone data
uint64_t baseTime;
uint8_t countAudioFrame=0;
uint16_t countMic=0;
uint16_t mic_data[16640];
//uint16_t mic_data2[8320];
//uint16_t mic_data3[8320];
//uint16_t mic_data4[8320];
//uint8_t mic_http[66560];
//uint8_t mic_http[33280];
unsigned char mic_http[33280];

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// data write count
volatile int countRSSI = 0;

// data buffers
String rssi_data = "length=";
String transmit_rssi;

// define timers
hw_timer_t * timer0 = NULL;

// defining semaphores
// for notifications handling
BaseType_t pxRSSITaskWoken;

// get semaphores
SemaphoreHandle_t rssiSemaphore = xSemaphoreCreateBinary();
SemaphoreHandle_t rssiHttpSemaphore = xSemaphoreCreateBinary();


// defining task handles
// interrupt tasks for core 0
TaskHandle_t intRSSI;
TaskHandle_t dummy;

// http tasks for posting data
TaskHandle_t httpRSSI;

void IRAM_ATTR onTimerRSSI() {    

    // mic data
    uint16_t audio = analogRead(A6);
    // time stamp
    uint64_t tim64 = esp_timer_get_time();
    //uint64_t tim64 = 0;
    
    // copy data
    if(countMic%256 == 0){
      baseTime = tim64;
      if(countMic%256 <= 32){
        mic_data[(countMic%256)*3+MIC_DATA*countMic] = audio;
        mic_data[(countMic%256)*3+MIC_DATA*countMic+4] = tim64 & 0xFFFF;
        mic_data[(countMic%256)*3+MIC_DATA*countMic+3] = (tim64>>16) & 0xFFFF;
        mic_data[(countMic%256)*3+MIC_DATA*countMic+2] = (tim64>>16) & 0xFFFF;
        mic_data[(countMic%256)*3+MIC_DATA*countMic+1] = (tim64>>16) & 0xFFFF;
      }
      /*else if(countMic%256 > 16 && countMic%256 <= 32){
        mic_data2[(countMic%256)*3+MIC_DATA*countMic] = audio;
        mic_data2[(countMic%256)*3+MIC_DATA*countMic+4] = tim64 & 0xFFFF;
        mic_data2[(countMic%256)*3+MIC_DATA*countMic+3] = (tim64>>16) & 0xFFFF;
        mic_data2[(countMic%256)*3+MIC_DATA*countMic+2] = (tim64>>16) & 0xFFFF;
        mic_data2[(countMic%256)*3+MIC_DATA*countMic+1] = (tim64>>16) & 0xFFFF;
      }
      else if(countMic%256 > 32 && countMic%256 <= 48){
        mic_data3[(countMic%256)*3+MIC_DATA*countMic] = audio;
        mic_data3[(countMic%256)*3+MIC_DATA*countMic+4] = tim64 & 0xFFFF;
        mic_data3[(countMic%256)*3+MIC_DATA*countMic+3] = (tim64>>16) & 0xFFFF;
        mic_data3[(countMic%256)*3+MIC_DATA*countMic+2] = (tim64>>16) & 0xFFFF;
        mic_data3[(countMic%256)*3+MIC_DATA*countMic+1] = (tim64>>16) & 0xFFFF;
      }
      else if(countMic%256 > 48){
        mic_data4[(countMic%256)*3+MIC_DATA*countMic] = audio;
        mic_data4[(countMic%256)*3+MIC_DATA*countMic+4] = tim64 & 0xFFFF;
        mic_data4[(countMic%256)*3+MIC_DATA*countMic+3] = (tim64>>16) & 0xFFFF;
        mic_data4[(countMic%256)*3+MIC_DATA*countMic+2] = (tim64>>16) & 0xFFFF;
        mic_data4[(countMic%256)*3+MIC_DATA*countMic+1] = (tim64>>16) & 0xFFFF;
      }*/
    }else{
      tim64 = baseTime - tim64;
      if(countMic%256 <= 16){
        mic_data[((countMic%256)+1)*3+MIC_DATA*countMic] = audio;
        mic_data[((countMic%256)+1)*3+MIC_DATA*countMic+1] = (tim64) & 0xFFFF;
      }
      /*else if(countMic%256 > 16 && countMic%256 <= 32){
        mic_data2[((countMic%256)+1)*3+MIC_DATA*countMic] = audio;
        mic_data2[((countMic%256)+1)*3+MIC_DATA*countMic+1] = (tim64) & 0xFFFF;
      }
      else if(countMic%256 > 32 && countMic%256 <= 48){
        mic_data3[((countMic%256)+1)*3+MIC_DATA*countMic] = audio;
        mic_data3[((countMic%256)+1)*3+MIC_DATA*countMic+1] = (tim64) & 0xFFFF;
      }
      else if(countMic%256 > 48){
        mic_data4[((countMic%256)+1)*3+MIC_DATA*countMic] = audio;
        mic_data4[((countMic%256)+1)*3+MIC_DATA*countMic+1] = (tim64) & 0xFFFF;
      }*/
    }
    countMic ++;

    if(countMic%8000 == 0){
        //portENTER_CRITICAL(&mux);
        memcpy(mic_http, mic_data, 33280);
        //memcpy(&(mic_http[16640]), mic_data2, 16640);
        //memcpy(&(mic_http[16640*2]), mic_data3, 16640);
        //memcpy(&(mic_http[16640*3]), mic_data4, 16640);
        //mic_http[66560] = countAudioFrame;
        
        mic_http[33279] = countAudioFrame;
        //if (countMic == 8000){
          //flagMic = true;
          countMic = 0;
        if(countAudioFrame == 255){
            countAudioFrame = 0;  
        }else{
          countAudioFrame ++;
        }
        //}
        //portEXIT_CRITICAL(&mux);
        pxRSSITaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(httpRSSI, &pxRSSITaskWoken);
        if (pxRSSITaskWoken == pdTRUE){
            portYIELD_FROM_ISR();
        }
        //USE_SERIAL.printf("Mic data samples: %d", countMic);
    }
    
}

// Task functions
// intRSSI task code, to attach interrupt to core zero
void rssiTask(void * parameter){

    //esp_task_wdt_delete(intRSSI);
    // RSSI Timer
    /*timer0 = timerBegin(0, 80, true);
    timerAttachInterrupt(timer0, &onTimerRSSI, true);
    timerAlarmWrite(timer0, 1000, true);
    timerAlarmEnable(timer0);*/
    
    for(;;){
      // wait for timer interrupt to take the reading
      //xSemaphoreTake(rssiSemaphore, portMAX_DELAY);
      //xSemaphoreGive(rssiSemaphore);
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      USE_SERIAL.printf("What the fuck is wrong with you ...?");
      countRSSI++;
      // Get RSSI
      int32_t rssi = WiFi.RSSI();
      // getting timestamp
      uint64_t tim64 = rtc_time_get();

      rssi_data += (String(rssi)+"-");//+String(tim64)+",")

      if(countRSSI>0  && countRSSI%1000==0){
        //xSemaphoreTake(rssiHttpSemaphore, portMAX_DELAY);
        transmit_rssi = rssi_data;
        rssi_data = "length=";
        countRSSI = 0; 
        //xSemaphoreGive(rssiHttpSemaphore);
      }
      portYIELD();
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
  
  // preparing payloads
  // mic
  //char var[4] = {'m','i', 'c' , '='};
  //memcpy(mic_http, var, 4);
  //mic_http[40004] = '\0';
  
  // enabling timestamping
  uint32_t rtc_src = *((volatile uint32_t *) RTC_CNTL_CLK_CONF_REG);
  *((volatile uint32_t *) RTC_CNTL_CLK_CONF_REG) = (rtc_src & mask) | newVal;
  rtc_clk_32k_enable_external();

  // initializing tasks
  
  /*xTaskCreatePinnedToCore(rssiTask,   // function to implement task
                            "intRSSI",      // Task name
                            20000,           // stack size in words
                            NULL,           // arguments
                            2,              // priority
                            &intRSSI,       // task handle
                            0);*/

  xTaskCreatePinnedToCore(httpRssiTask,   // function to implement task
                            "httpRSSI",      // Task name
                            4096,         // stack size in words
                            NULL,           // arguments
                            2,              // priority
                            &httpRSSI,       // task handle
                            0);
  delay(500);
  xTaskCreatePinnedToCore(dummyTask,   // function to implement task
                            "dummy",      // Task name
                            1000,           // stack size in words
                            NULL,           // arguments
                            1,              // priority
                            &dummy,       // task handle
                            1);

}

// http functions
void httpRssiTask(void * parameter){

  /*while(true)
  {
      delay(10000);  
  }*/

  USE_SERIAL.printf("Timer initialized ...\n");
  while (!client.connect(host, port)) {
 
        USE_SERIAL.println("Connection to host failed");
 
        //delay(1000);
        //return;
  }
  client.print(WiFi.macAddress());
  USE_SERIAL.println("Connected to server successful!");
  /*HTTPClient http;
  http.begin("http://10.42.0.1:8080/"); //HTTP
  http.addHeader("Content-Type", "application/octet-stream");
  http.setTimeout(400);
  http.setConnectTimeout(400);*/

  //esp_task_wdt_delete(httpRSSI);
  while(true){ 
    //xSemaphoreTake(rssiHttpSemaphore, portMAX_DELAY);
    //xSemaphoreGive(rssiHttpSemaphore);
    
    // while(flagMic == true){}
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    USE_SERIAL.printf("MIC triggered by interrupt: %d\n", countAudioFrame);
    //USE_SERIAL.println(countRSSI);
    
    // wait for WiFi connection
    if((WiFi.status() == WL_CONNECTED)) {
        //count += 1;
        

        // USE_SERIAL.print("[HTTP] begin...\n");
        // configure traged server and url
        // http.begin("http://192.168.137.254:8080/"); //HTTP
        
        //http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        //http.addHeader("Content-Type", "application/octet-stream");
        
        //USE_SERIAL.print("[HTTP] POST ...\n");
        uint64_t tim64 = esp_timer_get_time();
        //int httpCode = http.POST((uint8_t *)(mic_http), 33280);
        int httpCode = client.write((uint8_t *)(mic_http), 33280);
        //unsigned char hello[17] = {'m','i','c','=','h', 'e', 'l', 'l', 'o'};
        //uint8_t val[7];
        //memcpy(&(hello[9]), val, 7);
        //hello[16] = '\0';
        //int httpCode = http.POST((uint8_t *)(hello), 17);
        //int httpCode = http.POST((uint8_t *)(post), 20);
        //USE_SERIAL.printf("[HTTP] GET... code: %d\n", httpCode);
        // httpCode will be negative on error
        tim64 = esp_timer_get_time() - tim64;
        tim64 /= 1000;
        uint32_t tim32 = tim64 & 0xFFFFFFFF;
        if(httpCode > 0) {
            // HTTP header has been send and Server response header has been handled
            USE_SERIAL.printf("[HTTP] POST... code: %d in %d ms\n", httpCode, tim32);
        }
        else{
          //http.setTimeout(250);
          //http.setConnectTimeout(250);
          //USE_SERIAL.print("[HTTP] POST ...\n");
          //int httpCode = http.POST((uint8_t *)(mic_http), 33280);
          USE_SERIAL.printf("[HTTP] POST timeout: %d\n", tim32);
        }
        //USE_SERIAL.printf("Sent ...");
        //http.end();
    }
    else{
        USE_SERIAL.print("Exiting HTTP ....\n");
    }
    //xSemaphoreGive(rssiHttpSemaphore);
  }
}


// http functions
void dummyTask(void * parameter){
    timer0 = timerBegin(0, 40, true);
    timerAttachInterrupt(timer0, &onTimerRSSI, true);
    timerAlarmWrite(timer0, 125, true);
    timerAlarmEnable(timer0);

    // initializing the audio frame
    mic_data[0] = countAudioFrame;

    vTaskSuspend(NULL);
}

void loop(){
    //vTaskSuspend(NULL);
    delay(10000);
}
