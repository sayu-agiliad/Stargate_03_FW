/*
 * ble_mngr.h
 *
 */

#ifndef  BLE_MNGR_H_
#define  BLE_MNGR_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Clock.h>

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "hci_tl.h"
#include "linkdb.h"
#include "gatt.h"
#include "gapgattserver.h"
#include "gattservapp.h"
#include "central.h"
#include "gapbondmgr.h"
#include "osal_snv.h"
#include "hci.h"
#include "ICallBleAPIMSG.h"

#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Queue.h>

#include <xdc/runtime/System.h>
#include <xdc/runtime/Types.h>
#include <xdc/runtime/Timestamp.h>
#include <ti/sysbios/knl/Clock.h>
//#include <std.h>

#include "util.h"
#include "Board.h"
#include "bleUserConfig.h"

#define SBC_START_DISCOVERY_EVT               0x0001
#define SBC_PAIRING_STATE_EVT                 0x0002
#define SBC_RSSI_READ_EVT                     0x0008
#define SBC_STATE_CHANGE_EVT                  0x0020



#define SBC_PERIODIC_EVENT                     0x0040

#define SBC_PACKET_VERIFY_EVENT          	   0x0080

#define SBC_LED_BLINK_EVENT                    0x0100

#define HCI_OPCODE_SIZE                       0x02


// Maximum number of scan responses
#define DEFAULT_MAX_SCAN_RES                  100/*8*//*100*/

// Scan duration in ms
#define DEFAULT_SCAN_DURATION                 /*4000*/6000 /*30*/

/* scan window duration */
#define SCAN_WINDOW_DURATION                  53/* 33msec */   //80

/* scan interval duration */
#define SCAN_INTERVAL_DURATION                9547/* 300msec */  //480// 9600*0.625 = 6sec


// Discovery mode (limited, general, all)
#define DEFAULT_DISCOVERY_MODE                DEVDISC_MODE_ALL

// TRUE to use active scan
#define DEFAULT_DISCOVERY_ACTIVE_SCAN         FALSE


// TRUE to use white list during discovery
#define DEFAULT_DISCOVERY_WHITE_LIST          FALSE

// TRUE to use high scan duty cycle when creating link
#define DEFAULT_LINK_HIGH_DUTY_CYCLE          FALSE

// TRUE to use white list when creating link
#define DEFAULT_LINK_WHITE_LIST               FALSE

// Default RSSI polling period in ms
#define DEFAULT_RSSI_PERIOD                   1000

// Whether to enable automatic parameter update request when a connection is
// formed
#define DEFAULT_ENABLE_UPDATE_REQUEST         FALSE

// Minimum connection interval (units of 1.25ms) if automatic parameter update
// request is enabled
#define DEFAULT_UPDATE_MIN_CONN_INTERVAL      12

// Maximum connection interval (units of 1.25ms) if automatic parameter update
// request is enabled
#define DEFAULT_UPDATE_MAX_CONN_INTERVAL      12

// Slave latency to use if automatic parameter update request is enabled
#define DEFAULT_UPDATE_SLAVE_LATENCY          0

// Supervision timeout value (units of 10ms) if automatic parameter update
// request is enabled
#define DEFAULT_UPDATE_CONN_TIMEOUT           3000

// Default passcode
#define DEFAULT_PASSCODE                      19655

// Default GAP pairing mode
#define DEFAULT_PAIRING_MODE                  GAPBOND_PAIRING_MODE_INITIATE /*GAPBOND_PAIRING_MODE_WAIT_FOR_REQ*/

// Default MITM mode (TRUE to require passcode or OOB when pairing)
#define DEFAULT_MITM_MODE                     FALSE

// Default bonding mode, TRUE to bond
#define DEFAULT_BONDING_MODE                   TRUE/*FALSE*/

// Default GAP bonding I/O capabilities
#define DEFAULT_IO_CAPABILITIES               GAPBOND_IO_CAP_NO_INPUT_NO_OUTPUT/*GAPBOND_IO_CAP_DISPLAY_ONLY*/

// Default timer period for auto-connect
#define TIMER_PERIOD          2000

// Default service discovery timer delay in ms
#define DEFAULT_SVC_DISCOVERY_DELAY           50

// TRUE to filter discovery results on desired service UUID
#define DEFAULT_DEV_DISC_BY_SVC_UUID          TRUE/*FALSE*/

// Default auto connect timer delay in ms
#define DEFAULT_AUTOCONNECT_DELAY           50

/* One shot timer delay */
#define ONESHOT_TIMER_DELAY                 1000

