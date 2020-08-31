/******************************************************************************

 @file  sensortag_hum.c

 @brief This file contains the Sensor Tag sample application,
        Humidity part, for use with the TI Bluetooth Low
        Energy Protocol Stack.

 Group: WCS, BTS
 Target Device: CC2650, CC2640, CC1350

 ******************************************************************************

 Copyright (c) 2015-2016, Texas Instruments Incorporated
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:

 *  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

 *  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 *  Neither the name of Texas Instruments Incorporated nor the names of
    its contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 ******************************************************************************
 Release Name: ble_sdk_2_02_01_18
 Release Date: 2016-10-26 15:20:04
 *****************************************************************************/

#ifndef EXCLUDE_NTP
/*********************************************************************
 * INCLUDES
 */
#include "gatt.h"
#include "gattservapp.h"
#include "board.h"
#include "string.h"


#include<xdc/runtime/Types.h>			// [LATER]
#include "gptimer.h"

#include "ntpservice.h"
#include "ntp.h"
#include "sensortag.h"
#include "SensorTagTest.h"
#include "SensorUtil.h"

#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Task.h>

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * CONSTANTS
 */

// How often to perform sensor reads (milliseconds)
#define SENSOR_DEFAULT_PERIOD   200			// used to be 1000 		[LATER]

// Length of the data for this sensor
#define SENSOR_DATA_LEN         NTP_LOCAL_LEN

// Task configuration
#define SENSOR_TASK_PRIORITY    1
#define SENSOR_TASK_STACK_SIZE  600

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL VARIABLES
 */

// Entity ID globally used to check for source and/or destination of messages
static ICall_EntityID sensorSelfEntity;

// Semaphore globally used to post events to the application thread
static ICall_Semaphore sensorSem;

// Task setup
static Task_Struct sensorTask;
static Char sensorTaskStack[SENSOR_TASK_STACK_SIZE];

// Parameters
static uint8_t sensorConfig[NTP_LOCAL_LEN];
static uint8_t sensorPeriod[NTP_REMOTE_LEN];

// Time keeping
static int32 pre_offset;
static uint32_t pre_delay;
Types_Timestamp64 tx_time;

/*********************************************************************
 * LOCAL FUNCTIONS
 */
static void sensorTaskFxn(UArg a0, UArg a1);
static void sensorConfigChangeCB(uint8_t paramID);
static void initCharacteristicValue(uint8_t paramID, uint8_t value,
                                    uint8_t paramLen);

/*********************************************************************
 * PROFILE CALLBACKS
 */
static sensorCBs_t sensorCallbacks =
{
  sensorConfigChangeCB,  // Characteristic value change callback
};


/*********************************************************************
 * PUBLIC FUNCTIONS
 */
/*********************************************************************
 * @fn      SensorTagHum_createTask
 *
 * @brief   Task creation function for the SensorTag
 *
 * @param   none
 *
 * @return  none
 */
void SensorTagNtp_createTask(void)
{
  Task_Params taskParames;

  // Create the task for the state machine
  Task_Params_init(&taskParames);
  taskParames.stack = sensorTaskStack;
  taskParames.stackSize = SENSOR_TASK_STACK_SIZE;
  taskParames.priority = SENSOR_TASK_PRIORITY;

  Task_construct(&sensorTask, sensorTaskFxn, &taskParames, NULL);
}

/*********************************************************************
 * @fn      SensorTagHum
 *
 * @brief   Initialization function for the SensorTag humidity sensor
 *
 * @param   none
 *
 * @return  none
 */
void SensorTagNtp_init(void)
{
  // Add service
  Ntp_addService();

  // Register callbacks with profile
  Ntp_registerAppCBs(&sensorCallbacks);

  // Initialize the module state variables
  SensorTagNtp_reset();
  initCharacteristicValue(SENSOR_PERI,
		  	  	  	  	  0,
						  NTP_LOCAL_LEN);

}


int32 time_us(Types_Timestamp64 time){

	int32 count32 = 4294967296;
    int32 micro = time.hi*count32/48 + time.lo/48;

	return micro;
}


int32 time_int(uint8_t* time){

	int32 micro;
	memcpy(&micro, &time, 8);

	return micro;
}


/*********************************************************************
 * @fn      SensorTagHum_processCharChangeEvt
 *
 * @brief   SensorTag Humidity event handling
 *
 * @param   none
 *
 * @return  none
 */
