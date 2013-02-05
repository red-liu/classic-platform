/*
* Configuration of module: Rte (Rte_Blinker.h)
*
* Created by:              
* Copyright:               
*
* Configured for (MCU):    MPC551x
*
* Module vendor:           ArcCore
* Generator version:       0.0.13
*
* Generated by Arctic Studio (http://arccore.com) 
*/

/* Rte_Blinker.h */

#ifndef RTE_BLINKER_H
#define RTE_BLINKER_H

#include "Rte_Type.h"

#define RTE_E_DigitalOutput_E_NOT_OK 1

#define Rte_Call_LED_Port_Set Rte_Call_Blinker_LED_Port_Set

Std_ReturnType Rte_Call_Blinker_LED_Port_Set(const DigitalLevel value);

void BlinkerRunnable(void);

#endif
