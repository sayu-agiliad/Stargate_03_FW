/*
 * ble_mngr.c
 *
 */

#include <string.h>

#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/family/arm/cc26xx/TimestampProvider.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Queue.h>
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
#include "osal_snv.h"
#include "ICallBleAPIMSG.h"

#include "util.h"
#include "board_key.h"
#include "board_lcd.h"
#include "Board.h"


#include "bleUserConfig.h"

#include <ti/drivers/lcd/LCDDogm1286.h>
#include "stdio.h"
#include "sm.h"
#include "ble_mngr.h"
#include "../GATTprofile.h"

/* One shot timer clock handler */
void OneShot_timer_event_handler(UArg a0)
{
    events |= SBC_PACKET_VERIFY_EVENT;

    // Wake up the application thread when it waits for clock event
      Semaphore_post(sem);
}
void Sw_timer_event_handler(UArg a0)
{

    events |= SBC_PERIODIC_EVENT;

    // Wake up the application thread when it waits for clock event
    Semaphore_post(sem);
}


/*********************************************************************
 * @fn      BLECentral_Init
 *
 * @brief   Initialization function for the Simple BLE Central App Task.
 *          This is called during initialization and should contain
 *          any application specific initialization (ie. hardware
 *          initialization/setup, table initialization, power up
 *          notification).
 *
 * @param   none
 *
 * @return  none
 */
static void BLECentral_init(void)
{
  uint8_t i;

  // ******************************************************************
  // N0 STACK API CALLS CAN OCCUR BEFORE THIS CALL TO ICall_registerApp
  // ******************************************************************
  // Register the current thread as an ICall dispatcher application
  // so that the application can send and receive messages.
  ICall_registerApp(&selfEntity, &sem);


  // Create an RTOS queue for message from profile to be sent to app.
  appMsgQueue = Util_constructQueue(&appMsg);

  // Setup discovery delay as a one-shot timer

  Util_constructClock(&OneShot_timer_Clock, OneShot_timer_event_handler,
                      ONESHOT_TIMER_DELAY, 0, FALSE, SBC_PACKET_VERIFY_EVENT);
//

//  Board_initKeys(BLECentral_keyChangeHandler);

//  Board_openLCD();

  // Initialize internal data
  for (i = 0; i < MAX_NUM_BLE_CONNS; i++)
  {
    readRssi[i].connHandle = GAP_CONNHANDLE_ALL;
    readRssi[i].pClock = NULL;
  }

  // Setup Central Profile
  {
    uint8_t scanRes = DEFAULT_MAX_SCAN_RES;

    GAPCentralRole_SetParameter(GAPCENTRALROLE_MAX_SCAN_RES, sizeof(uint8_t),
                                &scanRes);
  }

  // Setup GAP
  GAP_SetParamValue(TGAP_GEN_DISC_SCAN, DEFAULT_SCAN_DURATION);
  GAP_SetParamValue(TGAP_LIM_DISC_SCAN, DEFAULT_SCAN_DURATION);

  /* set scan window, interval */
  GAP_SetParamValue(TGAP_GEN_DISC_SCAN_WIND, SCAN_WINDOW_DURATION);
  GAP_SetParamValue(TGAP_GEN_DISC_SCAN_INT, SCAN_INTERVAL_DURATION);
  GAP_SetParamValue(TGAP_REJECT_CONN_PARAMS, FALSE);
  /* Changing Gap parameters for optimization */

  GAP_SetParamValue(TGAP_CONN_EST_INT_MIN,40);
  GAP_SetParamValue(TGAP_CONN_EST_INT_MAX,56);

  GGS_SetParameter(GGS_DEVICE_NAME_ATT, GAP_DEVICE_NAME_LEN,
                   (void *)attDeviceName);

  // Setup the GAP Bond Manager
  {
    // configuring GAP Bond Manager for 'Just Works' encryption
//    uint32_t passkey = DEFAULT_PASSCODE;
//   uint8_t pairMode = GAPBOND_PAIRING_MODE_INITIATE/*DEFAULT_PAIRING_MODE*/;
    uint8_t pairMode = GAPBOND_PAIRING_MODE_INITIATE;
//	  uint8_t pairMode = GAPBOND_PAIRING_MODE_NO_PAIRING;

    uint8_t mitm = DEFAULT_MITM_MODE;
    uint8_t ioCap = DEFAULT_IO_CAPABILITIES;
    uint8_t bonding = true;

//    GAPBondMgr_SetParameter(GAPBOND_DEFAULT_PASSCODE, sizeof(uint32_t),
//                            &passkey);

    GAPBondMgr_SetParameter(GAPBOND_PAIRING_MODE, sizeof(uint8_t), &pairMode);
    GAPBondMgr_SetParameter(GAPBOND_MITM_PROTECTION, sizeof(uint8_t), &mitm);
    GAPBondMgr_SetParameter(GAPBOND_ERASE_ALLBONDS, 0, 0);

    GAPBondMgr_SetParameter(GAPBOND_IO_CAPABILITIES, sizeof(uint8_t), &ioCap);
    GAPBondMgr_SetParameter(GAPBOND_BONDING_ENABLED, sizeof(uint8_t), &bonding);

  }

  // Initialize GATT Client
  VOID GATT_InitClient();

  // Register to receive incoming ATT Indications/Notifications
  GATT_RegisterForInd(selfEntity);

  // Initialize GATT attributes
  GGS_AddService(GATT_ALL_SERVICES);         // GAP
  GATTServApp_AddService(GATT_ALL_SERVICES); // GATT attributes

  /* Service, characteristics registration */
  GattProfile_AddService(GATT_ALL_SERVICES);

  // Start the Device
  VOID GAPCentralRole_StartDevice(&BLECentral_roleCB);

  // Register with bond manager after starting device
  GAPBondMgr_Register(&BLECentral_bondCB);

  // Register with GAP for HCI/Host messages (for RSSI)
  GAP_RegisterForMsgs(selfEntity);

  // Register for GATT local events and ATT Responses pending for transmission
  GATT_RegisterForMsgs(selfEntity);

  /* Initializing application's state to 'start discovery' */

}

