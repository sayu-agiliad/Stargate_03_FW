/**************************************************************************************************
  Filename:       GattGATTprofile.c
  Revised:        $Date: 2015-07-20 11:31:07 -0700 (Mon, 20 Jul 2015) $
  Revision:       $Revision: 44370 $

  Description:    This file contains the Gatt GATT profile sample GATT service
                  profile for use with the BLE sample application.

  Copyright 2010 - 2015 Texas Instruments Incorporated. All rights reserved.

  IMPORTANT: Your use of this Software is limited to those specific rights
  granted under the terms of a software license agreement between the user
  who downloaded the software, his/her employer (which must be your employer)
  and Texas Instruments Incorporated (the "License").  You may not use this
  Software unless you agree to abide by the terms of the License. The License
  limits your use, and you acknowledge, that the Software may not be modified,
  copied or distributed unless embedded on a Texas Instruments microcontroller
  or used solely and exclusively in conjunction with a Texas Instruments radio
  frequency transceiver, which is integrated into your product.  Other than for
  the foregoing purpose, you may not use, reproduce, copy, prepare derivative
  works of, modify, distribute, perform, display or sell this Software and/or
  its documentation for any purpose.

  YOU FURTHER ACKNOWLEDGE AND AGREE THAT THE SOFTWARE AND DOCUMENTATION ARE
  PROVIDED “AS IS” WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
  INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY, TITLE, 
  NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL
  TEXAS INSTRUMENTS OR ITS LICENSORS BE LIABLE OR OBLIGATED UNDER CONTRACT,
  NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR OTHER
  LEGAL EQUITABLE THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES
  INCLUDING BUT NOT LIMITED TO ANY INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE
  OR CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF PROCUREMENT
  OF SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY THIRD PARTIES
  (INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.

  Should you have any questions regarding your right to use this Software,
  contact Texas Instruments Incorporated at www.TI.com. 
**************************************************************************************************/

/*********************************************************************
 * INCLUDES
 */
#include "GATTprofile.h"

#include <string.h>

#include "bcomdef.h"
#include "OSAL.h"
#include "linkdb.h"
#include "att.h"
#include "gatt.h"
#include "gatt_uuid.h"
#include "gattservapp.h"
#include "gapbondmgr.h"
#include "hci_tl.h"
#include "util.h"
#include <ti/sysbios/knl/Task.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Types.h>
#include <xdc/runtime/Timestamp.h>
#include <ti/sysbios/knl/Clock.h>
#include "Board.h"
#include "board_lcd.h"
#include <ti/drivers/lcd/LCDDogm1286.h>
#include "central.h"
/*********************************************************************
 * MACROS
 */
#define GattPROFILE_SERV_UUID             0xFEDC

/*********************************************************************
 * CONSTANTS
 */

#define SERVAPP_NUM_ATTR_SUPPORTED        17

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */
#define SLEEP_TIME            			    10000
//extern uint8 aesKey[16];
// encrypted data received from app
extern uint8_t rcvdEncryptedData[KEYLEN];
extern void Sw_timer_event_handler(UArg val);
//extern uint16_t on_time = 200,off_time = 2000;
//extern bool verifyRcvdPacketFlag;
extern bool isRegistered;
extern PIN_Handle led_pins_handle;
uint8 notify_length = 0;
extern bool receiveEncryptedPcktFlag;
extern bool procedureInProgress;
extern uint8_t app_state;
extern uint8_t read_ussk;
extern uint8 aesKey[16];
//extern uint32_t originalUSSK;
extern uint8_t originalUSSK_len;
extern gapAuthParams_t params;

void Initiate_pair(void);
enum
{
    APP_START_DISCOVERY=1, /* set first state to 1 */
    APP_ESTABLISH_LINK,
    APP_USSK_READ,
	APP_PREPARE_PACKET,
	APP_ENCRYPT_PACKET,
	APP_WRITE_PACKET,
	APP_RECEIVE_ENCRYPTED_PACKET,
	APP_VERIFY_RCVD_PACKET,
    APP_TERMINATE_LINK,
	APP_INITIATE_PAIR,
};
/* LED on-time off-time */
uint16_t on_time = 1,off_time = 2000;

// one shot timer for connection / packet verification timeout
Clock_Struct OneShot_timer_Clock;
// Clock object used for blinking LED
Clock_Struct led_Clock;
Clock_Struct startDiscClock;


// Gatt GATT Profile Service UUID: 0xFFF0
CONST uint8 GattProfileServUUID[ATT_BT_UUID_SIZE] =
{ 
  LO_UINT16(GattPROFILE_SERV_UUID), HI_UINT16(GattPROFILE_SERV_UUID)
};