void SensorTagNtp_processCharChangeEvt(uint8_t paramID)
{
  uint8_t newValue[NTP_REMOTE_LEN];
  uint8_t offset[NTP_LOCAL_LEN];
  Types_Timestamp64 time;

  switch (paramID)
  {
  case  SENSOR_CONF:
	  // GET TIMESTAMP HERE
	  GPT_int_count(&time);
	  Ntp_getParameter(SENSOR_CONF, &newValue);
	  memcpy(&sensorConfig, &newValue, NTP_REMOTE_LEN);

	  // CALL FUNCTION TO CALCULATE AND SET OFFSET
	  int32 offset_temp = ((time_int(newValue) - time_us(tx_time)) + (time_int(newValue[8]) - time_us(time)))/2;
	  int32 delay = (time_us(time) - time_us(tx_time)) - (time_int(newValue[8]) - time_int(newValue));
      pre_offset = offset_temp;
      pre_delay = pre_delay;
      memcpy(offset, &offset_temp, 4);
	  memcpy(&(offset[4]), &delay, 4);
	  Ntp_setParameter(SENSOR_PERI, SENSOR_DATA_LEN, offset);

    break;

  default:
    // Should not get here
    break;
  }
}

/*********************************************************************
 * @fn      SensorTagHum_reset
 *
 * @brief   Reset characteristics
 *
 * @param   none
 *
 * @return  none
 */
void SensorTagNtp_reset(void)
{
  initCharacteristicValue(SENSOR_DATA, 0, SENSOR_DATA_LEN);
  initCharacteristicValue(SENSOR_CONF, 0, NTP_REMOTE_LEN);
  initCharacteristicValue(SENSOR_PERI, 0, SENSOR_DATA_LEN);
}

/*********************************************************************
* Private functions
*/


/*********************************************************************
 * @fn      sensorTaskFxn
 *
 * @brief   The task loop of the humidity readout task
 *
 * @param   a0 - not used
 *
 * @param   a1 - not used
 *
 * @return  none
 */
static void sensorTaskFxn(UArg a0, UArg a1)
{
  typedef union {
    struct {
      uint16_t timeHi, timeHi2, timeLo, timeLo2; 	// changed [LATER]
    } v;
    uint8_t a[SENSOR_DATA_LEN];
  } Data_t;

  // Register task with BLE stack
  ICall_registerApp(&sensorSelfEntity, &sensorSem);

  Data_t data;

  //Timestamp_get64(&time);
  //System_printf('Time 0: %d ', time.lo);
  // Task loop
  while (true)
  {
    if (gapProfileState == GAPROLE_CONNECTED)
    {
      GPT_int_count(&tx_time);
      data.v.timeLo2 = (tx_time.lo & 0xFFFF);
      data.v.timeLo = (tx_time.lo>>16 & 0xFFFF);
      data.v.timeHi2 = (tx_time.hi & 0xFFFF);
      data.v.timeHi = (tx_time.hi>>16 & 0xFFFF);
      Ntp_setParameter(SENSOR_DATA, SENSOR_DATA_LEN, data.a);
    }
    else
    {
      DELAY_MS(SENSOR_DEFAULT_PERIOD);
    }
  }
}

/*********************************************************************
 * @fn      sensorConfigChangeCB
 *
 * @brief   Callback from Humidity Service indicating a value change
 *
 * @param   paramID - parameter ID of the value that was changed.
 *
 * @return  none
 */
static void sensorConfigChangeCB(uint8_t paramID)
{
  // Wake up the application thread
  SensorTag_charValueChangeCB(SERVICE_ID_NTP, paramID);
}

/*********************************************************************
 * @fn      initCharacteristicValue
 *
 * @brief   Initialize a characteristic value
 *
 * @param   paramID - parameter ID of the value is to be cleared
 *
 * @param   value - value to initialize with
 *
 * @param   paramLen - length of the parameter
 *
 * @return  none
 */
static void initCharacteristicValue(uint8_t paramID, uint8_t value,
                                    uint8_t paramLen)
{
  uint8_t data[NTP_REMOTE_LEN];

  memset(data,value,paramLen);
  Ntp_setParameter(paramID, paramLen, data);
}
#endif // EXCLUDE_NTP

/*********************************************************************
*********************************************************************/