/*********************************************************************
 * @fn      StartScan
 *
 * @brief   initiate scanning by semaphore post
 *
 * @param   none
 *
 * @return  none
 */

void StartScan(void)
{
    app_state = APP_START_DISCOVERY;
    Sw_timer_event_handler(0);

}
void app_state_handler()
{

uint8_t addrType;
uint8_t *peerAddr;

uint8_t i=0,k=0,m=0;

uint8_t status = 0;

// Connect or disconnect

    switch(app_state)
    {


    case APP_START_DISCOVERY:


        if (state != BLE_STATE_CONNECTED)
        {
            if (!scanningStarted)
            {

                scanningStarted = TRUE;
                scanRes = 0;
                status = GAPCentralRole_StartDiscovery(DEFAULT_DISCOVERY_MODE,
							DEFAULT_DISCOVERY_ACTIVE_SCAN,
							DEFAULT_DISCOVERY_WHITE_LIST);

			    System_printf("Scan started : %d\n ", status);
			    System_flush();


            }

        }


        break;

    case APP_ESTABLISH_LINK:


    	if (state == BLE_STATE_IDLE)
        {
            // if there is a scan result
            if (scanRes > 0)
            {

            	uint8_t ret=0;
                // connect to current device in scan result

                peerAddr = devList[scanRes-1].addr;
                addrType = devList[scanRes-1].addrType;

                if((ret = GAPCentralRole_EstablishLink(DEFAULT_LINK_HIGH_DUTY_CYCLE,
                DEFAULT_LINK_WHITE_LIST,addrType, peerAddr)) == SUCCESS)
                {
                    state = BLE_STATE_CONNECTING;
//                    Util_restartClock(&OneShot_timer_Clock, 2000);
                    chck_conn_state = 1;

                }
                /* establish link failed */
                else
                {

                    /* Go back to scanning for devices */


                    app_state = APP_START_DISCOVERY;
                    Sw_timer_event_handler(0);
                    /* Disconnection indicating LED */

                    /* switch LED off */

                }

            }

        }

        break;


    case APP_PREPARE_PACKET:


        /* Check if security packet is to be prepared or not */
        if(preparePacketFlag == true){

            /* Reset above flag for single process */
            preparePacketFlag = false;
            isRegistered = true;
            /* Send Security packet to App, as services have been discovered for a registered App */
            if(isRegistered == TRUE){

                  /* Reset securityPacket buffer to 0 before filling it */
                  memset(&securityPacket[0],0,sizeof(securityPacket));

                  /* Adding received random bytes to packet to be written to App's characteristic
                   * with modified values */
                  originalUSSK_len = 4;
                  for(i=0; i<((uint8_t)DECRYPTED_PACKET_LENGTH - (originalUSSK_len+2)); i++){
                      /* adding 0x1 to each byte of received random bytes */
                      securityPacket[i] =  randomReceived[i];// + 0x1;
                  }


//                      /* Get a 4 byte random number */
                      fourByteRandom = Util_GetTRNG();

                  memcpy(&securityPacket[i],&fourByteRandom,4);
                  /* Encrypt generated bytes */
                  uint8 h;
                  System_printf("\n GENERATED PACKET RAW \n");
                  for(h = 0;h<16; h++)
                  {
                 	 System_printf(" %x\t", securityPacket[h]);
                  }
                  System_printf("\n");
                  System_flush();

                  HCI_LE_EncryptCmd(&aesKey[0], &securityPacket[0]);

            }

    }



        break;

    case APP_ENCRYPT_PACKET:

        /* Reverse encrypted packet before storing in security packet */
        ReverseBytes(&encryptedPacketBuffer[0], 16);

        /* Store encrypted security packet data */
        memcpy(&encryptedSecurityPacket[0], &encryptedPacketBuffer[0],16);


        /* Encrypt generated bytes */
        HCI_LE_EncryptCmd(&aesKey[0], &securityPacket[16]);


        break;

    case APP_WRITE_PACKET:

        /* Check if prepared security packet is to be written on App's characteristic */
        if(writePacketFlag == true)
        {
        	uint8_t status = 0;
            /* Reset above flag for single process */
            writePacketFlag = false;

            /* Reverse encrypted packet before storing in security packet */
            ReverseBytes(&encryptedPacketBuffer[0], 16);

            uint8 h;
            System_printf("\n GENERATED PACKET ENC \n");
            for(h = 0;h<16; h++)
            {
           	 System_printf(" %x\t", encryptedPacketBuffer[h]);
            }
            System_printf("\n");
            System_flush();
            memcpy(&encryptedSecurityPacket[0], &encryptedPacketBuffer[0],16);

            GattProfile_SetParameter(GATTPROFILE_CHAR1, 16, encryptedSecurityPacket);


              /* 20 bytes of packet has been sent */
             first20BytesSent = true;


    }

        break;

    case APP_RECEIVE_ENCRYPTED_PACKET:
        if(receiveEncryptedPcktFlag == true){

            receiveEncryptedPcktFlag = false;

             HCI_EXT_DecryptCmd(&aesKey[0], &rcvdEncryptedData[0]);


        }
        break;

    case APP_VERIFY_RCVD_PACKET:
    {
    	 uint8 condition = false;
    	 uint8 tmp_compare[4] = {0};
        uint8 i = 0;

        if(verifyRcvdPacketFlag == true){


        	verifyRcvdPacketFlag = false;

            /* Update received random data to compare with sent one */
            for(i=0;i<4;i++)
                decryptedrcvdData[i] = decryptedrcvdData[i];// - 0x01;

//            ReverseBytes(&decryptedrcvdData[0], 4);

            memcpy(&tmp_compare[0],&fourByteRandom,4);

            /* Compare received random data with sent one */
            for(i=0;i<4;i++)
            {

                if(decryptedrcvdData[i] == tmp_compare[i])
                    ;

                else
                    break;
            }

            /* Received data is different */
            if(i == 4)
            {
//            	app_state = APP_TERMINATE_LINK;
//				Sw_timer_event_handler(0);
//				Util_startClock(&OneShot_timer_Clock);

            	PIN_setOutputValue(led_pins_handle, Board_LED1, 0);

            	PIN_setOutputValue(led_pins_handle, Board_LED2, 1);
				on_time = 100;
				off_time = 1;

            }
            else
            {
                /* Set application's state to 'terminate link' to disconnect
                 * as received data didn't match with sent one */
            	app_state = APP_TERMINATE_LINK;
				Sw_timer_event_handler(0);
            }


        }


    }
        break;

    case APP_TERMINATE_LINK:
        if (state == BLE_STATE_CONNECTING ||
              state == BLE_STATE_CONNECTED)
        {
//        Task_sleep(1000);
        // disconnect
        state = BLE_STATE_DISCONNECTING;
//        if(pkt_verify_timeout == 1)
        {
        	pkt_verify_timeout = 0;
        	GAPCentralRole_TerminateLink(0xffff);
        }
//        else
//        GAPCentralRole_TerminateLink(connHandle);


        }
        break;

     case APP_INITIATE_PAIR:

        default:
        break;
    }


}

