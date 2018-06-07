/*
 * init.c
 *
 */
#include <xdc/runtime/Error.h>
#include <xdc/runtime/System.h>
#include <ti/sysbios/family/arm/cc26xx/TimestampProvider.h>
#include <ti/sysbios/family/arm/cc26xx/Power.h>
//#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/sysbios/family/arm/cc26xx/PowerCC2650.h>
#include <ti/sysbios/BIOS.h>
#include <stdio.h>
#include "ICall.h"
#include "hal_assert.h"
#include "central.h"
/* Header files required to enable instruction fetch cache */
#include <inc/hw_memmap.h>
#include <driverlib/vims.h>

/*
#ifndef USE_DEFAULT_USER_CFG

#include "bleUserConfig.h"

// BLE user defined configuration
bleUserCfg_t user0Cfg = BLE_USER_CFG;

#endif // USE_DEFAULT_USER_CFG
*/

void init_ICall(void)
{
    /* Initialize ICall module */
    ICall_init();

    /* Start tasks of external images - Priority 5 */
    /* to be added in library*/
    ICall_createRemoteTasks();


}


