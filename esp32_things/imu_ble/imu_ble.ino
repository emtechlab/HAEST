#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <SparkFunMPU9250-DMP.h>

#define SerialPort Serial
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define LOCAL_CHAR_UUID     "beb5483e-36e1-4688-b7f5-ea07361b26a8"


uint16_t imu_data[10];
MPU9250_DMP imu;
BLECharacteristic *pCharLocal;
// define timers
hw_timer_t* timer0 = NULL;
BaseType_t pxReadTaskWoken;
TaskHandle_t httpRead;

void IRAM_ATTR onTimer() {   

    pxReadTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(httpRead, &pxReadTaskWoken);
    if (pxReadTaskWoken == pdTRUE){
            portYIELD_FROM_ISR();
    }
}


void httpReadTask(void * parameter){

    while(1){
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      uint64_t tim64;
      if (imu.dataReady()){
          imu.update();
          tim64 = esp_timer_get_time();
          imu_data[0] = imu.ax;
          imu_data[1] = imu.ay;
          imu_data[2] = imu.az;
          imu_data[3] = imu.gx;
          imu_data[4] = imu.gy;
          imu_data[5] = imu.gz;
      }else{
          Serial.printf("Values were not available ...\n");
          memset(imu_data, 0, 12);
          tim64 = esp_timer_get_time();
      }

      imu_data[9] = tim64 & 0xFFFF;
      imu_data[8] = (tim64>>16) & 0xFFFF;
     imu_data[7] = (tim64>>32) & 0xFFFF;
     imu_data[6] = (tim64>>48) & 0xFFFF;
     pCharLocal->setValue((uint8_t *) imu_data, 20);
     pCharLocal->notify();
     portYIELD();
  }
}


void setup() 
{
  SerialPort.begin(115200);

  // Call imu.begin() to verify communication with and
  // initialize the MPU-9250 to it's default values.
  // Most functions return an error code - INV_SUCCESS (0)
  // indicates the IMU was present and successfully set up
  if (imu.begin() != INV_SUCCESS)
  {
    while (1)
    {
      SerialPort.println("Unable to communicate with MPU-9250");
      SerialPort.println("Check connections, and try again.");
      SerialPort.println();
      delay(5000);
    }
  }

  // Use setSensors to turn on or off MPU-9250 sensors.
  // Any of the following defines can be combined:
  // INV_XYZ_GYRO, INV_XYZ_ACCEL, INV_XYZ_COMPASS,
  // INV_X_GYRO, INV_Y_GYRO, or INV_Z_GYRO
  // Enable all sensors:
  imu.setSensors(INV_XYZ_GYRO | INV_XYZ_ACCEL);

  // Use setGyroFSR() and setAccelFSR() to configure the
  // gyroscope and accelerometer full scale ranges.
  // Gyro options are +/- 250, 500, 1000, or 2000 dps
  imu.setGyroFSR(2000); // Set gyro to 2000 dps
  // Accel options are +/- 2, 4, 8, or 16 g
  imu.setAccelFSR(8); // Set accel to +/-2g

  // setLPF() can be used to set the digital low-pass filter
  // of the accelerometer and gyroscope.
  // Can be any of the following: 188, 98, 42, 20, 10, 5
  // (values are in Hz).
  imu.setLPF(188); // Set LPF corner frequency to 5Hz

  // The sample rate of the accel/gyro can be set using
  // setSampleRate. Acceptable values range from 4Hz to 1kHz
  imu.setSampleRate(1000); // Set sample rate to 10Hz

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
  pCharLocal->setValue((uint8_t *) imu_data, 20);
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
  timerAlarmWrite(timer0, 5000, true);
  timerAlarmEnable(timer0);

  xTaskCreatePinnedToCore(httpReadTask,   // function to implement task
                            "httpRead",      // Task name
                            4096,         // stack size in words
                            NULL,           // arguments
                            2,              // priority
                            &httpRead,       // task handle
                            0);

}

void loop() {
}