/*********************************************************************
 * @fn      Central_createTask
 *
 * @brief   Task creation function for the  BLE Central.
 *
 * @param   none
 *
 * @return  none
 */
void Central_createTask(void)
{
  Task_Params taskParams;

  // Configure task
  Task_Params_init(&taskParams);
  taskParams.stack = sbcTaskStack;
  taskParams.stackSize = SBC_TASK_STACK_SIZE;
  taskParams.priority = SBC_TASK_PRIORITY;

  Task_construct(&sbcTask, BLECentral_taskFxn, &taskParams, NULL);
}



static void PacketVerifyFail_handler(void)
{
    app_state = APP_TERMINATE_LINK;
    Sw_timer_event_handler(0);

}


/*********************************************************************
 * @fn      BLECentral_processAppMsg
 *
 * @brief   Central application event processing function.
 *
 * @param   pMsg - pointer to event structure
 *
 * @return  none
 */
static void BLECentral_processAppMsg(sbcEvt_t *pMsg)
{
  switch (pMsg->hdr.event)
  {
    case SBC_STATE_CHANGE_EVT:
      BLECentral_processStackMsg((ICall_Hdr *)pMsg->pData);

      // Free the stack message
      ICall_freeMsg(pMsg->pData);
      break;


    // Pairing event
    case SBC_PAIRING_STATE_EVT:
      {
//        BLECentral_processPairState(pMsg->hdr.state, *pMsg->pData);

        ICall_free(pMsg->pData);
        break;
      }


    default:
      // Do nothing.
      break;
  }
}


/*********************************************************************
 * @fn      BLECentral_taskFxn
 *
 * @brief   Application task entry point for the Simple BLE Central.
 *
 * @param   none
 *
 * @return  events not processed
 */
static void BLECentral_taskFxn(UArg a0, UArg a1)
{
  // Initialize application
  BLECentral_init();

  // Application main loop
  for (;;)
  {
    // Waits for a signal to the semaphore associated with the calling thread.
    // Note that the semaphore associated with a thread is signaled when a
    // message is queued to the message receive queue of the thread or when
    // ICall_signal() function is called onto the semaphore.
    ICall_Errno errno = ICall_wait(ICALL_TIMEOUT_FOREVER);

    if (errno == ICALL_ERRNO_SUCCESS)
    {
      ICall_EntityID dest;
      ICall_ServiceEnum src;
      ICall_HciExtEvt *pMsg = NULL;

      if (ICall_fetchServiceMsg(&src, &dest,
                                (void **)&pMsg) == ICALL_ERRNO_SUCCESS)
      {
        if ((src == ICALL_SERVICE_CLASS_BLE) && (dest == selfEntity))
        {
          // Process inter-task message
          BLECentral_processStackMsg((ICall_Hdr *)pMsg);
        }

        if (pMsg)
        {
          ICall_freeMsg(pMsg);
        }
      }
    }

    // If RTOS queue is not empty, process app message
    while (!Queue_empty(appMsgQueue))
    {
      sbcEvt_t *pMsg = (sbcEvt_t *)Util_dequeueMsg(appMsgQueue);
      if (pMsg)
      {
        // Process message
        BLECentral_processAppMsg(pMsg);

        // Free the space from the message
        ICall_free(pMsg);
      }
    }

    if (events & SBC_PERIODIC_EVENT)
    {
      events &= ~SBC_PERIODIC_EVENT;
//      Util_startClock(&Sw_timer_Clock);

      app_state_handler();

    }
    if(events & SBC_PACKET_VERIFY_EVENT)
    {
        /* Reset event flag */
        events &= ~SBC_PACKET_VERIFY_EVENT;

        /* Call one shot timer handler */
        PacketVerifyFail_handler();

    }

/*    if(events & SBC_LED_BLINK_EVENT)
    {
         Reset event flag
        events &= ~SBC_LED_BLINK_EVENT;

        led1 = !led1;
        if(led1)
        {
           Util_rescheduleClock(&led_Clock,on_time);
        }
        else
        {
           Util_rescheduleClock(&led_Clock,off_time);
        }
//
        PIN_setOutputValue(led_pins_handle, Board_LED3, led1);

    }*/
  }
}

