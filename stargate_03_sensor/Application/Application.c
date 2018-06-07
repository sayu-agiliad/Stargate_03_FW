/*
 * Application.c
 *
 */
#include <string.h>

#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/family/arm/cc26xx/TimestampProvider.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Queue.h>
#include <ti/sysbios/BIOS.h>

#include <xdc/runtime/System.h>
#include <xdc/runtime/Types.h>
#include <xdc/runtime/Timestamp.h>
#include <ti/sysbios/knl/Clock.h>

#include "bcomdef.h"

#include "hci_tl.h"
#include "linkdb.h"
#include "gatt.h"
#include "gapgattserver.h"
#include "gattservapp.h"
#include "central.h"
#include "gapbondmgr.h"
#include "simpleGATTprofile.h"

#include "osal_snv.h"
#include "ICallBleAPIMSG.h"

#include "util.h"
#include "board_key.h"
#include "board_lcd.h"
#include "Board.h"

#include "stdio.h"
#include "sm.h"

#include "sensor_api.h"

#include <util.h>

static void App_taskFxn(UArg a0, UArg a1);
/*********************************************************************
 * TYPEDEFS
 */

// App event passed from profiles.
typedef struct
{
  appEvtHdr_t hdr; // event header
  uint8_t *pData;  // event data
} AppEvt_t;


// Task configuration
#define APP_TASK_PRIORITY                     1

#ifndef APP_TASK_STACK_SIZE
#define APP_TASK_STACK_SIZE                   864
#endif
// Task configuration
Task_Struct AppTask;
Char AppTaskStack[APP_TASK_STACK_SIZE];


static Queue_Struct appMsg;
static Queue_Handle application_MsgQueue;

// Entity ID globally used to check for source and/or destination of messages
static ICall_EntityID selfEntity;

// Semaphore globally used to post events to the application thread
static ICall_Semaphore sem;

static Semaphore_Struct App_sem;
static Semaphore_Handle App_sem_hdl;

/*********************************************************************
 * @fn      App_createTask
 *
 * @brief   Task creation function for the  Application.
 *
 * @param   none
 *
 * @return  none
 */
void App_createTask(void)
{
  Task_Params taskParams;

  // Configure task
  Task_Params_init(&taskParams);
  taskParams.stack = AppTaskStack;
  taskParams.stackSize = APP_TASK_STACK_SIZE;
  taskParams.priority = APP_TASK_PRIORITY;

  Task_construct(&AppTask, App_taskFxn, &taskParams, NULL);
}

void App_init(void)
{
    Semaphore_Params semParams;
    // Setup semaphore for sequencing accesses to PIN_open()
    Semaphore_Params_init(&semParams);
//    semParams.mode = Semaphore_Mode_BINARY;
    App_sem_hdl = Semaphore_create(0, &semParams, NULL);


}
/*********************************************************************
 * @fn      App_enqueueMsg
 *
 * @brief   Creates a message and puts the message in RTOS queue.
 *
 * @param   event - message event.
 * @param   state - message state.
 * @param   pData - message data pointer.
 *
 * @return  TRUE or FALSE
 */
static uint8_t App_enqueueMsg(uint8_t event, uint8_t state,
                                           uint8_t *pData)
{
	AppEvt_t *pMsg = ICall_malloc(sizeof(AppEvt_t));

  // Create dynamic pointer to message.
  if (pMsg)
  {
    pMsg->hdr.event = event;
    pMsg->hdr.state = state;
    pMsg->pData = pData;

    // Enqueue the message.
    return Util_enqueueMsg(application_MsgQueue, App_sem_hdl, (uint8_t *)pMsg);
  }

  return FALSE;
}

/*********************************************************************
 * @fn      App_taskFxn
 *
 * @brief   Application task entry point .
 *
 * @param   none
 *
 * @return  events not processed
 */
static void App_taskFxn(UArg a0, UArg a1)
{
	uint8_t data, count = 3;
	App_init();

	SendData(&data, count);
	 for (;;)
	  {
	    // Waits for a signal to the semaphore associated with the calling thread.
	    // Note that the semaphore associated with a thread is signaled when a
	    // message is queued to the message receive queue of the thread or when
	    // ICall_signal() function is called onto the semaphore.
		    System_printf("wait for app event\n ");
		    System_flush();
		 bool status = Semaphore_pend(App_sem_hdl, BIOS_WAIT_FOREVER);

	    // If RTOS queue is not empty, process app message
	    while (!Queue_empty(application_MsgQueue))
	    {
	    	AppEvt_t *pMsg = (AppEvt_t *)Util_dequeueMsg(application_MsgQueue);
	      if (pMsg)
	      {
	        // Process message
//	        BLECentral_processAppMsg(pMsg);

	        // Free the space from the message
	        ICall_free(pMsg);
	      }
	    }
	  }

}
