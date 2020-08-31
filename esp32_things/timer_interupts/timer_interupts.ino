/* Read sensors, timestamp them and send over to the internet*/

// includes
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <soc/rtc.h>
#include <ESP.h>

// definitions
#define USE_SERIAL Serial
#define RTC_CNTL_TIME_UPDATE_REG 0x3FF4800C
#define  RTC_CNTL_CLK_CONF_REG 0x3FF48070
#define mask 0x3FFFFFFF
#define newVal 0x40000000
#define updat 0x80000000

// Global variables
// wifi connection handle
WiFiMulti wifiMulti;
// defining interrupt pin for GPIO
int intPin = 0;     // setting to onboard button for now, can use pin 36

// data write count
volatile int countGpio = 0;
volatile int countRSSI = 0;
volatile int countMotion = 0;
volatile int countMicrophone = 0;
volatile int countIMU = 0;

// data buffers
String *timestamp_data = (String *) malloc(8000);
String *rssi_data = (String *) malloc(12000);
String *motion_data = (String *) malloc(10000);
String *microphone_data = (String *) malloc(40000);
String *imu_data = (String *) malloc(12000);

// define timers
hw_timer_t * timer0 = NULL;
hw_timer_t * timer1 = NULL;
hw_timer_t * timer2 = NULL;
hw_timer_t * timer3 = NULL;

// defining semaphores
// get semaphores
SemaphoreHandle_t gpioSemaphore = xSemaphoreCreateBinary();
SemaphoreHandle_t rssiSemaphore = xSemaphoreCreateBinary();
SemaphoreHandle_t motionSemaphore = xSemaphoreCreateBinary();
SemaphoreHandle_t microphoneSemaphore = xSemaphoreCreateBinary();
SemaphoreHandle_t imuSemaphore = xSemaphoreCreateBinary();


// defining task handles
// interrupt tasks for core 0
TaskHandle_t intGpio;
TaskHandle_t intRSSI;
TaskHandle_t intMotion;
TaskHandle_t intMicrophone;
TaskHandle_t intIMU;

// http tasks for posting data
TaskHandle_t httpRSSI;
TaskHandle_t httpMotion;
TaskHandle_t httpMicrophone;
TaskHandle_t httpIMU;

// interrupt service routines
void IRAM_ATTR isrGpio() {

    USE_SERIAL.printf("INSIDE GPIO interrupt\n");
    // getting timestamp
    uint64_t tim64 = rtc_time_get();
    uint32_t tim32 = tim64 & 0xFFFFFFFF;

    countGpio++;
    USE_SERIAL.println(*timestamp_data);
    *timestamp_data += ","+String(tim32)+",";
    USE_SERIAL.println(*timestamp_data);
    if (countGpio > 0 && countGpio%1 == 0 ){
        USE_SERIAL.printf("Giving up semaphore.\n");
        xSemaphoreGiveFromISR(gpioSemaphore, NULL);
    }
}

void IRAM_ATTR onTimerRSSI() {

    USE_SERIAL.printf("INSIDE RSSI interrupt\n");
    // getting timestamp
    /*uint64_t tim64 = rtc_time_get();
    uint32_t tim32 = tim64 & 0xFFFFFFFF;

    //countRSSI++;
    USE_SERIAL.println(*rssi_data);
    *rssi_data += ","+String(tim32)+",";
    USE_SERIAL.println(*rssi_data);

    // Get RSSI
    //long rssi = WiFi.RSSI();*/
    countRSSI++;
    if (countRSSI > 0 && countRSSI%10 == 0 ){
        USE_SERIAL.printf("Giving up semaphore.\n");
        xSemaphoreGiveFromISR(rssiSemaphore, NULL);
    }
}

void IRAM_ATTR onTimerMotion() {

    USE_SERIAL.printf("INSIDE Motion interrupt\n");
    // getting timestamp
    /*uint64_t tim64 = rtc_time_get();
    uint32_t tim32 = tim64 & 0xFFFFFFFF;

    countMotion++;
    //USE_SERIAL.println(*motion_data);
    //*motion_data += ","+String(tim32)+",";
    //USE_SERIAL.println(*motion_data);

    // Get RSSI
    //Read Motion Sensor Data
    USE_SERIAL.println("Hey there ...");*/
    countMotion++;
    if (countMotion > 0 && countMotion%10 == 0 ){
        USE_SERIAL.printf("Giving up semaphore.\n");
        xSemaphoreGiveFromISR(motionSemaphore, NULL);
    }
}

void IRAM_ATTR onTimerMicrophone() {

    USE_SERIAL.printf("INSIDE Microphone interrupt\n");
    // getting timestamp
    /*uint64_t tim64 = rtc_time_get();
    uint32_t tim32 = tim64 & 0xFFFFFFFF;

    
    USE_SERIAL.println(*microphone_data);
    *microphone_data += ","+String(tim32)+",";
    USE_SERIAL.println(*microphone_data);

    // Get RSSI
    /* Get microphone data*/
    countMicrophone++;
    if (countMicrophone > 0 && countMicrophone%40 == 0 ){
        USE_SERIAL.printf("Giving up semaphore.\n");
        xSemaphoreGiveFromISR(microphoneSemaphore, NULL);
    }
}