static void ReverseBytes(uint8_t *buf, uint8_t len)
{
  uint8_t temp;
  uint8_t index = (uint8_t)(len - 1);
  uint8_t i;

  // adjust length as only half the operations are needed
  len >>= 1;

  // reverse the order of the bytes
  for (i=0; i<len; i++)
  {
    temp           = buf[i];
    buf[i]         = buf[index - i];
    buf[index - i] = temp;
  }

  return;
}

/* This function scans for USSK in the decrypted adv packet */
static void scanForUssk(void)
{
    uint8 usskIndexAscii=0;
    uint8 usskIndexHex=0;
    uint8 receivedUSSK[5]={0};
    originalUSSK_len = 4;
    uint8_t i=0,j=0,h=0;

//    strcpy(originalUSSK, "1234");
    //uint32_t nvmStoredUSSK;
    //uint32_t buffer;
    //uint8 status;

    /* Check for ussk index in decrypted packet */
    /* 14th index has ussk index */
    usskIndexAscii = decryptedData[14];

    /* Calculating index in Hex */
    usskIndexHex = atoi((const char *)&usskIndexAscii);

    System_printf("\n %x, %x \n", usskIndexHex,usskIndexAscii);
    System_flush();
    /* store decrypted ussk in local variable */
    memcpy(&receivedUSSK[0], &decryptedData[usskIndexAscii - 1], originalUSSK_len);

    System_printf("\n####  USSK: %s \n", (char*)receivedUSSK);
     System_flush();
     System_printf("\n decrypted adv data \n");
     for(h = 0;h<16; h++)
     {
    	 System_printf(" %x\t", decryptedData[h]);
     }
     System_printf("\n");
     System_flush();

    /* check if received ussk is correct
     * if true add the current device */
    if(!memcmp(receivedUSSK,originalUSSK, 4)){


//    	Task_sleep(1000);


//        System_printf("\n    USSK MATCHED \n");
//        System_flush();

        /* USSK matched, store random part received in advert packet */
        for(i=0,j=0; j<((uint8_t)DECRYPTED_PACKET_LENGTH - originalUSSK_len); i++,j++){

            /* Skip USSK while storing random part of received packet */
            if(i == (usskIndexAscii - 1)){
                /* Update iterator variable to skip ussk */
                i = i + originalUSSK_len;
            }

            /* Store random part */
            randomReceived[j] = decryptedData[i];

        }

        System_printf("\n random received adv data %%%%%% \n");
        for(h = 0;h<16; h++)
        {
       	 System_printf(" %x\t", randomReceived[h]);
        }
        System_printf("\n");
        System_flush();

        GAPCentralRole_CancelDiscovery();

        /* Add this device, as USSK matched */
        BLECentral_addDeviceInfo(devAddr,devAddrType);

    }
    else
    {
        /* if ussk not matched, print on screen */

        scanRes = 0;
        /* Set application's state to 'start discovery' */


    }


    return;
}

/* This function process HCI command complete event */
static void ProcessHCICmdCompleteEvt(hciEvt_CmdComplete_t *pMsg)
{
    switch (pMsg->cmdOpcode)
    {
        /* Response of decrypt command */
        case HCI_EXT_DECRYPT:
        {

            /* Check if decryption completed successfully */
            if(pMsg->pReturnParam[2] == SUCCESS)
            {
                // decrypt ADV packet from app
                if(app_state == APP_START_DISCOVERY){

                    /* Store decrypted data */
                    memcpy(&decryptedData[0], &(pMsg->pReturnParam[3]),
                          KEYLEN);

                    // reverse byte order of data (MSB..LSB required)
                    ReverseBytes(&decryptedData[0], KEYLEN);

                    /* Check if ussk present in decrypted data */
                    scanForUssk();

                }

                else
                    // decrypt received challenge response from app
                    if(app_state == APP_RECEIVE_ENCRYPTED_PACKET){

                    /* Store decrypted data */
                    memcpy(&decryptedrcvdData[0], &(pMsg->pReturnParam[3]),
                          KEYLEN);

                    // reverse byte order of data (MSB..LSB required)
                    ReverseBytes(&decryptedrcvdData[0], KEYLEN);

                    /* set flag to change state */
                    verifyRcvdPacketFlag = true;

                    /* change app's state to verify received packet */
                    app_state = APP_VERIFY_RCVD_PACKET;
                    Sw_timer_event_handler(0);
                }

            }
        }
        break;

        case HCI_LE_ENCRYPT:
        {


            /* Check if encryption completed successfully */
            if(pMsg->pReturnParam[0] == SUCCESS)
            {
                /* Store encrypted security packet data */
                memcpy(&encryptedPacketBuffer[0], &(pMsg->pReturnParam[1]),16
                /*(uint8_t)DECRYPTED_PACKET_LENGTH - originalUSSK_len + 10*/);


                if(app_state == APP_PREPARE_PACKET){

                /* Change application's state */
                writePacketFlag = true;

                /* Change application's state */
                app_state = APP_WRITE_PACKET;
                Sw_timer_event_handler(0);
                }


            }
        }
        break;

    default:
        break;

    }
}


/*********************************************************************
 * @fn      BLECentral_processGATTMsg
 *
 * @brief   Process GATT messages and events.
 *
 * @return  none
 */
