
#include <ti/sysbios/BIOS.h>
#include <ti/drivers/timer/GPTimerCC26XX.h>
#include <xdc/runtime/Types.h>
#include <ti/sysbios/knl/Task.h>
#include "gptimer.h"

#define GPT_TASK_PRIORITY                      1
#define GPT_TASK_STACK_SIZE                    512
static Task_Struct GPTTask;
static Char GPTTaskStack[GPT_TASK_STACK_SIZE];

UInt32 overflow_count_loc = 0;
static GPTimerCC26XX_Handle hTimer;

static void timerCallback(GPTimerCC26XX_Handle handle, GPTimerCC26XX_IntMask interruptMask) {
       // interrupt callback code goes here. Minimize processing in interrupt.
	   overflow_count_loc += 1;
}


static void GPT_taskFxn(UArg a0, UArg a1) {
     GPTimerCC26XX_Params params;
     GPTimerCC26XX_Params_init(&params);
     params.width          = GPT_CONFIG_32BIT;
     params.mode           = GPT_MODE_PERIODIC_UP;
     params.debugStallMode = GPTimerCC26XX_DEBUG_STALL_OFF;
     // hTimer = GPTimerCC26XX_open(CC2650_GPTIMER0A, &params);
     hTimer = GPTimerCC26XX_open(0, &params);
     if(hTimer == NULL) {
       // Log_error0("Failed to open GPTimer");
       // System_printf("Failed to open GPTimer\n");
       Task_exit();
     }

     Types_FreqHz  freq;
     BIOS_getCpuFreq(&freq);
     // GPTimerCC26XX_Value loadVal = freq.lo - 1; //47999999
     // GPTimerCC26XX_Value loadVal = 4294967296/2 - 1; //full
     GPTimerCC26XX_Value loadVal = 4294967295; //full
     GPTimerCC26XX_setLoadValue(hTimer, loadVal);
     GPTimerCC26XX_registerInterrupt(hTimer, timerCallback, GPT_INT_TIMEOUT);

     GPTimerCC26XX_start(hTimer);

     while(1) {
      Task_sleep(BIOS_WAIT_FOREVER);
     }
}


// timestamp function
void GPT_int_count(Types_Timestamp64* timestamp){

	timestamp->lo = GPTimerCC26XX_getValue(hTimer);
	timestamp->hi = overflow_count_loc;
	//return timestamp;
}

void GPT_createTask(void)
{
  Task_Params taskParams;

  // Configure task
  Task_Params_init(&taskParams);
  taskParams.stack = GPTTaskStack;
  taskParams.stackSize = GPT_TASK_STACK_SIZE;
  taskParams.priority = GPT_TASK_PRIORITY;

  Task_construct(&GPTTask, GPT_taskFxn, &taskParams, NULL);
}
