#ifndef COMMON_H
#define COMMON_H

#ifdef __cplusplus
extern "C"
{
#endif


#include <stdio.h>
#include <stdint.h>

#define DEV_MAC_ID_LEN      30
#define DEV_DATA_LEN        128

/* Return codes */
#define BLE_MGR_RET_SUCCESS              0  /* Success */
#define BLE_MGR_RET_ERR_INV_ARG         -1  /* Invalid argument */
#define BLE_MGR_RET_ERR_CON_FAIL        -2  /* DBUS Connection failure */
#define BLE_MGR_RET_ERR_SCAN_START_FAIL -3  /* Scan start fail */
#define BLE_MGR_RET_ERR_SCAN_STOP_FAIL  -4  /* Scan stop fail */
#define BLE_MGR_RET_ERR_DEV_CON_FAIL    -5  /* Device connect fail */
#define BLE_MGR_RET_ERR_DEV_DISCON_FAIL -6  /* Device disconnect fail */
#define BLE_MGR_RET_ERR_RD_CHAR_FAIL    -7  /* Read characteristic fail */
#define BLE_MGR_RET_ERR_WR_CHAR_FAIL    -8  /* Write characteristic fail */
#define BLE_MGR_RET_ERR_NO_VERIF_CB     -9  /* Verification callback is not
                                                   registered */
#define BLE_MGR_RET_ERR_INV_STATE       -10 /* Invalid device state,
                                                   unlikely to receive */
#define BLE_MGR_RET_ERR_AUTH_DATA       -11 /* Authentication data is
                                                   invalid */
#define BLE_MGR_RET_ERR_DEV_ID_INV      -12 /* Service uuid is matched, but
                                                   Device_ID is invalid */
#define BLE_MGR_RET_ERR_NO_DEV          -13 /* Device is not connected */
#define BLE_MGR_RET_ERR_NEW_MSG         -14 /* DBUS message creation fail */
#define BLE_MGR_RET_ERR_MSG_SEND        -15 /* DBUS message send fail */
#define BLE_MGR_RET_ERR_MSG_REPLY       -16 /* DBUS message reply fail */
#define BLE_MGR_RET_ERR_PROXY_SET       -17 /* DBUS proxy set fail */
#define BLE_MGR_RET_ERR_CLIENT_RDY      -18 /* DBUS client ready watch
                                                   fail */
#define BLE_MGR_RET_ERR_ITER_ARG        -19 /* DBUS get interface arg
                                                   fail */
#define BLE_MGR_RET_ERR_PROP_GET        -20 /* DBUS get property fail */
#define BLE_MGR_RET_ERR_ADAPTER_OFF     -21 /* Adapter is powered off */
#define BLE_MGR_RET_ERR_NO_ADAPTER      -22 /* No Adapter found or adapter
                                                   is removed */
#define BLE_MGR_RET_ERR_MALLOC          -23 /* Malloc failed to allocate
                                                   memory */
#define BLE_MGR_RET_ERR_ADV_START_FAIL  -24 /* Advertise start fail */
#define BLE_MGR_RET_ERR_SOCKET_INIT     -25 /* Socket init failure */
#define BLE_MGR_RET_ERR_SOCKET_WR       -26 /* Socket write failure */
#define BLE_MGR_RET_ERR_SOCKET_RD       -27 /* Socket read failure */
#define BLE_MGR_RET_ERR_SOCKET_RSP      -28 /* Socket response failure */
#define BLE_MGR_RET_ERR_SOCKET_INV_RSP  -29 /* Invalid event in
                                                   Socket response */
#define BLE_MGR_RET_ERR_SERV_REG        -30 /* Service registration
                                                   failure */
#define BLE_MGR_RET_ERR_CHAR_REG        -31 /* Characteristic registration
                                                   failure */
#define BLE_MGR_RET_ERR_DESC_REG        -32 /* Descriptor registration
                                                   failure */
#define BLE_MGR_RET_ERR_NEW_PROXY       -33 /* Get DBUS proxy handler
                                                   is failed */
#define BLE_MGR_RET_ERR_UNKWN_CMD       -34 /* Unknown Command */
#define BLE_MGR_RET_ERR_CMD_FAILED      -35 /* Command Failed */
#define BLE_MGR_RET_ERR_ALRDY_CONN      -36 /* Already Connected */
#define BLE_MGR_RET_ERR_DEV_BUSY        -37 /* Busy */
#define BLE_MGR_RET_ERR_CMD_REJECTED    -38 /* Rejected */
#define BLE_MGR_RET_ERR_CMD_NOT_SUPP    -39 /* Not Supported */
#define BLE_MGR_RET_ERR_CMD_CANCELLED   -40 /* Cancelled */
#define BLE_MGR_RET_ERR_INVALID_INDX    -41 /* Invalid Index */
#define BLE_MGR_RET_ERR_PERM_DENIED     -42 /* Permission Denied */
#define BLE_MGR_RET_ERR_TIMER_STOP      -43 /* Permission Denied */
#define BLE_MGR_RET_ERR_NO_DATA_CB      -44 /* Data callback is not
                                                   registered */
#define BLE_MGR_RET_ERR_INV_ID          -45 /* Invalid ID */
#define BLE_MGR_RET_FAIL                -46  /* Success */

/* instructions to sensor */
#define SETUP_SENSOR		             01 /*  */
#define START_OPERATION                  02  /*   */
#define END_OPERATION		             03 /*    */
//#define START_OPERATION                 -46  /*  */

/* Device parameters */
typedef struct dev_data
{
   char  deviceID[30];
   char  dev_addr[DEV_MAC_ID_LEN];
   uint8_t  data[DEV_DATA_LEN];
} dev_data_t;

dev_data_t dev_data_obj;

#ifdef __cplusplus
}
#endif


#endif
