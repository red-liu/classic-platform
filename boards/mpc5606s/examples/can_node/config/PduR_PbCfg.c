/*
* Configuration of module: PduR (PduR_PbCfg.c)
*
* Created by:              
* Copyright:               
*
* Configured for (MCU):    MPC551x
*
* Module vendor:           ArcCore
* Generator version:       2.0.2
*
* Generated by Arctic Studio (http://arccore.com) 
*           on Tue May 10 13:37:44 CEST 2011
*/


#include "PduR.h"
#include "PduR_Cfg.h"
#include "PduR_PbCfg.h"

#if PDUR_CANIF_SUPPORT == STD_ON
#include "CanIf.h"
#endif
#if PDUR_CANTP_SUPPORT == STD_ON
#include "CanTp.h"
#endif
#if PDUR_LINIF_SUPPORT == STD_ON
#include "LinIf.h"
#endif
#if PDUR_COM_SUPPORT == STD_ON
#include "Com.h"
#endif
#if PDUR_DCM_SUPPORT == STD_ON
#include "Dcm.h"
#endif


PduRTxBufferTable_type PduRTxBufferTable = {
	.PduRMaxTxBufferNumber = 1,
	.PduRTxBuffer = {
		{
			.Depth = 0,
		},
	}
};


PduRRoutingTable_type PduRRoutingTable = {
	.PduRRoutingPath = {
		{ // End of routing table
			.PduR_Arc_EOL = 1
		}
	}
};




PduR_PBConfigType PduR_Config = {
	.PduRConfigurationId = 0,
	.PduRTxBufferTable = &PduRTxBufferTable,
	.PduRRoutingTable = &PduRRoutingTable,
};