// Characteristic 1 UUID: 0xFFF1
CONST uint8 GattProfilechar1UUID[ATT_BT_UUID_SIZE] =
{ 
  LO_UINT16(GATTPROFILE_CHAR1_UUID), HI_UINT16(GATTPROFILE_CHAR1_UUID)
};


/*********************************************************************
 * EXTERNAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL VARIABLES
 */

static GattProfileCBs_t *GattProfile_AppCBs = NULL;

/*********************************************************************
 * Profile Attributes - variables
 */

// Gatt Profile Service attribute
static CONST gattAttrType_t GattProfileService = { ATT_BT_UUID_SIZE, GattProfileServUUID };


// Gatt Profile Characteristic 1 Properties
static uint8 GattProfileChar1Props = GATT_PROP_READ | GATT_PROP_WRITE| GATT_PROP_WRITE_NO_RSP|GATT_PROP_NOTIFY;

// Characteristic 1 Value
static uint8 GattProfileChar1[21] = {0}; //[50] = {0};

// Gatt Profile Characteristic 1 User Description
static uint8 GattProfileChar1UserDesp[17] = "Characteristic 1";

static gattCharCfg_t *GattProfileChar4Config;

/*********************************************************************
 * Profile Attributes - Table
 */

/*static*/ gattAttribute_t GattProfileAttrTbl[5] =
{
  // Gatt Profile Service
  { 
    { ATT_BT_UUID_SIZE, primaryServiceUUID }, /* type */
    GATT_PERMIT_READ,                         /* permissions */
    0,                                        /* handle */
    (uint8 *)&GattProfileService            /* pValue */
  },

    // Characteristic 1 Declaration
    { 
      { ATT_BT_UUID_SIZE, characterUUID },
      GATT_PERMIT_READ, 
      0,
      &GattProfileChar1Props
    },

      // Characteristic Value 1
      { 
        { ATT_BT_UUID_SIZE, GattProfilechar1UUID },
        GATT_PERMIT_READ | GATT_PERMIT_WRITE ,
        0, 
        &GattProfileChar1[0]
      },

      // Characteristic 4 configuration
      {
        { ATT_BT_UUID_SIZE, clientCharCfgUUID },
        GATT_PERMIT_READ | GATT_PERMIT_WRITE,
        0,
        (uint8 *)&GattProfileChar4Config
      },

      // Characteristic 1 User Description
      { 
        { ATT_BT_UUID_SIZE, charUserDescUUID },
        GATT_PERMIT_READ ,
        0, 
		GattProfileChar1UserDesp
      },      


};

/*********************************************************************
 * LOCAL FUNCTIONS
 */
static bStatus_t GattProfile_ReadAttrCB(uint16_t connHandle,
                                          gattAttribute_t *pAttr, 
                                          uint8_t *pValue, uint16_t *pLen,
                                          uint16_t offset, uint16_t maxLen,
                                          uint8_t method);
static bStatus_t GattProfile_WriteAttrCB(uint16_t connHandle,
                                           gattAttribute_t *pAttr,
                                           uint8_t *pValue, uint16_t len,
                                           uint16_t offset, uint8_t method);

/*********************************************************************
 * PROFILE CALLBACKS
 */
// Gatt Profile Service Callbacks
CONST gattServiceCBs_t GattProfileCBs =
{
  GattProfile_ReadAttrCB,  // Read callback function pointer
  GattProfile_WriteAttrCB, // Write callback function pointer
  NULL                       // Authorization callback function pointer
};

/*********************************************************************
 * PUBLIC FUNCTIONS
 */

/*********************************************************************
 * @fn      GattProfile_AddService
 *
 * @brief   Initializes the Gatt Profile service by registering
 *          GATT attributes with the GATT server.
 *
 * @param   services - services to add. This is a bit map and can
 *                     contain more than one service.
 *
 * @return  Success or Failure
 */
bStatus_t GattProfile_AddService( uint32 services )
{
  uint8 status;

   //Allocate Client Characteristic Configuration table
  GattProfileChar4Config = (gattCharCfg_t *)ICall_malloc( sizeof(gattCharCfg_t) *
                                                            linkDBNumConns );
  if ( GattProfileChar4Config == NULL )
  {
    return ( bleMemAllocError );
  }

  // Initialize Client Characteristic Configuration attributes
  GATTServApp_InitCharCfg( INVALID_CONNHANDLE, GattProfileChar4Config );

  if ( services & GATTPROFILE_SERVICE )
  {
    // Register GATT attribute list and CBs with GATT Server App
    status = GATTServApp_RegisterService( GattProfileAttrTbl,
                                          GATT_NUM_ATTRS( GattProfileAttrTbl ),
                                          GATT_MAX_ENCRYPT_KEY_SIZE,
                                          &GattProfileCBs );
  }
  else
  {
    status = SUCCESS;
  }

  return ( status );
}

