#include <SimpleBLE.h>
#include <soc/rtc.h>

int ledPin = 5;
#define RTC_CNTL_TIME_UPDATE_REG 0x3FF4800C
#define  RTC_CNTL_CLK_CONF_REG 0x3FF48070
#define mask 0x3FFFFFFF
#define newVal 0x40000000
#define updat 0x80000000

union{
    uint32_t data[2];
    unsigned char bytes[8];  
}tempdata;

void setup()
{
    pinMode(ledPin, OUTPUT);
    Serial.begin(115200);
    uint32_t rtc_src = *((volatile uint32_t *) RTC_CNTL_CLK_CONF_REG);
    *((volatile uint32_t *) RTC_CNTL_CLK_CONF_REG) = (rtc_src & mask) | newVal;
    //rtc_clk_32k_enable(true);
    rtc_clk_32k_enable_external();
}

void loop()
{
    float temp = temperatureRead();
    Serial.println("Hello, world! temperature:");
    Serial.println(temp, 5);
    //digitalWrite(ledPin, HIGH);
    //delay(500);
    //digitalWrite(ledPin, LOW);
    // update it
    uint32_t freq = rtc_clk_slow_freq_get_hz();
    Serial.println(freq);
    uint64_t tim = rtc_time_get();
    uint32_t tim2 = tim & 0xFFFFFFFF;
    Serial.println(tim2);
    /*uint32_t up = *((volatile uint32_t *) RTC_CNTL_TIME_UPDATE_REG);
    if ((up & 0x40000000) != 0){
      *((volatile uint32_t *) RTC_CNTL_TIME_UPDATE_REG) = (up & mask) | updat;
      Serial.println((up & newVal) | updat);
      uint32_t time_hi = *((volatile uint32_t *) (0x3FF48014));
      uint32_t time_lo = *((volatile uint32_t *) (0x3FF48010));
      Serial.println(time_lo);
      Serial.println(time_hi);
    }*/
    delay(1000);
}
