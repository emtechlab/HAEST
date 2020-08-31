/*
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleServer.cpp
    Ported to Arduino ESP32 by Evandro Copercini
    updates by chegewara
*/

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <soc/rtc.h>
#include <string>
#include <sstream>

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define LOCAL_CHAR_UUID     "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define REMOTE_CHAR_UUID    "beb5484e-36e1-4688-b7f5-ea07361b26a8"
#define OFF_CHAR_UUID       "beb5485e-36e1-4688-b7f5-ea07361b26a8"


BLECharacteristic *pCharLocal;
BLECharacteristic *pCharRemote;
BLECharacteristic *pCharOff;

uint64_t tx_timestamp;
uint64_t rx_timestamp;
uint64_t remote_timestamp[2];
uint32_t offset[2];

// define timers
hw_timer_t* timer0 = NULL;

// defining semaphores
// for notifications handling
BaseType_t pxNtpTaskWoken;
TaskHandle_t ntpTask;

// callback for remote timestamps
class LocalChar_Callbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic){
    rx_timestamp = esp_timer_get_time();
    uint8_t* received = pCharacteristic->getData();
    memcpy(remote_timestamp, received, 16); 
    uint64_t t3 = remote_timestamp[0];
    uint64_t t2 = remote_timestamp[1];
    // calculate offset and delay
    offset[0] = (((t2-tx_timestamp)+(t3-rx_timestamp))/2) & 0xFFFFFFFF;
    offset[1] = ((rx_timestamp-tx_timestamp)-(t3-t2)) & 0xFFFFFFFF;
    pCharOff->setValue((uint8_t *) offset, 8);
  }
};


void IRAM_ATTR onTimer() {    

    tx_timestamp = esp_timer_get_time();
    pCharLocal->setValue((uint8_t *) tx_timestamp, 8);
}


void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE work!");

  BLEDevice::init("Sparkfun ESP32 Things");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
  // for local timestamps
  pCharLocal = pService->createCharacteristic(
                                         LOCAL_CHAR_UUID,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_NOTIFY
                                       );
  // Create a BLE Descriptor
  pCharLocal->addDescriptor(new BLE2902());
  // Set initail value
  pCharLocal->setValue((uint8_t *) tx_timestamp, 8);
  
  // remote
  pCharRemote = pService->createCharacteristic(
                                         REMOTE_CHAR_UUID,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE
                                       );
  // Register callback
  pCharRemote->setCallbacks(new LocalChar_Callbacks());
  // Set initail value
  pCharOff->setValue((uint8_t *) remote_timestamp, 16);
  
  // offset + delay
  pCharOff = pService->createCharacteristic(
                                         OFF_CHAR_UUID,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_NOTIFY
                                       );
  // Create a BLE Descriptor
  pCharOff->addDescriptor(new BLE2902());
  // Set initail value
  pCharOff->setValue((uint8_t *) offset, 8);

  pService->start();
  // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("Characteristic defined! Now you can read it in your phone!");

  timer0 = timerBegin(0, 80, true);
  timerAttachInterrupt(timer0, &onTimer, true);
  timerAlarmWrite(timer0, 2000000, true);
  timerAlarmEnable(timer0);

  
}

void loop() {
}
