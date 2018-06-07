/*
 * init.h
 * functions and variable used to implement fw APIS
 *
 */

#ifndef STARGATE_LIB_INIT_H_
#define STARGATE_LIB_INIT_H_


#include <stdio.h>
#include <stdint.h>

void init_ICall(void);

void Central_createTask(void);

/*********************************************************************
 * @fn      StartScan
 *
 * @brief   initiate scanning by semaphore post
 *
 * @param   none
 *
 * @return  none
 */

void StartScan(void);

#endif /* STARGATE_LIB_INIT_H_ */
