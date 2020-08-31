/* Read sensors, timestamp them and send over to the internet*/

// includes
#include <WiFi.h>
#include <WiFiMulti.h>
//#include "esp_wifi.h"
#include <SparkFunMPU9250-DMP.h>

// definitions
#define USE_SERIAL Serial
#define RSSI_DATA 5
#define IMU_DATA 10

// Global Variables
WiFiMulti wifiMulti;
wifi_ap_record_t wifidata;
WiFiClient clientRSSI;
MPU9250_DMP imu;
const uint16_t port = 8092;
const char * host = "10.42.0.1";
//const char * host = "192.168.137.133";

int curChannel = 1;
int8_t rssi;
uint32_t rssiTimestamp;

// for RSSI
// portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// wifi promiscous mode callbacks
const wifi_promiscuous_filter_t filt={ //Idk what this does
    .filter_mask=WIFI_PROMIS_FILTER_MASK_MGMT|WIFI_PROMIS_FILTER_MASK_DATA
};
// update rssi and timestamp
void sniffer(void* buf, wifi_promiscuous_pkt_type_t type) { 
  // Serial.printf("Received a packet\n");
  wifi_promiscuous_pkt_t *p = (wifi_promiscuous_pkt_t*)buf;
  rssi = p->rx_ctrl.rssi;
  rssiTimestamp = p->rx_ctrl.timestamp;
}

//Mic struct
// rssi data
WiFiClient clientRssi;
uint8_t countRssiFrame=0;
uint16_t countRssi=0;
uint8_t rssi_data[16384];
unsigned char rssi_http[16384];

//Motion struct
// imu data
WiFiClient clientImu;
uint8_t countImuFrame=0;
uint16_t countImu=0;
uint16_t imu_data[10000];
unsigned char imu_http[20001];

// define timers
hw_timer_t* timer2 = NULL;
hw_timer_t* timer3 = NULL;

// defining semaphores
// for notifications handling
BaseType_t pxRssiTaskWoken;
BaseType_t pxImuTaskWoken;

// defining task handles
// interrupt tasks for core 0
TaskHandle_t intRssi;
TaskHandle_t intImu;

// http tasks for posting data
TaskHandle_t httpRssi;
TaskHandle_t httpImu;


void IRAM_ATTR onTimerRssi(){
    
    // copy data
    //esp_wifi_sta_get_ap_info(&wifidata);
    /*rssi_data[RSSI_DATA*countRssi] = rssi;
    rssi_data[RSSI_DATA*countRssi+4] = rssiTimestamp & 0xFF;
    rssi_data[RSSI_DATA*countRssi+3] = (rssiTimestamp>>8) & 0xFF;
    rssi_data[RSSI_DATA*countRssi+2] = (rssiTimestamp>>8) & 0xFF;
    rssi_data[RSSI_DATA*countRssi+1] = (rssiTimestamp>>8) & 0xFF;
  
    countRssi ++;

    if(countRssi%3276 == 0){
        USE_SERIAL.printf("Rssi: %d\n", rssi);
        memcpy(rssi_http, rssi_data, 4096);      
        rssi_http[4095] = countRssiFrame;
        countRssi = 0;
        if(countRssiFrame == 255){
            countRssiFrame = 0;  
        }else{
          countRssiFrame ++;
        }*/

        pxRssiTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(httpRssi, &pxRssiTaskWoken);
        if (pxRssiTaskWoken == pdTRUE){
            portYIELD_FROM_ISR();
        }
    //}

}

void IRAM_ATTR onTimerImu() {    

    // IMU data
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
    imu_data[IMU_DATA*countImu+7] = (tim64>>16) & 0xFFFF;
    imu_data[IMU_DATA*countImu+6] = (tim64>>16) & 0xFFFF;
    
    countImu ++;
    
    if(countImu%1000 == 0){
        USE_SERIAL.printf("IMU: %d\n", imu.ax);
        //USE_SERIAL.printf("IMU: %d\n", countImu);
        memcpy(imu_http, imu_data, 20000);        
        imu_http[20000] = countImuFrame;
        countImu = 0;
        if(countImuFrame == 255){
            countImuFrame = 0;  
        }else{
          countImuFrame ++;
        }
        
        pxImuTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(httpImu, &pxImuTaskWoken);
        if (pxImuTaskWoken == pdTRUE){
            portYIELD_FROM_ISR();
        }
    }
}