void IRAM_ATTR onTimerIMU() {

    USE_SERIAL.printf("INSIDE IMU interrupt\n");
    // getting timestamp
    /*uint64_t tim64 = rtc_time_get();
    uint32_t tim32 = tim64 & 0xFFFFFFFF;

    USE_SERIAL.println(*imu_data);
    *imu_data += ","+String(tim32)+",";
    USE_SERIAL.println(*imu_data);

    // Get RSSI
    /* Get IMU data*/
    countIMU++;
    if (countIMU > 0 && countIMU%10 == 0 ){
        USE_SERIAL.printf("Giving up semaphore.\n");
        xSemaphoreGiveFromISR(imuSemaphore, NULL);
    }
}

// Task functions
// intGpio task code, to attach interrupt to core zero
void gpioTask(void * parameter){

    // attaching interrupt
    pinMode(intPin, INPUT_PULLUP);
    attachInterrupt(intPin, isrGpio, RISING);
    vTaskSuspend(NULL);
}

// intRSSI task code, to attach interrupt to core zero
void rssiTask(void * parameter){

    // RSSI Timer
    timer0 = timerBegin(0, 80, true);
    timerAttachInterrupt(timer0, &onTimerRSSI, true);
    timerAlarmWrite(timer0, 100000, true);
    timerAlarmEnable(timer0);
    
    // attaching interrupt
    /*service interrupt here or on core one
     Or*/
     vTaskSuspend(NULL);
}

// intMotion task code, to attach interrupt to core zero
void motionTask(void * parameter){
    // Motion Timer
    timer1 = timerBegin(1, 80, true);
    timerAttachInterrupt(timer1, &onTimerMotion, true);
    timerAlarmWrite(timer1, 100000, true);
    timerAlarmEnable(timer1);
    
    // attaching interrupt
    /*service interrupt here or on core one
     Or*/
    vTaskSuspend(NULL);
}

// intMicrophone task code, to attach interrupt to core zero
void microphoneTask(void * parameter){
    // Microphone Timer
    timer2 = timerBegin(2, 5, true);
    timerAttachInterrupt(timer2, &onTimerMicrophone, true);
    timerAlarmWrite(timer2, 100000, true);
    timerAlarmEnable(timer2);
    
    // attaching interrupt
    /*service interrupt here or on core one
     Or*/
    vTaskSuspend(NULL);
}

// intIMU task code, to attach interrupt to core zero
void imuTask(void * parameter){
    // Motion Timer
    timer3 = timerBegin(3, 80, true);
    timerAttachInterrupt(timer3, &onTimerIMU, true);
    timerAlarmWrite(timer3, 100000, true);
    timerAlarmEnable(timer3);
    
    // attaching interrupt
    /*service interrupt here or on core one
     Or*/
    vTaskSuspend(NULL);
}


void setup() {
 
  Serial.begin(115200);

  // getting wifi connection
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

  // enabling timestamping
  uint32_t rtc_src = *((volatile uint32_t *) RTC_CNTL_CLK_CONF_REG);
  *((volatile uint32_t *) RTC_CNTL_CLK_CONF_REG) = (rtc_src & mask) | newVal;
  rtc_clk_32k_enable_external();

  // initializing tasks
  /*xTaskCreatePinnedToCore(gpioTask,   // function to implement task
                            "intGpio",      // Task name
                            1000,           // stack size in words
                            NULL,           // arguments
                            0,              // priority
                            &intGpio,       // task handle
                            0);
  xTaskCreatePinnedToCore(rssiTask,   // function to implement task
                            "intRSSI",      // Task name
                            1000,           // stack size in words
                            NULL,           // arguments
                            0,              // priority
                            &intRSSI,       // task handle
                            0);
  xTaskCreatePinnedToCore(motionTask,   // function to implement task
                            "intMotion",      // Task name
                            1000,           // stack size in words
                            NULL,           // arguments
                            0,              // priority
                            &intMotion,       // task handle
                            0);*/
  xTaskCreatePinnedToCore(microphoneTask,   // function to implement task
                            "intMicrophone",      // Task name
                            1000,           // stack size in words
                            NULL,           // arguments
                            0,              // priority
                            &intMicrophone,       // task handle
                            0);
  /*xTaskCreatePinnedToCore(imuTask,   // function to implement task
                            "intIMU",      // Task name
                            1000,           // stack size in words
                            NULL,           // arguments
                            0,              // priority
                            &intIMU,       // task handle
                            0);
    xTaskCreatePinnedToCore(httpRssiTask,   // function to implement task
                            "httpRSSI",      // Task name
                            10000,           // stack size in words
                            NULL,           // arguments
                            1,              // priority
                            &httpRSSI,       // task handle
                            1);
  xTaskCreatePinnedToCore(httpMotionTask,   // function to implement task
                            "httpMotion",      // Task name
                            10000,           // stack size in words
                            NULL,           // arguments
                            1,              // priority
                            &httpMotion,       // task handle
                            1);*/
  xTaskCreatePinnedToCore(httpMicrophoneTask,   // function to implement task
                            "httpMicrophone",      // Task name
                            60000,           // stack size in words
                            NULL,           // arguments
                            1,              // priority
                            &httpMicrophone,       // task handle
                            1);
  /*xTaskCreatePinnedToCore(httpImuTask,   // function to implement task
                            "httpIMU",      // Task name
                            10000,           // stack size in words
                            NULL,           // arguments
                            1,              // priority
                            &httpIMU,       // task handle
                            1);*/
}

