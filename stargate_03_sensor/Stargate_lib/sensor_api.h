#ifndef SENSOR_API_H
#define SENSOR_API_H

#ifdef __cplusplus
extern "C"
{
#endif


#include <stdio.h>
#include <stdint.h>
//#include "common.h"


//typedef void (DeviceDataReadCallback_t) (dev_data_t sensor_info);
    
typedef void (RegConn_CB) (void);

typedef void (RegComm_CB) (uint32_t command, char* data);
    
/*
API Syntax:   int initializeBLE ( )
Description:  Initializes Icall framework and GAP, GATT configuration.
Parameters:   None
Return:       In case of error, an error object with details.
              In case of success, the error object is empty.
*/
int initializeBLE (void);


/*

API Syntax:   int SendData(uint8_t *data, uint8_t count)
Description:  sends data to connected device with timestamp, can be used during sensor setup
			  and start operation.

Parameters
data:         pointer to data value to be sent to connected device. The data could be both events as well as sensor data.
count :       count of data being sent
Return:       messageID Message to be passed on application

*/
int SendData(uint8_t *data, uint8_t count);

/*
API Syntax:   RegisterForConnSuccess(RegConn_CB *callback)
Description:  Register callback function for successful connection with Gateway.

Parameters:   callback pointer to callback event structure
Return:       In case of error, an error object with details.
              In case of success, the error object is empty.
*/

int RegisterForConnSuccess(RegConn_CB *callback);

/*
API Syntax:   RegisterForCommands(RegComm_CB *callback)
Description:  Register callback function for commands received from Gateway
			  Sensor setup, start operation, end operation.

			  RegComm_CB also receives which command is received and corresponding data.

Parameters:   callback pointer to callback event structure
Return:       In case of error, an error object with details.
              In case of success, the error object is empty.
*/

int RegisterForCommands(RegComm_CB *callback);


#ifdef __cplusplus
}
#endif


#endif