static void BLECentral_processGATTMsg(gattMsgEvent_t *pMsg)
{

//    uint8_t ret = 0;

    typedef union{
        uint32_t receivedUSSK;
        uint8_t usskArray[4];
    }ussk_UNION;

    ussk_UNION USSK_union;

    /* Reset member variable to 0 */
    USSK_union.receivedUSSK = 0;


    uint8_t i = 0;
    uint8_t j = 0;

  if (state == BLE_STATE_CONNECTED)
  {
	  Task_sleep(1000);
    // See if GATT server was unable to transmit an ATT response
    if (pMsg->hdr.status == blePending)
    {
      // No HCI buffer was available. App can try to retransmit the response
      // on the next connection event. Drop it for now.

    }
    else if ((pMsg->method == ATT_READ_RSP)   ||
             ((pMsg->method == ATT_ERROR_RSP) &&
              (pMsg->msg.errorRsp.reqOpcode == ATT_READ_REQ)))
    {
      if (pMsg->method == ATT_ERROR_RSP)
      {

        /* USSK Read failed, go back to scanning */

        /* reset app state */
        state = BLE_STATE_IDLE;

        /* reset scanning started flag */
        scanningStarted = FALSE;

        /* Set application's state to 'start discovery' */

//        app_state = APP_START_DISCOVERY;
//        Sw_timer_event_handler(0);

        /* switch LED off */
//        Util_stopClock(&led_Clock);
//        PIN_setOutputValue(led_pins_handle, Board_LED3, 0);

      }
      else
      {


          /* Store received USSK on global variable */
    	  if(isRegistered == false)
    	  {

    	  }
      }
      procedureInProgress = FALSE;
    }
    else if ((pMsg->method == ATT_WRITE_RSP)  ||
            (pMsg->method == ATT_WRITE_REQ)  ||
             ((pMsg->method == ATT_ERROR_RSP) &&
              (pMsg->msg.errorRsp.reqOpcode == ATT_WRITE_REQ)))
    {
      if (pMsg->method == ATT_ERROR_RSP)
      {

      }

      else if(pMsg->method == ATT_WRITE_RSP)
      {
        // After a successful write, display the value that was written and
        // increment value


		if(USSK_ack == TRUE)
		{

			USSK_ack = FALSE;
			/* Set application's state to 'terminate link' */

//			app_state = APP_TERMINATE_LINK;
//			Sw_timer_event_handler(0);
		}
		else


        if(first20BytesSent == true)
        {/*

          uint8_t status = 0;

           reset flag for next write
          first20BytesSent = false;

          // Do a write
            attWriteReq_t req;

             Allocate memory to send remaining 12 bytes and 1 registration byte, as first 20 bytes are sent
             * 13th byte is registration byte
            req.pValue = GATT_bm_alloc(connHandle, ATT_WRITE_REQ, 13 , NULL);


            if ( req.pValue != NULL )
            {


            	Task_sleep(5000);


                  if(isRegistered == true)
                  {
                  // send registration byte as ASCII '2' if device is registered
                      encryptedSecurityPacket[32] = 0x32;
                  }
                  else
                  {
                   // send registration byte as ASCII '1' if device is  not registered
                      encryptedSecurityPacket[32] = 0x31;
                  }


                   Copy data to be sent
                  memcpy(&(req.pValue[0]),&encryptedSecurityPacket[20],13);

                  req.handle = charHdl;
                  req.sig = 0;
                  req.cmd = 0;
                  req.len = 13;

                  status = GATT_WriteCharValue(connHandle, &req, selfEntity);

                  if ( status != SUCCESS )
                  {
                    GATT_bm_free((gattMsg_t *)&req, ATT_WRITE_REQ);

					app_state = APP_TERMINATE_LINK;
					Sw_timer_event_handler(0);
                  }
                  else
                  {
                       400 millisecond timeout clock start to disconnect if security packets doesnt match
//                      Util_restartClock(&OneShot_timer_Clock, ONESHOT_TIMER_DELAY);
                  }

            }

        */}

        procedureInProgress = FALSE;

      }

      else if(pMsg->method == ATT_WRITE_REQ){
      }

    }
    else if (pMsg->method == ATT_FLOW_CTRL_VIOLATED_EVENT)
    {
      // ATT request-response or indication-confirmation flow control is
      // violated. All subsequent ATT requests or indications will be dropped.
      // The app is informed in case it wants to drop the connection.


      // Display the opcode of the message that caused the violation.


    }
    else if (pMsg->method == ATT_MTU_UPDATED_EVENT)
    {
      // MTU size updated
    }
    else if (discState != BLE_DISC_STATE_IDLE)
    {
//      BLECentral_processGATTDiscEvent(pMsg);
    }
  } // else - in case a GATT message came after a connection has dropped, ignore it.

  // Needed only for ATT Protocol messages
  GATT_bm_free(&pMsg->msg, pMsg->method);
}

/*********************************************************************
 * @fn      BLECentral_processCmdCompleteEvt
 *
 * @brief   Process an incoming OSAL HCI Command Complete Event.
 *
 * @param   pMsg - message to process
 *
 * @return  none
 */
static void BLECentral_processCmdCompleteEvt(hciEvt_CmdComplete_t *pMsg)
{
    switch (pMsg->cmdOpcode)
  {
    case HCI_READ_RSSI:
      {
        int8 rssi = (int8)pMsg->pReturnParam[3];

      }
      break;

    default:
      break;
  }
}

/*********************************************************************
 * @fn      BLECentral_processStackMsg
 *
 * @brief   Process an incoming task message.
 *
 * @param   pMsg - message to process
 *
 * @return  none
 */