void setup() {
 
  Serial.begin(115200);
  
   /* setup wifi */
  /*wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  esp_wifi_set_mode(WIFI_MODE_NULL);
  esp_wifi_start();
  
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_filter(&filt);
  esp_wifi_set_promiscuous_rx_cb(&sniffer);
  esp_wifi_set_channel(curChannel, WIFI_SECOND_CHAN_NONE);*/
  wifiMulti.addAP("experiments", "inception");
  Serial.printf("Waiting for wifi connection ...\n");
  while(wifiMulti.run() != WL_CONNECTED);
  Serial.printf("Connected to wifi successfully ...\n");

  // setting up IMU
  while (imu.begin() != INV_SUCCESS)
  {
      USE_SERIAL.println("Unable to communicate with MPU-9250");
      delay(1000);
  }
  USE_SERIAL.printf("Connected to MPU-9250\n");
  // Enable sensors:
  imu.setSensors(INV_XYZ_GYRO | INV_XYZ_ACCEL);
  // Gyro options are +/- 250, 500, 1000, or 2000 dps
  imu.setGyroFSR(2000); // Set gyro to 2000 dps
  // Accel options are +/- 2, 4, 8, or 16 g
  imu.setAccelFSR(8); // Set accel to +/-2g

  // setLPF() can be used to set the digital low-pass filter
  // of the accelerometer and gyroscope.
  // Can be any of the following: 188, 98, 42, 20, 10, 5
  // (values are in Hz).
  imu.setLPF(188); // Set LPF corner frequency to 5Hz
  // setSampleRate. Acceptable values range from 4Hz to 1kHz
  imu.setSampleRate(1000); // Set sample rate to 10Hz

  // creating socket connections for streaming
  while (!clientRssi.connect(host, port)) {
        USE_SERIAL.println("Connection to host failed from RSSI");
  }
  while (!clientImu.connect(host, port)) {
        USE_SERIAL.println("Connection to host failed from IMU");
  }

  // initializing tasks
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
}


// http functions
void httpImuTask(void * parameter){

  USE_SERIAL.printf("Timer initialized ...\n");
  /*while (!clientMic.connect(host, port)) {
        USE_SERIAL.println("Connection to host failed from AUDIO");
  }*/
  clientImu.print(WiFi.macAddress());
  clientImu.print("IMU");
  USE_SERIAL.println("Connected to server successful!");

  for(;;){ 

    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    USE_SERIAL.printf("IMU triggered by interrupt: %d\n", countImuFrame);
    
    // wait for WiFi connection
    if((WiFi.status() == WL_CONNECTED)) {
      
        uint64_t tim64 = esp_timer_get_time();
        int httpCode = clientImu.write((uint8_t *)(imu_http), 20001);
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

void httpRssiTask(void * parameter){

  USE_SERIAL.printf("Timer initialized ...\n");
  /*while (!clientMotion.connect(host, port)) {
        USE_SERIAL.println("Connection to host failed from MOTION");
  }*/
  clientRssi.print(WiFi.macAddress());
  clientRssi.print("RSSI");
  USE_SERIAL.println("Connected to server successful!");

  for(;;){

    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    //USE_SERIAL.printf("RSSI triggered by interrupt: %d\n", countRssiFrame);
    esp_wifi_sta_get_ap_info(&wifidata);
    rssiTimestamp = esp_timer_get_time();
    rssi_data[RSSI_DATA*countRssi] = wifidata.rssi;
    rssi_data[RSSI_DATA*countRssi+4] = rssiTimestamp & 0xFF;
    rssi_data[RSSI_DATA*countRssi+3] = (rssiTimestamp>>8) & 0xFF;
    rssi_data[RSSI_DATA*countRssi+2] = (rssiTimestamp>>8) & 0xFF;
    rssi_data[RSSI_DATA*countRssi+1] = (rssiTimestamp>>8) & 0xFF;
  
    countRssi ++;

    if(countRssi%3276 == 0){
        USE_SERIAL.printf("Sending Frame Rssi: %d\n", countRssiFrame);
        memcpy(rssi_http, rssi_data, 4096);      
        rssi_http[4095] = countRssiFrame;
        countRssi = 0;
        if(countRssiFrame == 255){
            countRssiFrame = 0;  
        }else{
          countRssiFrame ++;
        }

      // wait for WiFi connection
      if((WiFi.status() == WL_CONNECTED)) {
      
          uint64_t tim64 = esp_timer_get_time();
          int httpCode = clientRssi.write((uint8_t *)(rssi_http), 4096);
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


// interrupt task functions
void intImuTask(void * parameter){
    timer2 = timerBegin(2, 80, true);
    timerAttachInterrupt(timer2, &onTimerImu, true);
    timerAlarmWrite(timer2, 1000, true);
    timerAlarmEnable(timer2);

    vTaskSuspend(NULL);
}

void intRssiTask(void * parameter){
    timer3 = timerBegin(3, 80, true);
    timerAttachInterrupt(timer3, &onTimerRssi, true);
    timerAlarmWrite(timer3, 1000, true);
    timerAlarmEnable(timer3);

    vTaskSuspend(NULL);
}

void loop(){
    delay(10000);
}
