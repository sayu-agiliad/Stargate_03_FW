/*
 * sensor.c
 *
 */

#include <stdio.h>
#include <stdint.h>

#include "sensor_api.h"
#include "init.h"
#include "central.h"
//#include "ble_mngr.h"


/*
API Syntax:   int initializeBLE ( )
Description:  Initializes Icall framework and GAP, GATT configuration.
Parameters:   None
Return:       In case of error, an error object with details.
              In case of success, the error object is empty.
*/
int initializeBLE (void)
{

	/* Initialize ICall module */
	/* Start tasks of external images - Priority 5 */
	init_ICall();

    /* Kick off profile - Priority 3 */
    /* to be added in library*/
	GAPCentralRole_createTask();

	/* Kick off profile - Priority 3 */
	Central_createTask();

	return 0;
}


/*

API Syntax:   int SendData(uint8_t *data, uint8_t count)
Description:  sends data to connected device with timestamp, can be used during sensor setup
			  and start operation.

Parameters
data:         pointer to data value to be sent to connected device. The data could be both events as well as sensor data.
count :       count of data being sent
Return:       messageID Message to be passed on application

*/
int SendData(uint8_t *data, uint8_t count)
{

	StartScan();
	/* can be used if not connected
	 * data will be saved upto 20 frames*/


}


/*
API Syntax:   RegisterForConnSuccess(RegConn_CB *callback)
Description:  Register callback function for successful connection with Gateway.

Parameters:   callback pointer to callback event structure
Return:       In case of error, an error object with details.
              In case of success, the error object is empty.
*/

int RegisterForConnSuccess(RegConn_CB *callback)
{

}

/*
API Syntax:   RegisterForCommands(RegComm_CB *callback)
Description:  Register callback function for commands received from Gateway
			  Sensor setup, start operation, end operation.

			  RegComm_CB also receives which command is received and corresponding data.

Parameters:   callback pointer to callback event structure
Return:       In case of error, an error object with details.
              In case of success, the error object is empty.
*/

int RegisterForCommands(RegComm_CB *callback)
{

}