static void BLECentral_processStackMsg(ICall_Hdr *pMsg)
{

  switch (pMsg->event)
  {
    case GAP_MSG_EVENT:
      BLECentral_processRoleEvent((gapCentralRoleEvent_t *)pMsg);
      break;

    case GATT_MSG_EVENT:
      BLECentral_processGATTMsg((gattMsgEvent_t *)pMsg);
      break;

    case HCI_GAP_EVENT_EVENT:
      {
        // Process HCI message
        switch(pMsg->status)
        {
          case HCI_COMMAND_COMPLETE_EVENT_CODE:
            BLECentral_processCmdCompleteEvt((hciEvt_CmdComplete_t *)pMsg);
            break;

          case HCI_EXT_DECRYPT_EVENT:

            break;

          case HCI_VE_EVENT_CODE:
            /* Process HCI vendor specific event */
            ProcessHCICmdCompleteEvt((hciEvt_CmdComplete_t *)pMsg);
            break;
          default:

            break;
        }
      }
      break;
    case HCI_SMP_EVENT_EVENT:
    {
        /* Process HCI smp event */
        ProcessHCICmdCompleteEvt((hciEvt_CmdComplete_t *)pMsg);
    }
        break;
    default:
      break;
  }



}

/*********************************************************************
 * @fn      BLECentral_findSvcUuid
 *
 * @brief   Find a given UUID in an advertiser's service UUID list.
 *
 * @return  TRUE if service UUID found
 */
static bool BLECentral_findSvcUuid(uint16_t uuid, uint8_t *pData,
                                         uint8_t dataLen)
{
  uint8_t adLen;
  uint8_t adType;
  uint8_t *pEnd;

  pEnd = pData + dataLen - 1;

  // While end of data not reached
  while (pData < pEnd)
  {
    // Get length of next AD item
    adLen = *pData++;
    if (adLen > 0)
    {
      adType = *pData;

      // If AD type is for 16-bit service UUID
      if ((adType == GAP_ADTYPE_16BIT_MORE) ||
          (adType == GAP_ADTYPE_16BIT_COMPLETE))
      {
        pData++;
        adLen--;

        // For each UUID in list
        while (adLen >= 2 && pData < pEnd)
        {
          // Check for match
          if ((pData[0] == LO_UINT16(uuid)) && (pData[1] == HI_UINT16(uuid)))
          {
            // Match found
            return TRUE;
          }

          // Go to next
          pData += 2;
          adLen -= 2;
        }

        // Handle possible erroneous extra byte in UUID list
        if (adLen == 1)
        {
          pData++;
        }
      }
      else
      {
        // Go to next item
        pData += adLen;
      }
    }
  }

  // Match not found
  return FALSE;
}

/*********************************************************************
 * @fn      scanForGsk
 *
 * @brief   Scan Advertising packet for GSK.
 *
 * @param   pEvent - pointer to event structure
 *
 * @return  TRUE  : GSK matched
 * 			FALSE : GSK not matched
 */
static uint8_t scanForGsk(gapCentralRoleEvent_t *pEvent)
{


    uint8 currentDataLen;
    uint8 firstByte;
    uint8 i=0;
    uint8 j=0;
    uint8 AdvPacket[KEYLEN] = {0};
    uint8 GSK_original[] = "fbf0"; // GSK predefined
    uint32 match_gsk;

    /* complete list 128-bit Service UUID field,starting with 0x07 */
    while(j<=2){

        currentDataLen = (pEvent->deviceInfo).pEvtData[i];
        firstByte = (pEvent->deviceInfo).pEvtData[i+1];

        /* 128 bit Service UUID */
        if(firstByte != 0x07)
            i = i+currentDataLen+1;

        /* packet with 0x07(128 bit Service UUID) found */
        else{

            /* Copy originally received adv data into AdvPacket buffer */
            memcpy(&AdvPacket[0],&((pEvent->deviceInfo).pEvtData[i+2]), KEYLEN);



            break;
        }

        /* increment iterator */
        j++;

    }

    /* relevant field not found in packet */
    if(j>2)
        return FAILURE;
    /* relevant field found in packet */
//    else
//        return SUCCESS;

//////////////////////////// find GSK in advertising packet  /////////////////////////////////////////
    uint8 gskIndexAscii=0;
    uint8 gskIndexHex=0;
    uint32_t receivedGSK=0;

    ReverseBytes(&AdvPacket[0], KEYLEN);


    /* Check for gsk index in advrtising packet */
    /* 14th index has gsk index */
    gskIndexAscii = AdvPacket[14];

    /* Calculating index in Hex */
    gskIndexHex = atoi((const char *)&gskIndexAscii);

    /* store gsk in local variable */
    memcpy(&receivedGSK, &AdvPacket[gskIndexHex], 4);
    memcpy(&match_gsk, &GSK_original[0], 4);

    /* if true add the current device */
    if(receivedGSK == match_gsk)
    {
    	GAPCentralRole_CancelDiscovery();
        /* if gsk matched, print on screen */

         return true; // return true if GSK matched


    }
    else
    {

//
//        /* Set application's state to 'start discovery' */

        return false; // return false if GSK not matched
    }


//    return;
}
static uint8_t decryptAdvPacket(gapCentralRoleEvent_t *pEvent)
{
    uint8 currentDataLen;
    uint8 firstByte;
    uint8 i=0;
    uint8 j=0;
    uint8 reversedEncryptedPacket[KEYLEN] = {0};


    /* Decrypt complete list 128-bit Service UUID field,starting with 0x07 */
    while(j<=2){

        currentDataLen = (pEvent->deviceInfo).pEvtData[i];
        firstByte = (pEvent->deviceInfo).pEvtData[i+1];

        /* 128 bit Service UUID */
        if(firstByte != 0x07)
            i = i+currentDataLen+1;

        /* packet with 0x07(128 bit Service UUID) found */
        else{

            /* Copy originally received encrypted data into reversedEncryptedPacket buffer */
            memcpy(&reversedEncryptedPacket[0],&((pEvent->deviceInfo).pEvtData[i+2]), KEYLEN);

            /* Reverse received encrypted data bytes */
//            ReverseBytes(&reversedEncryptedPacket[0], KEYLEN);

            /* Call HCI_EXT vendor specific decryption API
             * send key, and address of field with 128bit Service UUID */
            HCI_EXT_DecryptCmd(&aesKey[0], &reversedEncryptedPacket[0]);


            break;
        }

        /* increment iterator */
        j++;

    }

    /* relevant field not found in packet */
    if(j>2)
        return FAILURE;
    /* relevant field found in packet */
    else
        return SUCCESS;

}

