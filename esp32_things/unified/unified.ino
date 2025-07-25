/* Read sensors, timestamp them and send over to the internet*/

// includes
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <ESP.h>
#include <esp_task_wdt.h>
#include <SparkFunMPU9250-DMP.h>
#include <driver/adc.h>

/*Defining macros to exclude sensing modalities*/
#define EXCLUDE_MOTION
#define EXCLUDE_RSSI
#define EXCLUDE_AUDIO
#define EXCLUDE_IMU   // accelerometer and gyroscope only

// put sensors to be included here
//#undef EXCLUDE_AUDIO
//#undef EXCLUDE_IMU
#undef EXCLUDE_MOTION
//#undef EXCLUDE_RSSI
   
// definitions
#define USE_SERIAL Serial
#define MIC_DATA 2
#define MOT_DATA 3
#define RSSI_DATA 9
#define IMU_DATA 10

// Global variables
// wifi connection handle
WiFiMulti wifiMulti;
const uint16_t port = 8090;
//old setting
//const char * host = "10.42.0.1";
const char * host = "192.168.1.254";

#ifndef EXCLUDE_AUDIO
  //Mic struct
  // microphone data
  WiFiClient clientMic;
  uint64_t baseTimeMic;
  uint8_t countAudioFrame=0;
  uint16_t countMic=0;
  uint16_t sub_count=0;
  uint32_t temp_audio=0;
  uint16_t mic_data[16640];
  unsigned char mic_http[33280];
#endif

#ifndef EXCLUDE_MOTION
  //Motion struct
  // motion data
  WiFiClient clientMotion;
  uint64_t baseTimeMotion;
  uint8_t countMotionFrame=0;
  uint16_t countMotion=0;
  uint16_t motion_data[4160];
  unsigned char motion_http[8320];
#endif

#ifndef EXCLUDE_IMU
  //IMU struct
  // motion data
  MPU9250_DMP imu;
  WiFiClient clientImu;
  uint8_t countImuFrame=0;
  uint16_t countImu=0;
  uint16_t imu_data[20000];
  unsigned char imu_http[40001];
  //uint16_t imu_data[4000];
  //unsigned char imu_http[8001];
#endif

#ifndef EXCLUDE_RSSI
  //RSSI struct
  // motion data
  wifi_ap_record_t wifidata;
  WiFiClient clientRssi;
  uint8_t countRssiFrame=0;
  uint16_t countRssi=0;
  uint8_t rssi_data[16384];
  unsigned char rssi_http[16384];
#endif

// define timers
hw_timer_t* timer0 = NULL;
hw_timer_t* timer1 = NULL;
hw_timer_t* timer2 = NULL;
hw_timer_t* timer3 = NULL;
// defining semaphores
// for notifications handling
BaseType_t pxMicTaskWoken;
BaseType_t pxMotionTaskWoken;
BaseType_t pxRssiTaskWoken;
BaseType_t pxImuTaskWoken;

// defining task handles
// interrupt tasks for core 0
TaskHandle_t intMic;
TaskHandle_t intMotion;
TaskHandle_t intRssi;
TaskHandle_t intImu;

// http tasks for posting data
TaskHandle_t httpMic;
TaskHandle_t httpMotion;
TaskHandle_t httpRssi;
TaskHandle_t httpImu;