/*********************************************************************
 * @fn      GattProfile_RegisterAppCBs
 *
 * @brief   Registers the application callback function. Only call 
 *          this function once.
 *
 * @param   callbacks - pointer to application callbacks.
 *
 * @return  SUCCESS or bleAlreadyInRequestedMode
 */
bStatus_t GattProfile_RegisterAppCBs( GattProfileCBs_t *appCallbacks )
{
  if ( appCallbacks )
  {
    GattProfile_AppCBs = appCallbacks;
    
    return ( SUCCESS );
  }
  else
  {
    return ( bleAlreadyInRequestedMode );
  }
}

/*********************************************************************
 * @fn      GattProfile_SetParameter
 *
 * @brief   Set a Gatt Profile parameter.
 *
 * @param   param - Profile parameter ID
 * @param   len - length of data to write
 * @param   value - pointer to data to write.  This is dependent on
 *          the parameter ID and WILL be cast to the appropriate 
 *          data type (example: data type of uint16 will be cast to 
 *          uint16 pointer).
 *
 * @return  bStatus_t
 */
bStatus_t GattProfile_SetParameter( uint8 param, uint8 len, void *value )
{
  bStatus_t ret = SUCCESS;
  switch ( param )
  {
    case GATTPROFILE_CHAR1:
//      if ( len == sizeof ( uint8 ) )
      {
        memcpy(&GattProfileChar1[0], value, len);
        notify_length = len;
//		GattProfileChar1 = *((uint8*)value);
		GATTServApp_ProcessCharCfg( GattProfileChar4Config, GattProfileChar1, FALSE,
                                            GattProfileAttrTbl, GATT_NUM_ATTRS( GattProfileAttrTbl ),
                                            INVALID_TASK_ID, GattProfile_ReadAttrCB );
      }
//      else
      {
//        ret = bleInvalidRange;
      }
      break;



    default:
      ret = INVALIDPARAMETER;
      break;
  }
  
  return ( ret );
}

/*********************************************************************
 * @fn      GattProfile_GetParameter
 *
 * @brief   Get a Gatt Profile parameter.
 *
 * @param   param - Profile parameter ID
 * @param   value - pointer to data to put.  This is dependent on
 *          the parameter ID and WILL be cast to the appropriate 
 *          data type (example: data type of uint16 will be cast to 
 *          uint16 pointer).
 *
 * @return  bStatus_t
 */
bStatus_t GattProfile_GetParameter( uint8 param, void *value )
{
  bStatus_t ret = SUCCESS;
  switch ( param )
  {
    case GATTPROFILE_CHAR1:

    	memcpy(value, GattProfileChar1, 20);
//      *((uint8*)value) = GattProfileChar1;
      break;


    default:
      ret = INVALIDPARAMETER;
      break;
  }
  
  return ( ret );
}

/*********************************************************************
 * @fn          GattProfile_ReadAttrCB
 *
 * @brief       Read an attribute.
 *
 * @param       connHandle - connection message was received on
 * @param       pAttr - pointer to attribute
 * @param       pValue - pointer to data to be read
 * @param       pLen - length of data to be read
 * @param       offset - offset of the first octet to be read
 * @param       maxLen - maximum length of data to be read
 * @param       method - type of read message
 *
 * @return      SUCCESS, blePending or Failure
 */
static bStatus_t GattProfile_ReadAttrCB(uint16_t connHandle,
                                          gattAttribute_t *pAttr,
                                          uint8_t *pValue, uint16_t *pLen,
                                          uint16_t offset, uint16_t maxLen,
                                          uint8_t method)
{
  bStatus_t status = SUCCESS;

  // If attribute permissions require authorization to read, return error
  if ( gattPermitAuthorRead( pAttr->permissions ) )
  {
    // Insufficient authorization
    return ( ATT_ERR_INSUFFICIENT_AUTHOR );
  }
  
  // Make sure it's not a blob operation (no attributes in the profile are long)
  if ( offset > 0 )
  {
    return ( ATT_ERR_ATTR_NOT_LONG );
  }
 
  if ( pAttr->type.len == ATT_BT_UUID_SIZE )
  {
    // 16-bit UUID
    uint16 uuid = BUILD_UINT16( pAttr->type.uuid[0], pAttr->type.uuid[1]);
    switch ( uuid )
    {

      case GATTPROFILE_CHAR1_UUID:
//      case GattPROFILE_CHAR2_UUID:
//      case GattPROFILE_CHAR4_UUID:
        *pLen = notify_length;
        pValue[0] = *pAttr->pValue;

        memcpy(&pValue[0],&pAttr->pValue[0], notify_length);
        break;


      default:
        // Should never get here! (characteristics 3 and 4 do not have read permissions)
        *pLen = 0;
        status = ATT_ERR_ATTR_NOT_FOUND;
        break;
    }
  }
  else
  {
    // 128-bit UUID
    *pLen = 0;
    status = ATT_ERR_INVALID_HANDLE;
  }

  return ( status );
}

