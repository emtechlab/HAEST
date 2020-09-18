/* Read sensors, timestamp them and send over to the internet*/

// includes
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>


#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define LOCAL_CHAR_UUID     "beb5483e-36e1-4688-b7f5-ea07361b26a8"


// Global variables
int intPin = 34;
uint64_t timestamp;

BLECharacteristic *pCharLocal;
BaseType_t pxReadTaskWoken;
TaskHandle_t httpRead;

// interrupt service routines
void IRAM_ATTR isrGpio() {

    timestamp = esp_timer_get_time();
    //Serial.println(*((uint8_t *) &timestamp));
    //pCharLocal->setValue((uint8_t *) &timestamp, 8);
    //pCharLocal->notify();
    pxReadTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(httpRead, &pxReadTaskWoken);
    if (pxReadTaskWoken == pdTRUE){
            portYIELD_FROM_ISR();
    }
}

void httpReadTask(void * parameter){

    while(1){
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      //Serial.println(*((uint8_t *) &timestamp));
      pCharLocal->setValue((uint8_t *) &timestamp, 8);
      pCharLocal->notify();
      portYIELD();
    }
}

void setup() {
 
  Serial.begin(115200);

  pinMode(intPin, INPUT_PULLUP);
  attachInterrupt(intPin, isrGpio, FALLING);

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
  pCharLocal->setValue((uint8_t *) &timestamp, 8);

  pService->start();
  // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("Characteristic defined! Now you can read it in your phone!");

  xTaskCreatePinnedToCore(httpReadTask,   // function to implement task
                            "httpRead",      // Task name
                            4096,         // stack size in words
                            NULL,           // arguments
                            1,              // priority
                            &httpRead,       // task handle
                            1);

  
}


// http functions
void loop(){
}