#define DEFAULT_BLINK_DELAY                 2000

#define DECRYPTED_PACKET_LENGTH             16

#define SECURITY_PACKET_LENGTH              32

#define SLEEP_TIME            			    10000


// Length of bd addr as a string
#define B_ADDR_STR_LEN                        15

// Task configuration
#define SBC_TASK_PRIORITY                     2

#ifndef SBC_TASK_STACK_SIZE
#define SBC_TASK_STACK_SIZE                   864
#endif

// Application states
enum
{
  BLE_STATE_IDLE,
  BLE_STATE_CONNECTING,
  BLE_STATE_CONNECTED,
  BLE_STATE_DISCONNECTING
};

// Discovery states
enum
{
  BLE_DISC_STATE_IDLE,                // Idle
  BLE_DISC_STATE_MTU,                 // Exchange ATT MTU size
  BLE_DISC_STATE_SVC,                 // Service discovery
  BLE_DISC_STATE_CHAR                 // Characteristic discovery
};

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

/*********************************************************************
 * TYPEDEFS
 */

// App event passed from profiles.
typedef struct
{
  appEvtHdr_t hdr; // event header
  uint8_t *pData;  // event data
} sbcEvt_t;

// RSSI read data structure
typedef struct
{
  uint16_t period;      // how often to read RSSI
  uint16_t connHandle;  // connection handle
  Clock_Struct *pClock; // pointer to clock struct
} readRssi_t;

/*********************************************************************
 * GLOBAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL VARIABLES
 */

/*********************************************************************
 * LOCAL VARIABLES
 */

// Entity ID globally used to check for source and/or destination of messages
static ICall_EntityID selfEntity;

// Semaphore globally used to post events to the application thread
static ICall_Semaphore sem;

// Clock object used to signal timeout
static Clock_Struct startDiscClock;

// Clock object used to signal timeout
static Clock_Struct Sw_timer_Clock;
// Clock object used for one shot timer, used to write packets
extern Clock_Struct OneShot_timer_Clock;
// Clock object used for blinking LED
extern Clock_Struct led_Clock;
extern gattAttribute_t simpleProfileAttrTbl[5];
extern CONST uint8 simpleProfilechar1UUID[ATT_BT_UUID_SIZE];
uint32_t fourByteRandom = 0;
gapAuthParams_t params;
/* Clock object for discovery delay */
// Queue object used for app messages
static Queue_Struct appMsg;
static Queue_Handle appMsgQueue;

// Task pending events
static uint16_t events = 0;

// Task configuration
Task_Struct sbcTask;
Char sbcTaskStack[SBC_TASK_STACK_SIZE];

// GAP GATT Attributes
static const uint8_t attDeviceName[GAP_DEVICE_NAME_LEN] = "Stargate 03"/*"Simple BLE Central"*/;

// Number of scan results and scan result index
static uint8_t scanRes;
static uint8_t scanIdx;

// Scan result list
static gapDevRec_t devList[DEFAULT_MAX_SCAN_RES];

// Scanning state
static bool scanningStarted = FALSE;

// Connection handle of current connection
static uint16_t connHandle = GAP_CONNHANDLE_INIT;

// Application state
static uint8_t state = BLE_STATE_IDLE;

// Discovery state
static uint8_t discState = BLE_DISC_STATE_IDLE;

// Discovered service start and end handle
static uint16_t svcStartHdl = 0;
static uint16_t svcEndHdl = 0;

// Discovered characteristic handle
static uint16_t charHdl = 0;

// Value to write
static uint8_t charVal = 0;

// Value read/write toggle
static bool doWrite = FALSE;

// GATT read/write procedure state
static bool procedureInProgress = FALSE;

// Maximum PDU size (default = 27 octets)
static uint16 maxPduSize;

// Array of RSSI read structures
static readRssi_t readRssi[MAX_NUM_BLE_CONNS];

uint8_t read_ussk;
/* Device address for connection */
uint8 devAddrType;           //!< address type: @ref ADDRTYPE_DEFINES
uint8 devAddr[B_ADDR_LEN];
/* Original USSK */
 uint8 originalUSSK[5] = {1, 2, 3, 4};
 uint8_t originalUSSK_len = 4;
 bool isRegistered = false, chck_conn_state = 0,pkt_verify_timeout = 0;