// http functions
void httpRssiTask(void * parameter){

  while(true){ 
    xSemaphoreTake(rssiSemaphore, portMAX_DELAY);
    USE_SERIAL.printf("RSSI triggered by interrupt: ");
    USE_SERIAL.println(countRSSI);
    
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
        String data = "length="+*rssi_data;
        USE_SERIAL.println(data);
        int httpCode = http.POST(data);
        // resetting data buffer
        countRSSI=0;
        free(rssi_data);
        rssi_data = (String *) malloc(10240);
        //free(cand);

        // httpCode will be negative on error
        if(httpCode > 0) {
            // HTTP header has been send and Server response header has been handled
            USE_SERIAL.printf("[HTTP] GET... code: %d\n", httpCode);
        }

        http.end();
    }
  }
}

// http functions
void httpMotionTask(void * parameter){

  while(true){
    xSemaphoreTake(motionSemaphore, portMAX_DELAY);
    USE_SERIAL.printf("Motion triggered by interrupt: ");
    USE_SERIAL.println(countMotion);
    
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
        String data = "length="+*motion_data;
        USE_SERIAL.println(data);
        int httpCode = http.POST(data);
        // resetting data buffer
        countMotion=0;
        free(motion_data);
        motion_data = (String *) malloc(10240);
        //free(cand);

        // httpCode will be negative on error
        if(httpCode > 0) {
            // HTTP header has been send and Server response header has been handled
            USE_SERIAL.printf("[HTTP] GET... code: %d\n", httpCode);
        }

        http.end();
    }
  }
}

// http functions
void httpMicrophoneTask(void * parameter){
  while(true){
    xSemaphoreTake(microphoneSemaphore, portMAX_DELAY);
    USE_SERIAL.printf("Microphone triggered by interrupt: ");
    USE_SERIAL.println(countMicrophone);
    
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
        http.setTimeout(100);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        //String* cand = (String *) malloc(4096);
        //*cand = "length=";
        //for (int i=0; i<4079; i++){
        //    *cand += 'a';  
        //}
        //USE_SERIAL.printf("another_string=%s \n", (*cand).c_str());
        String data = "length="+*microphone_data;
        //USE_SERIAL.println(data);
        int httpCode = http.POST(data);
        // resetting data buffer
        countMicrophone=0;
        //free(microphone_data);
        //microphone_data = (String *) malloc(10240);
        //free(cand);

        // httpCode will be negative on error
        if(httpCode > 0) {
            // HTTP header has been send and Server response header has been handled
            USE_SERIAL.printf("[HTTP] GET... code: %d\n", httpCode);
        }

        http.end();
    }
  }
}

// http functions
void httpImuTask(void * parameter){
  while(true){
    xSemaphoreTake(imuSemaphore, portMAX_DELAY);
    USE_SERIAL.printf("IMU triggered by interrupt: ");
    USE_SERIAL.println(countIMU);
    
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
        String data = "length="+*imu_data;
        USE_SERIAL.println(data);
        int httpCode = http.POST(data);
        // resetting data buffer
        countIMU=0;
        free(imu_data);
        imu_data = (String *) malloc(10240);
        //free(cand);

        // httpCode will be negative on error
        if(httpCode > 0) {
            // HTTP header has been send and Server response header has been handled
            USE_SERIAL.printf("[HTTP] GET... code: %d\n", httpCode);
        }

        http.end();
    }
  }
}

// http functions
void loop(){

    xSemaphoreTake(gpioSemaphore, portMAX_DELAY);
    USE_SERIAL.printf("GPIO triggered by interrupt: ");
    USE_SERIAL.println(countGpio);
    
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
        countGpio=0;
        free(timestamp_data);
        timestamp_data = (String *) malloc(10240);
        //free(cand);

        // httpCode will be negative on error
        if(httpCode > 0) {
            // HTTP header has been send and Server response header has been handled
            USE_SERIAL.printf("[HTTP] GET... code: %d\n", httpCode);
        }

        http.end();
    }
}
