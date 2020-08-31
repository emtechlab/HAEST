
#include <WiFi.h>
#include <WiFiMulti.h>
#include "esp_wifi.h"
// #include <SparkFunMPU9250-DMP.h>


// Global Variables
WiFiMulti wifiMulti;
WiFiClient clientRSSI;
//MPU9250_DMP imu;
const uint16_t port = 8090;
const char * host = "10.42.0.1";

int curChannel = 1;
int32_t rssi = 0;
int8_t tcp_rssi = 0;
wifi_ap_record_t wifidata;
uint32_t rssiTimestamp;
uint16_t count = 0;
uint16_t packet_count = 0;

// wifi promiscous mode callbacks
const wifi_promiscuous_filter_t filt={ //Idk what this does
    .filter_mask=WIFI_PROMIS_FILTER_MASK_MGMT|WIFI_PROMIS_FILTER_MASK_DATA
};
// update rssi and timestamp
void sniffer(void* buf, wifi_promiscuous_pkt_type_t type) { 
  // Serial.printf("Received a packet\n");
  wifi_promiscuous_pkt_t *p = (wifi_promiscuous_pkt_t*)buf;
  //if(p->rx_ctrl.rssi > rssi && rssi != 0 && p->rx_ctrl.rssi != 0){
    rssi += p->rx_ctrl.rssi;
  //}
  rssiTimestamp = p->rx_ctrl.timestamp;
  packet_count += 1;
}


void setup() {

  /* start Serial */
  Serial.begin(115200);

  /* setup wifi */
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  esp_wifi_set_mode(WIFI_MODE_NULL);
  esp_wifi_start();
  
  esp_wifi_set_promiscuous(true);
  //esp_wifi_set_promiscuous_filter(&filt);
  esp_wifi_set_promiscuous_rx_cb(&sniffer);
  esp_wifi_set_channel(curChannel, WIFI_SECOND_CHAN_NONE);
  wifiMulti.addAP("experiments", "inception");
  Serial.printf("Waiting for wifi connection ...\n");
  wifiMulti.run();
  Serial.printf("Connected to wifi successfully ...\n");

  Serial.println("starting!");

  while (!clientRSSI.connect(host, port)) {
        Serial.println("Connection to host failed from RSSI");
  }
}

void loop() {
  if((WiFi.status() == WL_CONNECTED)) {
    if(count%1000 == 0){
      Serial.printf("Rssi %d at %d\n", rssi, rssiTimestamp);
    }
    esp_wifi_sta_get_ap_info(&wifidata);
    if(packet_count != 0){
      tcp_rssi = (rssi/packet_count) & 0xFF;
    }
    else{
      tcp_rssi = 0;
    }
    clientRSSI.write((uint8_t *) &tcp_rssi, sizeof(tcp_rssi));
    clientRSSI.write((uint8_t *) &(wifidata.rssi), sizeof(tcp_rssi));
    clientRSSI.write((uint8_t *) &rssiTimestamp, sizeof(rssiTimestamp));
    Serial.printf("Packets received %d in last interval with avg: %d\n", packet_count, tcp_rssi);
    packet_count = 0;
    rssi = 0;
    count++;
    delay(20);
  }else{
     Serial.printf("Please nooooooo .....\n");
  }
}
