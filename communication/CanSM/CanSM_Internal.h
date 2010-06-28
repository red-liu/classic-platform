/* -------------------------------- Arctic Core ------------------------------
 * Arctic Core - the open source AUTOSAR platform http://arccore.com
 *
 * Copyright (C) 2009  ArcCore AB <contact@arccore.com>
 *
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation; See <http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 * -------------------------------- Arctic Core ------------------------------*/


#ifndef CANSM_INTERNAL_H_
#define CANSM_INTERNAL_H_

#include "CanSM.h"

#if (CANSM_DEV_ERROR_DETECT == STD_ON)
#define CANSM_DET_REPORTERROR(serviceId, errorId)			\
	Det_ReportError(MODULE_ID_CANSM, 0, serviceId, errorId)

#define CANSM_VALIDATE(expression, serviceId, errorId, ...)	\
	if (!(expression)) {									\
		CANSM_DET_REPORTERROR(serviceId, errorId);			\
		return __VA_ARGS__;									\
	}

#else
#define CANSM_DET_REPORTERROR(...)
#define CANSM_VALIDATE(...)
#endif

#define CANSM_VALIDATE_INIT(serviceID, ...)					\
		CANSM_VALIDATE((CanSM_Internal.InitStatus == CANSM_STATUS_INIT), serviceID, CANSM_E_UNINIT, __VA_ARGS__)

#define CANSM_VALIDATE_POINTER(pointer, serviceID, ...)					\
		CANSM_VALIDATE( (pointer != NULL), serviceID, CANSM_E_PARAM_POINTER, __VA_ARGS__)

#define CANSM_VALIDATE_NETWORK(net, serviceID, ...)					\
		CANSM_VALIDATE( (net < CANSM_NETWORK_COUNT), serviceID, CANSM_E_INVALID_NETWORK_HANDLE, __VA_ARGS__)

#define CANSM_VALIDATE_MODE(mode, serviceID, ...)					\
		CANSM_VALIDATE( (mode <= COMM_FULL_COMMUNICATION) && (mode != COMM_SILENT_COMMUNICATION), serviceID, CANSM_E_INVALID_NETWORK_MODE, __VA_ARGS__)

typedef enum {
	CANSM_STATUS_UNINIT,
	CANSM_STATUS_INIT,
} CanSM_Internal_InitStatusType;


typedef struct {
	uint8						CanIfControllerId;
} CanSM_Internal_ControllerType;

typedef struct {
	CanSM_Internal_ControllerType*		Controllers;
	ComM_ModeType						CurrentMode;
	ComM_ModeType						RequestedMode;
} CanSM_Internal_NetworkType;

typedef struct {
	CanSM_Internal_InitStatusType 		InitStatus;
	CanSM_Internal_NetworkType*		Networks;
} CanSM_InternalType;

Std_ReturnType CanSM_Internal_RequestComMode( NetworkHandleType NetworkHandle, ComM_ModeType ComM_Mode );
Std_ReturnType CanSM_Internal_RequestCanIfMode( NetworkHandleType NetworkHandle, ComM_ModeType ComM_Mode );
Std_ReturnType CanSM_Internal_RequestComGroupMode( NetworkHandleType NetworkHandle, ComM_ModeType ComM_Mode );

#endif /* CANSM_INTERNAL_H_ */