/* variable to store decrypted data */
uint8_t decryptedData[KEYLEN  + sizeof(uint8_t)];
uint8_t encryptedData[KEYLEN  + sizeof(uint8_t)];
uint8_t randomReceived[KEYLEN];
uint8_t rcvdEncryptedData[KEYLEN] = {0};
uint8_t decryptedrcvdData[KEYLEN] = {0};
uint8_t tenByteRandomGenerated[10] = {0};
uint8 aesKey[16] = "vitaNet-aes-key1";  //Key for decrypt stored here
uint8_t securityPacket[SECURITY_PACKET_LENGTH] = {0};
uint8_t encryptedSecurityPacket[SECURITY_PACKET_LENGTH + 1] = {0};
uint8_t encryptedPacketBuffer[16] = {0};
/* Current time */
uint32_t current_ticks;
uint32_t scanStarted_ticks;
uint32_t deviceFound_ticks;
uint32_t pairingStarted_ticks;
uint32_t deviceConnected_ticks;
uint32_t readReqSent_ticks;
uint32_t usskRegistered_ticks;
uint32_t start_conn_ticks;
//uint32_t new_time;
/* Clock frequency */
Types_FreqHz clkFreq;
/* 64bit timer */
Types_Timestamp64 time64;
/*BLE states*/
//uint8_t app_state = 0;

 uint8_t app_state = APP_START_DISCOVERY;

/* manually disconnect when not responding while link establishment */
uint8_t disconn_count = 0;

/* Flag to check whether security packet to be sent to App is to be prepared or not */
static bool preparePacketFlag = false;
/* Flag to check whether security packet to be written on App's characteristic or not */
static bool writePacketFlag = false;
/* Flag to check whether first 20 bytes have been written on App's characteristic or not */
static bool first20BytesSent = false;

bool receiveEncryptedPcktFlag = false;
/* Flag to check whether first 20 bytes have been written on App's characteristic or not  */
bool verifyRcvdPacketFlag = false;
/* status of led high/low */

//bool wait_for_connect_req = false;
bool USSK_ack = false;

bool led1 = FALSE;
extern PIN_Handle led_pins_handle;
/* LED on-time off-time */
extern uint16_t on_time,off_time;
//static uint8_t oneSecFlag = 0;

uint8_t snv_buffer[21];



//void Central_createTask(void);

static void BLECentral_taskFxn(UArg a0, UArg a1);

static void BLECentral_processStackMsg(ICall_Hdr *pMsg);

static void BLECentral_processRoleEvent(gapCentralRoleEvent_t *pEvent);

static void BLECentral_startDiscovery(void);

static void BLECentral_processGATTDiscEvent(gattMsgEvent_t *pMsg);

static void BLECentral_addDeviceInfo(uint8_t *pAddr, uint8_t addrType);

static uint8_t BLECentral_eventCB(gapCentralRoleEvent_t *pEvent);

static void BLECentral_pairStateCB(uint16_t connHandle, uint8_t state, uint8_t status);

void BLECentral_startDiscHandler(UArg a0);

static void BLECentral_processGATTMsg(gattMsgEvent_t *pMsg);

static void BLECentral_processAppMsg(sbcEvt_t *pMsg);

static uint8_t decryptAdvPacket(gapCentralRoleEvent_t *pEvent);

static uint8_t scanForGsk(gapCentralRoleEvent_t *pEvent);

static void ProcessHCICmdCompleteEvt(hciEvt_CmdComplete_t *pMsg);

static void PacketVerifyFail_handler(void);

static void BLECentral_processGATTMsg(gattMsgEvent_t *pMsg);

static void BLECentral_processCmdCompleteEvt(hciEvt_CmdComplete_t *pMsg);

static void BLECentral_processAppMsg(sbcEvt_t *pMsg);

static void BLECentral_init(void);

static bool BLECentral_findSvcUuid(uint16_t uuid, uint8_t *pData,
                                         uint8_t dataLen);

static void BLECentral_processPairState(uint8_t state, uint8_t status);

static uint8_t BLECentral_enqueueMsg(uint8_t event, uint8_t state,
                                           uint8_t *pData);

static void BLECentral_processCmdCompleteEvt(hciEvt_CmdComplete_t *pMsg);

static void ReverseBytes(uint8_t *buf, uint8_t len);

static void scanForUssk(void);

// GAP Role Callbacks
static gapCentralRoleCB_t BLECentral_roleCB =
{
  BLECentral_eventCB     // Event callback
};

// Bond Manager Callbacks
static gapBondCBs_t BLECentral_bondCB =
{
  NULL, // Passcode callback
  BLECentral_pairStateCB // Pairing state callback
};

#ifdef __cplusplus
}
#endif
#endif /* STARGATE_LIB_BLE_MNGR_H_ */