/*********************************************************************
 * @fn      BLECentral_processRoleEvent
 *
 * @brief   Central role event processing function.
 *
 * @param   pEvent - pointer to event structure
 *
 * @return  none
 */
static void BLECentral_processRoleEvent(gapCentralRoleEvent_t *pEvent)
{

  switch (pEvent->gap.opcode)
  {

    case GAP_DEVICE_INIT_DONE_EVENT:
      {
        maxPduSize = pEvent->initDone.dataPktLen;
	    System_printf("Device init done \n ");
	    System_flush();


      }
      break;

    case GAP_DEVICE_INFO_EVENT:
      {

    	    System_printf("Scan response received %s\n ",Util_convertBdAddr2Str(pEvent->deviceInfo.addr));
    	    System_flush();
    	    System_printf("------Received data\n");
    	    int i;
    	    for( i = 0; i<(pEvent->deviceInfo.dataLen); i++)
    	    {

    	    	 System_printf("%x\t",pEvent->deviceInfo.pEvtData[i]);

    	    }
    	    System_printf("\n-------Received data end\n");
    	    isRegistered = true;
        // if filtering device discovery results based on service UUID
        if (DEFAULT_DEV_DISC_BY_SVC_UUID == TRUE)
        {

        /* OPTION 2 implementation */
        if(isRegistered == true){

                if (BLECentral_findSvcUuid(GATTPROFILE_SERV_UUID,
                                            pEvent->deviceInfo.pEvtData,
                                            pEvent->deviceInfo.dataLen))
                {
                    /* print debug string */
                    /*current_ticks = Clock_getTicks();

                    /* decrypt advertisement packet */
                	 System_printf("@@ Decrypt avd data\n");
                    if(decryptAdvPacket(pEvent) == SUCCESS)
                    {
//                    	PIN_setOutputValue(led_pins_handle, Board_LED1, 1);
                            /* store device address and addr type for connection */

                    	devAddrType = pEvent->deviceInfo.addrType;
                        memcpy(devAddr,pEvent->deviceInfo.addr,B_ADDR_LEN);
                    }
                    else
                    {

                    }

                }
        }
        else{


                if (BLECentral_findSvcUuid(GATTPROFILE_SERV_UUID,
                                           pEvent->deviceInfo.pEvtData,
                                           pEvent->deviceInfo.dataLen))
                {
                    /* print debug string */


                    /* Print timestamps on LCD */

                    if(scanForGsk(pEvent) == true)
                    {


                    BLECentral_addDeviceInfo(pEvent->deviceInfo.addr,
                                           pEvent->deviceInfo.addrType);


                    }

                }
        }


        }
      }
      break;

    case GAP_DEVICE_DISCOVERY_EVENT:
      {
        // discovery complete


  	    System_printf("discovery event complete \n ");
  	    System_flush();
        // if not filtering device discovery results based on service UUID
        if (DEFAULT_DEV_DISC_BY_SVC_UUID == FALSE)
        {
          // Copy results
          scanRes = pEvent->discCmpl.numDevs;
          memcpy(devList, pEvent->discCmpl.pDevList,
                 (sizeof(gapDevRec_t) * scanRes));
        }


        if (scanRes > 0)
        {


            /* Stop Scanning */

          /* Set application's state to 'establish link' */
//        	if(isRegistered)
        	{

			  app_state = APP_ESTABLISH_LINK;
			  Sw_timer_event_handler(0);
        	}

        }
        else
        {
        	 PIN_setOutputValue(led_pins_handle, Board_LED1, 1);
        	 Task_sleep(1000);
        	 PIN_setOutputValue(led_pins_handle, Board_LED1, 0);

            app_state = APP_START_DISCOVERY;
            Sw_timer_event_handler(0);

        }

        /* if no device found (scanRes = 0), keep discovering */

        // initialize scan index to last device
        scanIdx = scanRes;

        /* scanningStarted flag is reset */
        scanningStarted = FALSE;
      }
      break;

    case GAP_LINK_ESTABLISHED_EVENT:
      {

    	    System_printf("\n link established....\n ");
    	    System_flush();
          /* Link established successfully */
        if (pEvent->gap.hdr.status == SUCCESS)
        {
        	PIN_setOutputValue(led_pins_handle, Board_LED1, 1);

        	state = BLE_STATE_CONNECTED;
        				  preparePacketFlag = true;
        	              app_state = APP_PREPARE_PACKET;
        	              Sw_timer_event_handler(0);

          if(isRegistered == TRUE)
          {

              /* Set preparePacketFlag flag to prepare packet to be written on App's characteristic */
              preparePacketFlag = true;



          }
          else
          {

          }




        }

        /* Link couldn't be established */
        else
        {
          state = BLE_STATE_IDLE;
          connHandle = GAP_CONNHANDLE_INIT;
          discState = BLE_DISC_STATE_IDLE;

          /* Set application's state to 'start discovery' */

//          app_state = APP_START_DISCOVERY;
//          Sw_timer_event_handler(0);


        }
      }

      break;

    case GAP_LINK_TERMINATED_EVENT:
      {
    	    System_printf("link terminated \n ");
    	    System_flush();

  		if(USSK_ack == TRUE)
  		{

  			USSK_ack = FALSE;

  		}
        /* timeout for terminate event */
    	chck_conn_state = 0;
//        Util_stopClock(&OneShot_timer_Clock);

        state = BLE_STATE_IDLE;
        connHandle = GAP_CONNHANDLE_INIT;
        discState = BLE_DISC_STATE_IDLE;
        charHdl = 0;
        procedureInProgress = FALSE;

        PIN_setOutputValue(led_pins_handle, Board_LED2, 0);
        /* Set application's state to 'start discovery' */

//        Task_sleep(5000);
        app_state = APP_START_DISCOVERY;
        Sw_timer_event_handler(0);


      }
      break;

    case GAP_AUTHENTICATION_COMPLETE_EVENT:
    {


    }
    break;
    case GAP_BOND_COMPLETE_EVENT:
      {

      }
      break;
    case  GAP_SLAVE_REQUESTED_SECURITY_EVENT:
    {


    }
    break;

    case GAP_LINK_PARAM_UPDATE_EVENT:

    {

    }
    break;

    default:
      break;
  }

}