/*********************************************************************
 * @fn      GattProfile_WriteAttrCB
 *
 * @brief   Validate attribute data prior to a write operation
 *
 * @param   connHandle - connection message was received on
 * @param   pAttr - pointer to attribute
 * @param   pValue - pointer to data to be written
 * @param   len - length of data
 * @param   offset - offset of the first octet to be written
 * @param   method - type of write message
 *
 * @return  SUCCESS, blePending or Failure
 */
static bStatus_t GattProfile_WriteAttrCB(uint16_t connHandle,
                                           gattAttribute_t *pAttr,
                                           uint8_t *pValue, uint16_t len,
                                           uint16_t offset, uint8_t method)
{
  bStatus_t status = SUCCESS;
  uint8 notifyApp = 0xFF;
//  uint8 rcvd_arr[16];
//  uint8 i = 0, j = 0;
  typedef union{
      uint32_t receivedUSSK;
      uint8_t usskArray[4];
  }ussk_UNION;

//  ussk_UNION USSK_union;

  // If attribute permissions require authorization to write, return error
  if ( gattPermitAuthorWrite( pAttr->permissions ) )
  {
    // Insufficient authorization
    return ( ATT_ERR_INSUFFICIENT_AUTHOR );
  }
  
  if ( pAttr->type.len == ATT_BT_UUID_SIZE )
  {
    // 16-bit UUID
    uint16 uuid = BUILD_UINT16( pAttr->type.uuid[0], pAttr->type.uuid[1]);
    switch ( uuid )
    {
      case GATTPROFILE_CHAR1_UUID:

          /* Store received USSK on global variable */
    	  if(isRegistered == true && len == 20)
			{

			  memcpy(&rcvdEncryptedData[0],&pValue[0],16);
			  /* Set flag to enter APP_VERIFY_RCVD_PACKET state */
			  receiveEncryptedPcktFlag = true;
			  /* Update application's state */

			  app_state = APP_RECEIVE_ENCRYPTED_PACKET;
			  Sw_timer_event_handler(0);
			}
    	  else
    	  if(isRegistered == false)
    	  {

    	  }

//    	  else
          if(len == 4) // write received on FFF1, when device selected from list
          {


              if((pValue[0] == 0x30 && pValue[1] == 0x31 && pValue[2] == 0x30 && pValue[3] == 0x31) && (isRegistered == false)) // 0101 from App
			  {
            	  // start led blinking when device selected from list in App


            	  Task_sleep(2000);
				  on_time = 100; off_time = 500;
				  Util_startClock(&led_Clock);
			  }
              else
              if((pValue[0] == 0x30 && pValue[1] == 0x31 && pValue[2] == 0x30 && pValue[3] == 0x32) && (isRegistered == false)) // 0102 from App
              {
            	  // stop led blinking when device is not confirmed or command received from App

            	  Task_sleep(2000);
				  Util_stopClock(&led_Clock);
//				  PIN_setOutputValue(led_pins_handle, Board_LED3, 0);
              }

              else
              if(pValue[0] == 0x30 && pValue[1] == 0x31 && pValue[2] == 0x30 && pValue[3] == 0x34) // 0103 from App
              {
            	  // stop led blinking when device is confirmed or Terminate command received from App

            	  Task_sleep(1000);
                  app_state = APP_TERMINATE_LINK;
                  Sw_timer_event_handler(0);

            	  Util_stopClock(&led_Clock);
//            	  PIN_setOutputValue(led_pins_handle, Board_LED3, 0);



              }


          }

    	 break;



      case GATT_CLIENT_CHAR_CFG_UUID:
        status = GATTServApp_ProcessCCCWriteReq( connHandle, pAttr, pValue, len,
                                                 offset, GATT_CLIENT_CFG_NOTIFY );
        break;
        
      default:
        // Should never get here! (characteristics 2 and 4 do not have write permissions)
        status = ATT_ERR_ATTR_NOT_FOUND;
        break;
    }
  }
  else
  {
    // 128-bit UUID
    status = ATT_ERR_INVALID_HANDLE;
  }

  // If a characteristic value changed then callback function to notify application of change
  if ( (notifyApp != 0xFF ) && GattProfile_AppCBs && GattProfile_AppCBs->pfnGattProfileChange )
  {
    GattProfile_AppCBs->pfnGattProfileChange( notifyApp );
  }
  
  return ( status );
}

/*********************************************************************
*********************************************************************/