#ifndef EXCLUDE_AUDIO
  void IRAM_ATTR onTimerMic() {    

    uint16_t audio;
    uint64_t tim64;

    /*if(sub_count < 24){
      //temp_audio += analogRead(A6);
      temp_audio += adc1_get_raw(ADC1_CHANNEL_6);
      sub_count += 1;
    }*/

    //else{

      //temp_audio += analogRead(A6);
      //temp_audio += adc1_get_raw(ADC1_CHANNEL_6);
      audio = adc1_get_raw(ADC1_CHANNEL_6);
      tim64 = esp_timer_get_time();
      //sub_count += 1;
      //audio = temp_audio/(sub_count+1);
      //sub_count = 0;
      //temp_audio = 0;
 
      // time stamp
      // uint64_t tim64 = esp_timer_get_time();
    
      // copy data
      if(countMic%256 == 0){
        baseTimeMic = tim64;
        //if(countMic%256 <= 32){
        mic_data[(countMic/256)*3+MIC_DATA*countMic] = audio;
        mic_data[(countMic/256)*3+MIC_DATA*countMic+4] = tim64 & 0xFFFF;
        mic_data[(countMic/256)*3+MIC_DATA*countMic+3] = (tim64>>16) & 0xFFFF;
        mic_data[(countMic/256)*3+MIC_DATA*countMic+2] = (tim64>>32) & 0xFFFF;
        mic_data[(countMic/256)*3+MIC_DATA*countMic+1] = (tim64>>48) & 0xFFFF;
        //}

      }else{
        tim64 = baseTimeMic - tim64;
        //USE_SERIAL.printf("Audio: %d ", countMic%256);
        //if(countMic%256 <= 32){
        mic_data[((countMic/256)+1)*3+MIC_DATA*countMic] = audio;
        //USE_SERIAL.printf("Audio here: %d\n", mic_data[((countMic%256)+1)*3+MIC_DATA*countMic]);
        mic_data[((countMic/256)+1)*3+MIC_DATA*countMic+1] = (tim64) & 0xFFFF;
        //}
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
    //}
  }
#endif

#ifndef EXCLUDE_MOTION
  void IRAM_ATTR onTimerMotion() {    

    // mic data
    //uint16_t motion = analogRead(A7);
    uint16_t motion = adc1_get_raw(ADC1_CHANNEL_7);
    // time stamp
    uint64_t tim64 = esp_timer_get_time();
    
    // copy data
    if(countMotion%256 == 0){
      baseTimeMotion = tim64;
      //if(countMotion%256 <= 8){
        motion_data[(countMotion/256)*2+MOT_DATA*countMotion] = motion;
        motion_data[(countMotion/256)*2+MOT_DATA*countMotion+4] = tim64 & 0xFFFF;
        motion_data[(countMotion/256)*2+MOT_DATA*countMotion+3] = (tim64>>16) & 0xFFFF;
        motion_data[(countMotion/256)*2+MOT_DATA*countMotion+2] = (tim64>>32) & 0xFFFF;
        motion_data[(countMotion/256)*2+MOT_DATA*countMotion+1] = (tim64>>48) & 0xFFFF;
      //}

    }else{
      tim64 = baseTimeMotion - tim64;
      //if(countMotion%256 <= 8){
        motion_data[((countMotion/256)+1)*2+MOT_DATA*countMotion] = motion;
        motion_data[((countMotion/256)+1)*2+MOT_DATA*countMotion+2] = (tim64) & 0xFFFF;
        motion_data[((countMotion/256)+1)*2+MOT_DATA*countMotion+1] = (tim64>>16) & 0xFFFF;
      //}
    }
    countMotion ++;

    //if(countMotion%200 == 0)
    //    USE_SERIAL.printf("Motion: %d\n", motion);
    //if(motion >= 670)
    //    USE_SERIAL.printf("Motion: %d\n", motion);
    if(countMotion%500 == 0){
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
#endif

#ifndef EXCLUDE_RSSI
  void IRAM_ATTR onTimerRssi(){

        pxRssiTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(httpRssi, &pxRssiTaskWoken);
        if (pxRssiTaskWoken == pdTRUE){
            portYIELD_FROM_ISR();
    }
  }
#endif

#ifndef EXCLUDE_IMU
  void IRAM_ATTR onTimerImu() {    

    // IMU data
    /*uint64_t tim64;
    if (imu.dataReady()){
        imu.update();
        tim64 = esp_timer_get_time();
        imu_data[IMU_DATA*countImu] = imu.ax;
        imu_data[IMU_DATA*countImu+1] = imu.ay;
        imu_data[IMU_DATA*countImu+2] = imu.az;
        imu_data[IMU_DATA*countImu+3] = imu.gx;
        imu_data[IMU_DATA*countImu+4] = imu.gy;
        imu_data[IMU_DATA*countImu+5] = imu.gz;
    }else{
        USE_SERIAL.printf("Values were not available ... %d\n", countImu);
        memset(&(imu_data[IMU_DATA*countImu]), 0, 12);
        tim64 = esp_timer_get_time();
    }

    imu_data[IMU_DATA*countImu+9] = tim64 & 0xFFFF;
    imu_data[IMU_DATA*countImu+8] = (tim64>>16) & 0xFFFF;
    imu_data[IMU_DATA*countImu+7] = (tim64>>32) & 0xFFFF;
    imu_data[IMU_DATA*countImu+6] = (tim64>>48) & 0xFFFF;
    
    countImu ++;
    if(countImu%100 == 0){
        //USE_SERIAL.println("Fingerprint ...");
        //float test = imu.calcAccel(imu.ax);
        //USE_SERIAL.println("Fingerprint ...");
        //USE_SERIAL.println(test);
        USE_SERIAL.printf("IMU: %d, %d, %d\n", imu.ax, imu.ay, imu.az);
    }
    
    if(countImu%2000 == 0){
        USE_SERIAL.printf("IMU: %d, %d, %d\n", imu.ax, imu.ay, imu.az);
        //USE_SERIAL.printf("IMU: %d\n", countImu);
        memcpy(imu_http, imu_data, 40000);        
        imu_http[40000] = countImuFrame;
        countImu = 0;
        if(countImuFrame == 255){
            countImuFrame = 0;  
        }else{
          countImuFrame ++;
        }*/
        
        pxImuTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(httpImu, &pxImuTaskWoken);
        if (pxImuTaskWoken == pdTRUE){
            portYIELD_FROM_ISR();
        }
    //}
}
#endif

void setup(){
 
  Serial.begin(115200);
  
  // getting wifi connection
  for(uint8_t t = 4; t > 0; t--) {
      USE_SERIAL.printf("[SETUP] WAIT %d...\n", t);
      USE_SERIAL.flush();
      delay(1000);
  }

  // limiting wifi transmission power
  // esp_wifi_set_max_tx_power(56);

  //DO NOT TOUCH
  //  This is here to force the ESP32 to reset the WiFi and initialise correctly.
  Serial.print("WIFI status = ");
  Serial.println(WiFi.getMode());
  WiFi.disconnect(true);
  delay(1000);
  WiFi.softAPdisconnect(true);           // disconnects AP Mode 
  delay(1000);
  WiFi.mode(WIFI_STA);
  delay(1000);
  Serial.print("WIFI status = ");
  Serial.println(WiFi.getMode());
  // End silly stuff !!!

  //old settings
  //wifiMulti.addAP("experiments", "inception");
  wifiMulti.addAP("NETGEAR46", "politehippo981");
  // wait for connection here
  USE_SERIAL.printf("Waiting for wifi connection ...\n");
  while(wifiMulti.run() != WL_CONNECTED);
  USE_SERIAL.printf("Connected to wifi successfully ...\n");

  // configuring ADC
  adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);    // setting adc1 channel 6 @ atten 11dB
  adc1_config_width(ADC_WIDTH_BIT_9);                           // setting adc1 width @ 12 bits
  adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_11);    // setting adc1 channel 6 @ atten 11dB
  adc1_config_width(ADC_WIDTH_BIT_12);                           // setting adc1 width @ 12 bits

  #ifndef EXCLUDE_IMU
    // setting up IMU
    while (imu.begin() != INV_SUCCESS)
    {
        USE_SERIAL.println("Unable to communicate with MPU-9250");
        delay(1000);
    }
    USE_SERIAL.printf("Connected to MPU-9250\n");
    imu.setSensors(INV_XYZ_GYRO | INV_XYZ_ACCEL);
    imu.setGyroFSR(2000); // Set gyro to 2000 dps
    imu.setAccelFSR(8); // Set accel to +/-2g
    imu.setLPF(188); // Set LPF corner frequency to 188Hz
    imu.setSampleRate(1000);
    // creating socket connections for streaming
    IPAddress ip = clientImu.localIP();  
    while (!clientImu.connect(host, port)) {
        if((WiFi.status() == WL_CONNECTED)){
        USE_SERIAL.print("Connection to host failed from IMU: ");
        USE_SERIAL.println(WiFi.macAddress());
        USE_SERIAL.println(ip);}
        else{
          USE_SERIAL.printf("Fuck not even connected to the WiFi\n");  
        }
    }
  #endif

  #ifndef EXCLUDE_RSSI
    while (!clientRssi.connect(host, port)) {
        USE_SERIAL.println("Connection to host failed from RSSI");
    }
  #endif

  #ifndef EXCLUDE_MOTION
    // creating socket connections for streaming
    while (!clientMotion.connect(host, port)) {
        USE_SERIAL.println("Connection to host failed from MOTION");
        if(WiFi.status() != WL_CONNECTED){
          USE_SERIAL.println(" because Wifi not connected.");        
        }
    }
  #endif

  #ifndef EXCLUDE_AUDIO
  while (!clientMic.connect(host, port)) {
        USE_SERIAL.println("Connection to host failed from AUDIO");
        delay(500);
  }
  #endif

  #ifndef EXCLUDE_MOTION
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
  #endif
  #ifndef EXCLUDE_AUDIO
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
  #endif
  #ifndef EXCLUDE_RSSI
  delay(1000);
  xTaskCreatePinnedToCore(httpRssiTask,   // function to implement task
                            "httpRssi",      // Task name
                            4096,         // stack size in words
                            NULL,           // arguments
                            2,              // priority
                            &httpRssi,       // task handle
                            0);
  delay(1000);
  xTaskCreatePinnedToCore(intRssiTask,   // function to implement task
                            "intRssi",      // Task name
                            4096,           // stack size in words
                            NULL,           // arguments
                            1,              // priority
                            &intRssi,       // task handle
                            1);
  #endif
  #ifndef EXCLUDE_IMU
  delay(1000);
  xTaskCreatePinnedToCore(httpImuTask,   // function to implement task
                            "httpImu",      // Task name
                            4096,         // stack size in words
                            NULL,           // arguments
                            2,              // priority
                            &httpImu,       // task handle
                            0);
  delay(1000);
  xTaskCreatePinnedToCore(intImuTask,   // function to implement task
                            "intImu",      // Task name
                            4096,           // stack size in words
                            NULL,           // arguments
                            1,              // priority
                            &intImu,       // task handle
                            1);
  #endif       
}

#ifndef EXCLUDE_AUDIO
  // http functions
  void httpMicTask(void * parameter){

    USE_SERIAL.printf("Timer initialized ...\n");
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
#endif

#ifndef EXCLUDE_MOTION
  void httpMotionTask(void * parameter){

    USE_SERIAL.printf("Timer initialized ...\n");
    clientMotion.print(WiFi.macAddress());
    clientMotion.print("MOT");
    USE_SERIAL.println("Connected to server successful!");

    for(;;){

      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      USE_SERIAL.printf("MOTIOn triggered by interrupt: %d\n", countMotionFrame);
    
      // wait for WiFi connection
      if((WiFi.status() == WL_CONNECTED)) {
      
        uint64_t tim64 = esp_timer_get_time();
        USE_SERIAL.printf("Wifi Status: %d\n", (WiFi.status() == WL_CONNECTED));
        int httpCode = clientMotion.write((uint8_t *)(motion_http), 8320);
        USE_SERIAL.printf("Wifi Status: %d\n", (WiFi.status() == WL_CONNECTED));
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
#endif

#ifndef EXCLUDE_AUDIO
  // interrupt task functions
  void intMicTask(void * parameter){
    timer0 = timerBegin(0, 40, true);
    timerAttachInterrupt(timer0, &onTimerMic, true);
    timerAlarmWrite(timer0, 250, true);
    timerAlarmEnable(timer0);

    vTaskSuspend(NULL);
  }
#endif

#ifndef EXCLUDE_MOTION
  void intMotionTask(void * parameter){
    timer1 = timerBegin(1, 80, true);
    timerAttachInterrupt(timer1, &onTimerMotion, true);
    timerAlarmWrite(timer1, 1000, true);
    timerAlarmEnable(timer1);

    vTaskSuspend(NULL);
  }
#endif

#ifndef EXCLUDE_IMU
  // http functions
  void httpImuTask(void * parameter){

    USE_SERIAL.printf("Timer initialized ...\n");
    clientImu.print(WiFi.macAddress());
    clientImu.print("IMU");
    clientImu.setTimeout(1);
    USE_SERIAL.println("Connected to server successful!");

    for(;;){ 

      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

      uint64_t tim64;
      if (imu.dataReady()){
        imu.update();
        tim64 = esp_timer_get_time();
        imu_data[IMU_DATA*countImu] = imu.ax;
        imu_data[IMU_DATA*countImu+1] = imu.ay;
        imu_data[IMU_DATA*countImu+2] = imu.az;
        imu_data[IMU_DATA*countImu+3] = imu.gx;
        imu_data[IMU_DATA*countImu+4] = imu.gy;
        imu_data[IMU_DATA*countImu+5] = imu.gz;
      }else{
        USE_SERIAL.printf("Values were not available ... %d\n", countImu);
        memset(&(imu_data[IMU_DATA*countImu]), 0, 12);
        tim64 = esp_timer_get_time();
      }

      imu_data[IMU_DATA*countImu+9] = tim64 & 0xFFFF;
      imu_data[IMU_DATA*countImu+8] = (tim64>>16) & 0xFFFF;
      imu_data[IMU_DATA*countImu+7] = (tim64>>32) & 0xFFFF;
      imu_data[IMU_DATA*countImu+6] = (tim64>>48) & 0xFFFF;
    
      countImu ++;
    
      if(countImu%2000 == 0){
        USE_SERIAL.printf("IMU: %d, %d, %d\n", imu.ax, imu.ay, imu.az);
        memcpy(imu_http, imu_data, 40000);        
        imu_http[40000] = countImuFrame;
        countImu = 0;
        if(countImuFrame == 255){
            countImuFrame = 0;  
        }else{
          countImuFrame ++;
        }
                                  
        USE_SERIAL.printf("IMU triggered by interrupt: %d\n", countImuFrame);
    
        // wait for WiFi connection
        if((WiFi.status() == WL_CONNECTED)) {
      
          //uint64_t tim64 = esp_timer_get_time();
          tim64 = esp_timer_get_time();
          int httpCode = clientImu.write((uint8_t *)(imu_http), 40001);
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
      }
      portYIELD();
    }
  }
#endif

#ifndef EXCLUDE_RSSI
  void httpRssiTask(void * parameter){

    USE_SERIAL.printf("Timer initialized ...\n");

    clientRssi.print(WiFi.macAddress());
    clientRssi.print("RSSI");
    USE_SERIAL.println("Connected to server successful!");

    for(;;){

      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      esp_wifi_sta_get_ap_info(&wifidata);
      uint64_t rssiTimestamp = esp_timer_get_time();
      rssi_data[RSSI_DATA*countRssi] = wifidata.rssi;
      //rssi_data[RSSI_DATA*countRssi+7] = rssiTimestamp & 0xFF;
      //rssi_data[RSSI_DATA*countRssi+5] = (rssiTimestamp>>8) & 0xFF;
      //rssi_data[RSSI_DATA*countRssi+3] = (rssiTimestamp>>16) & 0xFF;
      //rssi_data[RSSI_DATA*countRssi+1] = (rssiTimestamp>>32) & 0xFF;
      memcpy(&(rssi_data[RSSI_DATA*countRssi+1]), &rssiTimestamp, 8);

      //if(countRssi%500 == 0){
      //  USE_SERIAL.printf("RSSi %d\n", wifidata.rssi);
      //}
      countRssi ++;

      if(countRssi%1820 == 0){
        USE_SERIAL.printf("Sending Frame Rssi: %d\n", countRssiFrame);
        memcpy(rssi_http, rssi_data, 16384);
        rssi_http[16383] = countRssiFrame;
        countRssi = 0;
        if(countRssiFrame == 255){
            countRssiFrame = 0;  
        }else{
          countRssiFrame ++;
        }

        // wait for WiFi connection
        if((WiFi.status() == WL_CONNECTED)) {
      
         // uint64_t tim64 = esp_timer_get_time();
          int httpCode = clientRssi.write((uint8_t *)(rssi_http), 16384);
          // httpCode will be negative on error
          //tim64 = esp_timer_get_time() - tim64;
          //tim64 /= 1000;
          //uint32_t tim32 = tim64 & 0xFFFFFFFF;
          if(httpCode > 0) {
             USE_SERIAL.printf("[HTTP] POST... code: %d\n", httpCode);
          }
          else{
              USE_SERIAL.printf("[HTTP] POST timeout.\n");
          }
          USE_SERIAL.printf("Sent ...\n");
        }
        else{
          USE_SERIAL.print("Exiting HTTP ....\n");
        }
      }
      portYIELD();
    }
  }
#endif

#ifndef EXCLUDE_IMU
  // interrupt task functions
  void intImuTask(void * parameter){
    timer2 = timerBegin(2, 80, true);
    timerAttachInterrupt(timer2, &onTimerImu, true);
    timerAlarmWrite(timer2, 4000, true);
    timerAlarmEnable(timer2);

    vTaskSuspend(NULL);
  }
#endif

#ifndef EXCLUDE_RSSI
  void intRssiTask(void * parameter){
    timer3 = timerBegin(3, 80, true);
    timerAttachInterrupt(timer3, &onTimerRssi, true);
    timerAlarmWrite(timer3, 2000, true);
    timerAlarmEnable(timer3);

    vTaskSuspend(NULL);
  }
#endif

void loop(){
    delay(10000);
}