/*********************************************************************
 * @fn      BLECentral_startDiscovery
 *
 * @brief   Start service discovery.
 *
 * @return  none
 */
static void BLECentral_startDiscovery(void)
{
//  attExchangeMTUReq_t req;
//  uint8_t status;
  // Initialize cached handles
  svcStartHdl = svcEndHdl = charHdl = 0;


  uint8_t uuid[ATT_BT_UUID_SIZE] = { LO_UINT16(GATTPROFILE_SERV_UUID),
                                     HI_UINT16(GATTPROFILE_SERV_UUID) };

  discState = BLE_DISC_STATE_SVC;

  // Discovery simple BLE service
  GATT_DiscPrimaryServiceByUUID(connHandle, uuid, ATT_BT_UUID_SIZE,
                                     selfEntity);


}

/*********************************************************************
 * @fn      BLECentral_addDeviceInfo
 *
 * @brief   Add a device to the device discovery result list
 *
 * @return  none
 */
static void BLECentral_addDeviceInfo(uint8_t *pAddr, uint8_t addrType)
{
  uint8_t i;

  // If result count not at max
  if (scanRes < DEFAULT_MAX_SCAN_RES)
  {
    // Check if device is already in scan results
    for (i = 0; i < scanRes; i++)
    {
      if (memcmp(pAddr, devList[i].addr , B_ADDR_LEN) == 0)
      {
        return;
      }
    }

    // Add addr to scan result list
    memcpy(devList[scanRes].addr, pAddr, B_ADDR_LEN);
    devList[scanRes].addrType = addrType;

    // Increment scan result count
    scanRes++;
  }
}


/*********************************************************************
 * @fn      BLECentral_enqueueMsg
 *
 * @brief   Creates a message and puts the message in RTOS queue.
 *
 * @param   event - message event.
 * @param   state - message state.
 * @param   pData - message data pointer.
 *
 * @return  TRUE or FALSE
 */
static uint8_t BLECentral_enqueueMsg(uint8_t event, uint8_t state,
                                           uint8_t *pData)
{
  sbcEvt_t *pMsg = ICall_malloc(sizeof(sbcEvt_t));

  // Create dynamic pointer to message.
  if (pMsg)
  {
    pMsg->hdr.event = event;
    pMsg->hdr.state = state;
    pMsg->pData = pData;

    // Enqueue the message.
    return Util_enqueueMsg(appMsgQueue, sem, (uint8_t *)pMsg);
  }

  return FALSE;
}
/*********************************************************************
 * @fn      BLECentral_eventCB
 *
 * @brief   Central event callback function.
 *
 * @param   pEvent - pointer to event structure
 *
 * @return  TRUE if safe to deallocate event message, FALSE otherwise.
 */
static uint8_t BLECentral_eventCB(gapCentralRoleEvent_t *pEvent)
{
  // Forward the role event to the application
  if (BLECentral_enqueueMsg(SBC_STATE_CHANGE_EVT,
                                  SUCCESS, (uint8_t *)pEvent))
  {
    // App will process and free the event
    return FALSE;
  }

  // Caller should free the event
  return TRUE;
}

/*********************************************************************
 * @fn      BLECentral_pairStateCB
 *
 * @brief   Pairing state callback.
 *
 * @return  none
 */
static void BLECentral_pairStateCB(uint16_t connHandle, uint8_t state,
                                         uint8_t status)
{

  uint8_t *pData;

  // Allocate space for the event data.
  if ((pData = ICall_malloc(sizeof(uint8_t))))
  {
    *pData = status;

    // Queue the event.
    BLECentral_enqueueMsg(SBC_PAIRING_STATE_EVT, state, pData);
  }
}

/*********************************************************************
 * @fn      BLECentral_startDiscHandler
 *
 * @brief   Clock handler function
 *
 * @param   a0 - ignored
 *
 * @return  none
 */
void BLECentral_startDiscHandler(UArg a0)
{
  events |= SBC_START_DISCOVERY_EVT;

  // Wake up the application thread when it waits for clock event
  Semaphore_post(sem);
}

