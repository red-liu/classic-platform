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



/* Uncomment this only if you now what you are doing. This will make the mpc5606s driver use DMA.
 * The DMA based ADC demands channels being sequential in a group i.e. 1,2,3 or 5,6,7 and NOT 1,3,7.
 * This also forbids the use of streamed buffers at the moment. Work is ongoing to correct DMA behaviour.
 */
#define DONT_USE_DMA_IN_ADC_MPC5606S

#include <assert.h>
#include <stdlib.h>
//#include "System.h"
#include "mpc55xx.h"
#include "Modules.h"
#include "Mcu.h"
#include "Adc.h"
#ifndef DONT_USE_DMA_IN_ADC_MPC5606S
#include "Dma.h"
#endif
#include "Det.h"
#include "Os.h"
#include "isr.h"
#include "irq.h"
#include "arc.h"


#if !defined(CFG_MPC5606S)
typedef union
{
  vuint32_t R;
  struct
  {
    vuint32_t EOQ:1;
    vuint32_t PAUSE:1;
    vuint32_t :4;
    vuint32_t BN:1;
    vuint32_t RW:1;
    vuint32_t ADC_REG:16;
    vuint32_t ADC_REG_ADDR:8;
   } B;
}Adc_RegisterWriteType;

typedef union
{
  vuint32_t R;
  struct
  {
    vuint32_t EOQ:1;
    vuint32_t PAUSE:1;
    vuint32_t :4;
    vuint32_t BN:1;
    vuint32_t RW:1;
    vuint32_t MESSAGE_TAG:4;
    vuint32_t :12;
    vuint32_t ADC_REG_ADDR:8;
   } B;
}Adc_RegisterReadType;

typedef enum
{
  ADC_EQADC_QUEUE_0,
  ADC_EQADC_QUEUE_1,
  ADC_EQADC_QUEUE_2,
  ADC_EQADC_QUEUE_3,
  ADC_EQADC_QUEUE_4,
  ADC_EQADC_QUEUE_5,
  ADC_EQADC_NBR_OF_QUEUES
}Adc_eQADCQueueType;

typedef enum
{
  EQADC_CFIFO_STATUS_IDLE = 0,
  EQADC_CFIFO_STATUS_WAITINGFOR_TRIGGER = 0x2,
  EQADC_CFIFO_STATUS_TRIGGERED = 0x3
}Adc_EQADCQueueStatusType;

typedef int16_t Adc_EQADCRegister;

typedef enum
{
  ADC0_CR = 1,
  ADC0_TSCR,
  ADC0_TBCR,
  ADC0_GCCR,
  ADC0_OCCR
}Adc_EQADCRegisterType;

/* Command queue for calibration sequence. See 31.5.6 in reference manual. */
const Adc_CommandType AdcCalibrationCommandQueue [] =
{
  /* Four samples of 25 % of (VRh - VRl). */
  {
	.B.EOQ = 0, .B.PAUSE = 0, .B.BN = 0, .B.CAL = 0, .B.MESSAGE_TAG = 0, .B.LST = ADC_CONVERSION_TIME_128_CLOCKS, .B.TSR = 0, .B.FMT = 0,
	.B.CHANNEL_NUMBER = 44
  },
  {
    .B.EOQ = 0, .B.PAUSE = 0, .B.BN = 0, .B.CAL = 0, .B.MESSAGE_TAG = 0, .B.LST = ADC_CONVERSION_TIME_128_CLOCKS, .B.TSR = 0, .B.FMT = 0,
    .B.CHANNEL_NUMBER = 44
  },
  {
    .B.EOQ = 0, .B.PAUSE = 0, .B.BN = 0, .B.CAL = 0, .B.MESSAGE_TAG = 0, .B.LST = ADC_CONVERSION_TIME_128_CLOCKS, .B.TSR = 0, .B.FMT = 0,
    .B.CHANNEL_NUMBER = 44
  },
  {
    .B.EOQ = 0, .B.PAUSE = 0, .B.BN = 0, .B.CAL = 0, .B.MESSAGE_TAG = 0, .B.LST = ADC_CONVERSION_TIME_128_CLOCKS, .B.TSR = 0, .B.FMT = 0,
    .B.CHANNEL_NUMBER = 44
  },
  /* Four samples of 75 % of (VRh - VRl). */
  {
    .B.EOQ = 0, .B.PAUSE = 0, .B.BN = 0, .B.CAL = 0, .B.MESSAGE_TAG = 0, .B.LST = ADC_CONVERSION_TIME_128_CLOCKS, .B.TSR = 0, .B.FMT = 0,
    .B.CHANNEL_NUMBER = 43
  },
  {
    .B.EOQ = 0, .B.PAUSE = 0, .B.BN = 0, .B.CAL = 0, .B.MESSAGE_TAG = 0, .B.LST = ADC_CONVERSION_TIME_128_CLOCKS, .B.TSR = 0, .B.FMT = 0,
    .B.CHANNEL_NUMBER = 43
  },
  {
    .B.EOQ = 0, .B.PAUSE = 0, .B.BN = 0, .B.CAL = 0, .B.MESSAGE_TAG = 0, .B.LST = ADC_CONVERSION_TIME_128_CLOCKS, .B.TSR = 0, .B.FMT = 0,
    .B.CHANNEL_NUMBER = 43
  },
  {
    .B.EOQ = 1, .B.PAUSE = 0, .B.BN = 0, .B.CAL = 0, .B.MESSAGE_TAG = 0, .B.LST = ADC_CONVERSION_TIME_128_CLOCKS, .B.TSR = 0, .B.FMT = 0,
    .B.CHANNEL_NUMBER = 43
  }
};

/* DMA configuration for calibration sequence. */
const struct tcd_t AdcCalibrationDMACommandConfig =
{
  .SADDR = (uint32_t)AdcCalibrationCommandQueue,
  .SMOD = 0,
  .SSIZE = DMA_TRANSFER_SIZE_32BITS,
  .DMOD = 0,
  .DSIZE = DMA_TRANSFER_SIZE_32BITS,
  .SOFF = sizeof(Adc_CommandType),
  .NBYTES = sizeof(Adc_CommandType),
  .SLAST = 0,
  .DADDR = (vint32_t)&EQADC.CFPR[0].R,
  .CITERE_LINK = 0,
  .CITER = 0,
  .DOFF = 0,
  .DLAST_SGA = 0,
  .BITERE_LINK = 0,
  .BITER = 0,
  .BWC = 0,
  .MAJORLINKCH = 0,
  .DONE = 0,
  .ACTIVE = 0,
  .MAJORE_LINK = 0,
  .E_SG = 0,
  .D_REQ = 0,
  .INT_HALF = 0,
  .INT_MAJ = 0,
  .START = 0,
};

const struct tcd_t AdcCalibrationDMAResultConfig =
{
  .SADDR = (vint32_t)&EQADC.RFPR[0].R + 2,
  .SMOD = 0,
  .SSIZE = DMA_TRANSFER_SIZE_16BITS,
  .DMOD = 0,
  .DSIZE = DMA_TRANSFER_SIZE_16BITS,
  .SOFF = 0,
  .NBYTES = sizeof(Adc_ValueGroupType),
  .SLAST = 0,
  .DADDR = 0, /* Dynamic address, written later. */
  .CITERE_LINK = 0,
  .CITER = 0,
  .DOFF = sizeof(Adc_ValueGroupType),
  .DLAST_SGA = 0,
  .BITERE_LINK = 0,
  .BITER = 0,
  .BWC = 0,
  .MAJORLINKCH = 0,
  .DONE = 0,
  .ACTIVE = 0,
  .MAJORE_LINK = 0,
  .E_SG = 0,
  .D_REQ = 0,
  .INT_HALF = 0,
  .INT_MAJ = 0,
  .START = 0
};
#endif

typedef enum
{
  ADC_UNINIT,
  ADC_INIT,
}Adc_StateType;

/* Function prototypes. */
#if defined(CFG_MPC5606S)
static void Adc_ConfigureADC (const Adc_ConfigType *ConfigPtr);
static void Adc_ConfigureADCInterrupts (void);
#else
static void Adc_ConfigureEQADC (const Adc_ConfigType *ConfigPtr);
static void Adc_ConfigureEQADCInterrupts (void);
static void Adc_EQADCCalibrationSequence (void);
static void Adc_WriteEQADCRegister (Adc_EQADCRegisterType reg, Adc_EQADCRegister value);
static Adc_EQADCRegister Adc_ReadEQADCRegister (Adc_EQADCRegisterType reg);
#endif

/* Development error checking. */
static Std_ReturnType Adc_CheckReadGroup (Adc_GroupType group);
static Std_ReturnType Adc_CheckStartGroupConversion (Adc_GroupType group);
static Std_ReturnType Adc_CheckStopGroupConversion (Adc_GroupType group);
static Std_ReturnType Adc_CheckInit (const Adc_ConfigType *ConfigPtr);
static Std_ReturnType Adc_CheckDeInit (void);
static Std_ReturnType Adc_CheckSetupResultBuffer (Adc_GroupType group);


/* isoft - static variable declarations */
static Adc_StateType adcState = ADC_UNINIT;
static const Adc_ConfigType *AdcConfigPtr;      /* Pointer to configuration structure. */
static Adc_GroupType s_CurrGroupId;       /* current group Id */
static Adc_StreamNumSampleType s_CurrSampleCount = 0;   /* Streaming sample counter of current group */
static uint32_t loopCnt = 0;


#if (ADC_DEINIT_API == STD_ON)
Std_ReturnType Adc_DeInit (const Adc_ConfigType *ConfigPtr)
{
#if defined(CFG_MPC5606S)

  if (E_OK == Adc_CheckDeInit())
  {
    for(Adc_GroupType group = ADC_GROUP0; group < AdcConfigPtr->nbrOfGroups; group++)
    {
      /* Set group status to idle. */
      AdcConfigPtr->groupConfigPtr[group].status->groupStatus = ADC_IDLE;
    }

    /* Disable DMA transfer*/
    ADC_0.DMAE.B.DMAEN = 0;

    /* Power down ADC */
    ADC_0.MCR.R = 0x0001;

    /* Disable all interrupt*/
    ADC_0.IMR.R = 0;

    /* Clean internal status. */
    AdcConfigPtr = (Adc_ConfigType *)NULL;
    adcState = ADC_UNINIT;
  }

  return (E_OK);
    
#else

  Adc_eQADCQueueType queue;
  Adc_GroupType group;
  boolean queueStopped;

  if (ADC_UNINIT != adcState)
  {
    /* Stop all queues. */
    for (queue = ADC_EQADC_QUEUE_0; queue < ADC_EQADC_NBR_OF_QUEUES; queue++)
    {
      /* Disable queue. */
      EQADC.CFCR[queue].B.MODE = 0;

      /* Wait for queue to enter idle state. */
      queueStopped = FALSE;
      /* TODO replace switch with bit pattern. */
      while (!queueStopped)
      {
        switch (queue)
        {
        case ADC_EQADC_QUEUE_0:
          queueStopped = (EQADC.CFSR.B.CFS0 == EQADC_CFIFO_STATUS_IDLE);
          break;
        case ADC_EQADC_QUEUE_1:
          queueStopped = (EQADC.CFSR.B.CFS1 == EQADC_CFIFO_STATUS_IDLE);
          break;
        case ADC_EQADC_QUEUE_2:
          queueStopped = (EQADC.CFSR.B.CFS2 == EQADC_CFIFO_STATUS_IDLE);
          break;
        case ADC_EQADC_QUEUE_3:
          queueStopped = (EQADC.CFSR.B.CFS3 == EQADC_CFIFO_STATUS_IDLE);
          break;
        case ADC_EQADC_QUEUE_4:
          queueStopped = (EQADC.CFSR.B.CFS4 == EQADC_CFIFO_STATUS_IDLE);
          break;
        case ADC_EQADC_QUEUE_5:
          queueStopped = (EQADC.CFSR.B.CFS5 == EQADC_CFIFO_STATUS_IDLE);
          break;
        default :
          /* We should never get here... Terminate loop. */
          queueStopped = TRUE;
          break;
        }
      }

      /* Disable eDMA requests for commands and results. */
      EQADC.IDCR[queue].B.CFFS = 0;
      EQADC.IDCR[queue].B.RFDS = 0;

      /* Disable FIFO fill requests. */
      EQADC.IDCR[queue].B.CFFE = 0;
      EQADC.IDCR[queue].B.RFDE = 0;

      /* Disable interrupts. */
      EQADC.IDCR[queue].B.RFOIE = 0;
      EQADC.IDCR[queue].B.CFUIE = 0;
      EQADC.IDCR[queue].B.TORIE = 0;
      EQADC.IDCR[queue].B.EOQIE = 0;
    }

    /* Stop all DMA channels connected to EQADC. */
    for (group = ADC_GROUP0; group < AdcConfigPtr->nbrOfGroups; group++)
    {
      Dma_StopChannel (AdcConfigPtr->groupConfigPtr [group].dmaCommandChannel);
      Dma_StopChannel (AdcConfigPtr->groupConfigPtr [group].dmaResultChannel);

      /* Set group status to idle. */
      AdcConfigPtr->groupConfigPtr[group].status->groupStatus = ADC_IDLE;
    }

    /* Disable EQADC. */
    Adc_WriteEQADCRegister (ADC0_CR, 0);

    /* Clean internal status. */
    AdcConfigPtr = (Adc_ConfigType *)NULL;
    adcState = ADC_UNINIT;
  }
  return (E_OK);
#endif /* ENDOF defined(CFG_MPC5606S) */
}
#endif

Std_ReturnType Adc_Init (const Adc_ConfigType *ConfigPtr)
{
#if defined(CFG_MPC5606S)

  if (E_OK == Adc_CheckInit(ConfigPtr))
  {
            /* First of all, store the location of the configuration data. */
            AdcConfigPtr = ConfigPtr;

            /* Enable ADC. */
             Adc_ConfigureADC(ConfigPtr);

             Adc_ConfigureADCInterrupts();

            /* Move on to INIT state. */
            adcState = ADC_INIT;
            return E_OK;
  }
  else
  {
    return E_NOT_OK;
  }

#else

  Std_ReturnType returnValue;
  Adc_ChannelType channel;
  Adc_ChannelType channelId;
  Adc_GroupType group;
  Adc_CommandType *commandQueue;
  Adc_CommandType command;

  if (E_OK == Adc_CheckInit(ConfigPtr))
  {
    /* First of all, store the location of the configuration data. */
    AdcConfigPtr = ConfigPtr;

    /* Start configuring the eQADC queues. */
    for (group = ADC_GROUP0; group < ConfigPtr->nbrOfGroups; group++)
    {
      /* Loop through all channels and make the command queue. */
      for (channel = 0; channel < ConfigPtr->groupConfigPtr[group].numberOfChannels; channel++)
      {
        /* Get physical channel. */
        channelId = ConfigPtr->groupConfigPtr[group].channelList[channel];

        commandQueue = ConfigPtr->groupConfigPtr[group].commandBuffer;

        /* Begin with empty command. */
        command.R = 0;

        /* Physical channel number. */
        command.B.CHANNEL_NUMBER = channelId;
        /* Sample time. */
        command.B.LST = ConfigPtr->channelConfigPtr [channel].adcChannelConvTime;
        /* Calibration feature. */
        command.B.CAL = ConfigPtr->channelConfigPtr [channel].adcChannelCalibrationEnable;
        /* Result buffer FIFO. The number of groups must not be greater than the number of queues. */
        command.B.MESSAGE_TAG = group;

        /* Write command to command queue. */
        commandQueue [channel].R = command.R;

        /* Last channel in group. Write EOQ and configure eQADC FIFO. */
        if (channel == (ConfigPtr->groupConfigPtr[group].numberOfChannels - 1))
        {
          commandQueue [channel].B.EOQ = 1;
        }
      }
    }

    /* Enable ADC. */
    Adc_ConfigureEQADC (ConfigPtr);

    /* Perform calibration of the ADC. */
    Adc_EQADCCalibrationSequence ();

    /* Configure DMA channels. */
    for (group = ADC_GROUP0; group < ConfigPtr->nbrOfGroups; group++)
    {
      /* ADC307. */
      ConfigPtr->groupConfigPtr[group].status->groupStatus = ADC_IDLE;

      Dma_ConfigureChannel ((struct tcd_t *)ConfigPtr->groupConfigPtr [group].groupDMAResults, ConfigPtr->groupConfigPtr [group].dmaResultChannel);
      Dma_ConfigureChannel ((struct tcd_t *)ConfigPtr->groupConfigPtr [group].groupDMACommands, ConfigPtr->groupConfigPtr [group].dmaCommandChannel);
    }

    /* Start DMA channels. */
    for (group = ADC_GROUP0; group < ConfigPtr->nbrOfGroups; group++)
    {
      /* Invalidate queues. */
      EQADC.CFCR[group].B.CFINV = 1;

      Dma_StartChannel (ConfigPtr->groupConfigPtr [group].dmaResultChannel);
      Dma_StartChannel (ConfigPtr->groupConfigPtr [group].dmaCommandChannel);
    }

    Adc_ConfigureEQADCInterrupts ();

    /* Move on to INIT state. */
    adcState = ADC_INIT;
    returnValue = E_OK;
  }
  else
  {
    returnValue = E_NOT_OK;
  }

  return (returnValue);  
#endif  
}

Std_ReturnType Adc_SetupResultBuffer (Adc_GroupType group, Adc_ValueGroupType *bufferPtr)
{
  Std_ReturnType returnValue;

  /* Check for development errors. */
  if (E_OK == Adc_CheckSetupResultBuffer (group))
  {
    AdcConfigPtr->groupConfigPtr[group].status->resultBufferPtr = bufferPtr;
    
    returnValue = E_OK;
  }
  else
  {
    /* An error have been raised from Adc_CheckSetupResultBuffer(). */
    returnValue = E_NOT_OK;
  }

  return (returnValue);
}

#if (ADC_READ_GROUP_API == STD_ON)
Std_ReturnType Adc_ReadGroup (Adc_GroupType group, Adc_ValueGroupType *dataBufferPtr)
{
  Std_ReturnType returnValue;
  uint8_t channel;

  if (E_OK == Adc_CheckReadGroup (group))
  {
    if ((ADC_CONV_MODE_CONTINOUS == AdcConfigPtr->groupConfigPtr[group].conversionMode) &&
         ((ADC_STREAM_COMPLETED    == AdcConfigPtr->groupConfigPtr[group].status->groupStatus) ||
          (ADC_COMPLETED           == AdcConfigPtr->groupConfigPtr[group].status->groupStatus)))
    {
      /* ADC329, ADC331. */
      AdcConfigPtr->groupConfigPtr[group].status->groupStatus = ADC_BUSY;
      returnValue = E_OK;
    }
    else if ((ADC_CONV_MODE_ONESHOT == AdcConfigPtr->groupConfigPtr[group].conversionMode) &&
             (ADC_STREAM_COMPLETED  == AdcConfigPtr->groupConfigPtr[group].status->groupStatus))
    {
      /* ADC330. */
      AdcConfigPtr->groupConfigPtr[group].status->groupStatus = ADC_IDLE;

      returnValue = E_OK;
    }
    else
    {
      /* Keep status. */
      returnValue = E_OK;
    }

    if (E_OK == returnValue)
    {
      /* Copy the result to application buffer. */
#if defined(CFG_MPC5606S)

      for (channel = 0; channel < AdcConfigPtr->groupConfigPtr[group].numberOfChannels; channel++)
  {
    if(ADC_CONV_MODE_ONESHOT == AdcConfigPtr->groupConfigPtr[s_CurrGroupId].conversionMode)
    {
      dataBufferPtr[channel] = (uint16)(AdcConfigPtr->groupConfigPtr[group].status->resultBufferPtr[channel]);
    }
    else
    {
      if(ADC_ACCESS_MODE_SINGLE == AdcConfigPtr->groupConfigPtr[s_CurrGroupId].accessMode )
      {
        dataBufferPtr[channel] = (uint16)(AdcConfigPtr->groupConfigPtr[group].status->resultBufferPtr[channel]);
      }
      else
      {
        Adc_ValueGroupType *lastSamplePointer = AdcConfigPtr->groupConfigPtr[group].status->resultBufferPtr;
        lastSamplePointer += (AdcConfigPtr->groupConfigPtr[group].numberOfChannels) * (s_CurrSampleCount % AdcConfigPtr->groupConfigPtr[group].streamNumSamples);
        dataBufferPtr[channel] = lastSamplePointer[channel];
	    }
    }
  }
        
#else

    for (channel = 0; channel < AdcConfigPtr->groupConfigPtr[group].numberOfChannels; channel++)
    {
       dataBufferPtr[channel] = AdcConfigPtr->groupConfigPtr[group].resultBuffer[channel];
    }
#endif

    }
  }
  else
  {
    /* An error have been raised from Adc_CheckReadGroup(). */
    returnValue = E_NOT_OK;
  }

  return (returnValue);
}
#endif

Adc_StatusType Adc_GetGroupStatus (Adc_GroupType group)
{
  Adc_StatusType returnValue;
  if ((ADC_INIT == adcState) && (AdcConfigPtr != NULL))
  {
    /* Adc initilised, OK to move on... */
    returnValue = AdcConfigPtr->groupConfigPtr[group].status->groupStatus;
  }
  else
  {
    returnValue = ADC_IDLE;
#if ( ADC_DEV_ERROR_DETECT == STD_ON )
    Det_ReportError(MODULE_ID_ADC,0,ADC_GETGROUPSTATUS_ID, ADC_E_UNINIT );
#endif
    }

  return (returnValue);
}

#if defined(CFG_MPC5606S)
void Adc_Group0ConversionComplete (void)
{
	/* Clear ECH Flag and disable interruput */
	ADC_0.ISR.B.ECH = 1;
	ADC_0.IMR.B.MSKECH = 0;

	Adc_GroupDefType adcGroup = AdcConfigPtr->groupConfigPtr[s_CurrGroupId];
#ifdef DONT_USE_DMA_IN_ADC_MPC5606S
	/* Copy to result buffer */
	for(uint8 index=0; index < adcGroup.numberOfChannels; index++)
	{
		adcGroup.status->resultBufferPtr[index] = ADC_0.CDR[32+adcGroup.channelList[index]].B.CDATA;
	}
#endif
 
    if(ADC_CONV_MODE_ONESHOT == AdcConfigPtr->groupConfigPtr[s_CurrGroupId].conversionMode)
    {
    	adcGroup.status->groupStatus = ADC_STREAM_COMPLETED;
    }
    else
    {
      if(ADC_ACCESS_MODE_SINGLE == adcGroup.accessMode )
      {
    	  adcGroup.status->groupStatus = ADC_STREAM_COMPLETED;

        /* Disable trigger normal conversions for ADC0 */
        ADC_0.MCR.B.NSTART = 0;
      }
      else
      {
        if(ADC_STREAM_BUFFER_LINEAR == adcGroup.streamBufferMode)
        {
            s_CurrSampleCount++;
            if(s_CurrSampleCount < adcGroup.streamNumSamples)
            {
#ifdef DONT_USE_DMA_IN_ADC_MPC5606S
              adcGroup.status->resultBufferPtr += adcGroup.numberOfChannels;
#endif
              adcGroup.status->groupStatus = ADC_COMPLETED;
              
              ADC_0.IMR.B.MSKECH = 1;
            }
            else
            {
              /* Sample completed. */
            	adcGroup.status->groupStatus = ADC_STREAM_COMPLETED;

              /* Disable ECH interrupt */
              ADC_0.IMR.B.MSKECH = 0;

              /* Abort conversion */
              ADC_0.MCR.B.ABORTCHAIN = 1;

              /* Disable trigger normal conversions for ADC0 */
              ADC_0.MCR.B.NSTART=0;
            }
        }
        else if(ADC_STREAM_BUFFER_CIRCULAR == adcGroup.streamBufferMode)
        {
#ifdef DONT_USE_DMA_IN_ADC_MPC5606S
        	  static Adc_ValueGroupType *oldResultBufferPtr = 0;
            
            if( s_CurrSampleCount == 0) {
            	oldResultBufferPtr = adcGroup.status->resultBufferPtr;
            }
#endif
            s_CurrSampleCount++;
            if(s_CurrSampleCount < adcGroup.streamNumSamples)
            {
#ifdef DONT_USE_DMA_IN_ADC_MPC5606S
            	adcGroup.status->resultBufferPtr += adcGroup.numberOfChannels;
#endif
            	adcGroup.status->groupStatus = ADC_COMPLETED;
                
                ADC_0.IMR.B.MSKECH = 1;
            }
            else
            {
              /* Sample completed. */
            	adcGroup.status->groupStatus = ADC_STREAM_COMPLETED;

              s_CurrSampleCount = 0;

#ifndef DONT_USE_DMA_IN_ADC_MPC5606S
              Dma_ConfigureDestinationAddress((uint32_t)adcGroup.status->resultBufferPtr, DMA_ADC_GROUP0_RESULT_CHANNEL);
#else
              adcGroup.status->resultBufferPtr = oldResultBufferPtr;
#endif
              loopCnt++;

              /* Enable  ECH interrupt */
              ADC_0.IMR.B.MSKECH = 1;
            }
        }
        else
        {
            //nothing to do.
        }
      }
    }

  /* Call notification if enabled. */
#if (ADC_GRP_NOTIF_CAPABILITY == STD_ON)
  if (adcGroup.status->notifictionEnable && adcGroup.groupCallback != NULL)
  {
	  adcGroup.groupCallback();
  }
#endif
}

void Adc_WatchdogError (void)
{

}

void Adc_ADCError (void)
{

}

#else
void Adc_Group0ConversionComplete (void)
{
  /* ISR for FIFO 0 end of queue. Clear interrupt flag.  */
  EQADC.FISR[ADC_EQADC_QUEUE_0].B.EOQF = 1;

  /* Sample completed. */
  AdcConfigPtr->groupConfigPtr[ADC_GROUP0].status->groupStatus = ADC_STREAM_COMPLETED;

  /* Call notification if enabled. */
#if (ADC_GRP_NOTIF_CAPABILITY == STD_ON)
  if (AdcConfigPtr->groupConfigPtr[ADC_GROUP0].status->notifictionEnable && AdcConfigPtr->groupConfigPtr[ADC_GROUP0].groupCallback != NULL)
  {
    AdcConfigPtr->groupConfigPtr[ADC_GROUP0].groupCallback();
  }
#endif
}
void Adc_Group1ConversionComplete (void)
{
  /* ISR for FIFO 0 end of queue. Clear interrupt flag.  */
  EQADC.FISR[ADC_EQADC_QUEUE_1].B.EOQF = 1;

  /* Sample completed. */
  AdcConfigPtr->groupConfigPtr[ADC_GROUP1].status->groupStatus = ADC_STREAM_COMPLETED;

  /* Call notification if enabled. */
#if (ADC_GRP_NOTIF_CAPABILITY == STD_ON)
  if (AdcConfigPtr->groupConfigPtr[ADC_GROUP1].status->notifictionEnable && AdcConfigPtr->groupConfigPtr[ADC_GROUP1].groupCallback != NULL)
  {
    AdcConfigPtr->groupConfigPtr[ADC_GROUP1].groupCallback();
  }
#endif
}

void Adc_EQADCError (void)
{
  /* Something is wrong!! Check the cause of the error and try to correct it. */
  if (EQADC.FISR[ADC_EQADC_QUEUE_0].B.TORF)
  {
    /* Trigger overrun on queue 0!! */
    assert (0);
  }
  else if (EQADC.FISR[ADC_EQADC_QUEUE_1].B.TORF)
  {
    /* Trigger overrun on queue 1!! */
    assert (0);
  }
  else if (EQADC.FISR[ADC_EQADC_QUEUE_0].B.CFUF)
  {
    /* Command underflow on queue 0!! */
    assert (0);
  }
  else if (EQADC.FISR[ADC_EQADC_QUEUE_1].B.CFUF)
  {
    /* Command underflow on queue 1!! */
    assert (0);
  }
  else if (EQADC.FISR[ADC_EQADC_QUEUE_0].B.RFOF)
  {
    /* Result overflow on queue 0!! */
    assert (0);
  }
  else if (EQADC.FISR[ADC_EQADC_QUEUE_1].B.RFOF)
  {
    /* Result overflow on queue 1!! */
    assert (0);
  }
  else
  {
    /* Something else... TODO What have we missed above */
    assert(0);
  }
}

/* Helper macro to make sure that the qommand queue have
 * executed the commands in the fifo.
 * First check that the H/W negate the
 * single scan bit and then wait for EOQ. */
#define WAIT_FOR_QUEUE_TO_FINISH(q) \
  while (EQADC.FISR[q].B.SSS)             \
  {                                       \
    ;                                     \
  }                                       \
                                          \
  while (!EQADC.FISR[q].B.EOQF)           \
  {                                       \
    ;                                     \
  }

static void Adc_WriteEQADCRegister (Adc_EQADCRegisterType reg, Adc_EQADCRegister value)
{
  Adc_RegisterWriteType writeReg;
  uint32_t temp, oldMode;

  writeReg.R = 0;

  /* Write command. */
  writeReg.B.RW = 0;
  writeReg.B.EOQ = 1;
  writeReg.B.ADC_REG = value;
  writeReg.B.ADC_REG_ADDR = reg;

  /* Invalidate queue. */
  EQADC.CFCR[ADC_EQADC_QUEUE_0].B.CFINV = 1;


  /* Write command through FIFO. */
  EQADC.CFPR[ADC_EQADC_QUEUE_0].R = writeReg.R;

  /* Enable FIFO. */
  oldMode = EQADC.CFCR[ADC_EQADC_QUEUE_0].B.MODE;
  EQADC.CFCR[ADC_EQADC_QUEUE_0].B.MODE = ADC_CONV_MODE_ONESHOT;
  EQADC.CFCR[ADC_EQADC_QUEUE_0].B.SSE = 1;

  /* Wait for command to be executed. */
  WAIT_FOR_QUEUE_TO_FINISH(ADC_EQADC_QUEUE_0);

  /* Flush result buffer. */
  temp = EQADC.RFPR[ADC_EQADC_QUEUE_0].R;
  EQADC.FISR[ADC_EQADC_QUEUE_0].B.EOQF = 1;

  EQADC.CFCR[ADC_EQADC_QUEUE_0].B.MODE = oldMode;

}

static Adc_EQADCRegister Adc_ReadEQADCRegister (Adc_EQADCRegisterType reg)
{
  Adc_RegisterReadType readReg;
  Adc_EQADCRegister result;
  uint32_t oldMode, dmaRequestEnable;

  readReg.R = 0;

  /* Read command. */
  readReg.B.RW = 1;
  readReg.B.EOQ = 1;
  readReg.B.ADC_REG_ADDR = reg;
  readReg.B.MESSAGE_TAG = ADC_EQADC_QUEUE_0;

  /* Make sure that DMA requests for command fill and result drain is disabled. */
  if (EQADC.IDCR[ADC_EQADC_QUEUE_0].B.RFDE || EQADC.IDCR[ADC_EQADC_QUEUE_0].B.CFFE)
  {
    EQADC.IDCR[ADC_EQADC_QUEUE_0].B.CFFE = 0;
    EQADC.IDCR[ADC_EQADC_QUEUE_0].B.RFDE = 0;

    /* Remember to enable requests again... */
    dmaRequestEnable = TRUE;
  }
  else
  {
    dmaRequestEnable = FALSE;
  }

  /* Invalidate queue. */
  EQADC.CFCR[ADC_EQADC_QUEUE_0].B.CFINV = 1;

  /* Write command through FIFO. */
  EQADC.CFPR[ADC_EQADC_QUEUE_0].R = readReg.R;

  /* Enable FIFO. */
  oldMode = EQADC.CFCR[ADC_EQADC_QUEUE_0].B.MODE;
  EQADC.CFCR[ADC_EQADC_QUEUE_0].B.MODE = ADC_CONV_MODE_ONESHOT;
  EQADC.CFCR[ADC_EQADC_QUEUE_0].B.SSE = 1;

  /* Wait for command to be executed. */
  WAIT_FOR_QUEUE_TO_FINISH(ADC_EQADC_QUEUE_0);

  /* Read result buffer. */
  result = EQADC.RFPR[ADC_EQADC_QUEUE_0].R;
  EQADC.FISR[ADC_EQADC_QUEUE_0].B.EOQF = 1;

  EQADC.CFCR[ADC_EQADC_QUEUE_0].B.MODE = oldMode;

  if (dmaRequestEnable)
  {
    EQADC.IDCR[ADC_EQADC_QUEUE_0].B.CFFE = 1;
    EQADC.IDCR[ADC_EQADC_QUEUE_0].B.RFDE = 1;
  }
  else
  {
    /* Do nothing. */
  }
  return (result);
}
#endif 

#define SYSTEM_CLOCK_DIVIDE(f)    ((f / 2) - 1)

#if (ADC_GRP_NOTIF_CAPABILITY == STD_ON)
void Adc_EnableGroupNotification (Adc_GroupType group)
{
  AdcConfigPtr->groupConfigPtr[group].status->notifictionEnable = 1;
}

void Adc_DisableGroupNotification (Adc_GroupType group)
{
  AdcConfigPtr->groupConfigPtr[group].status->notifictionEnable = 0;
}
#endif

#if defined (CFG_MPC5606S)
static void  Adc_ConfigureADC (const Adc_ConfigType *ConfigPtr)
{
  /* Set ADC CLOCK */
  ADC_0.MCR.B.ADCLKSEL = ConfigPtr->hwConfigPtr->adcPrescale;

  ADC_0.DSDR.B.DSD = 254;

  /* Power on ADC */
  ADC_0.MCR.B.PWDN = 0;

  /* Enable DMA. */
  ADC_0.DMAE.B.DMAEN = 1;
}

void Adc_ConfigureADCInterrupts (void)
{
	ISR_INSTALL_ISR2(  "Adc_Err", Adc_ADCError, ADC_ER_INT,     2, 0 );
	ISR_INSTALL_ISR2(  "Adc_Grp", Adc_Group0ConversionComplete, ADC_EOC_INT,     2, 0 );
	ISR_INSTALL_ISR2(  "Adc_Wdg", Adc_WatchdogError, ADC_WD_INT,     2, 0 );
}

#if (ADC_ENABLE_START_STOP_GROUP_API == STD_ON)
void Adc_StartGroupConversion (Adc_GroupType group)
{
  uint32 groupChannelIdMask = 0;
  
  s_CurrGroupId = group;
  s_CurrSampleCount = 0;
  
   /* Run development error check. */
  if (E_OK == Adc_CheckStartGroupConversion (group))
  {
#ifndef DONT_USE_DMA_IN_ADC_MPC5606S
   	  Dma_ConfigureChannel ((struct tcd_t *)(AdcConfigPtr->groupConfigPtr[group].groupDMAResults), DMA_ADC_GROUP0_RESULT_CHANNEL);
	  Dma_ConfigureDestinationAddress ((uint32_t)AdcConfigPtr->groupConfigPtr[group].status->resultBufferPtr, DMA_ADC_GROUP0_RESULT_CHANNEL);
#endif
	  /* Set conversion mode. */
	  ADC_0.MCR.B.MODE = AdcConfigPtr->groupConfigPtr[group].conversionMode;

	  /* Enable Overwrite*/
	  ADC_0.MCR.B.OWREN = 1;

	  /* Set Conversion Time. */
	  ADC_0.CTR[1].B.INPLATCH = AdcConfigPtr->groupConfigPtr[group].adcChannelConvTime.INPLATCH;
	  ADC_0.CTR[1].B.INPCMP = AdcConfigPtr->groupConfigPtr[group].adcChannelConvTime.INPCMP;
	  ADC_0.CTR[1].B.INPSAMP = AdcConfigPtr->groupConfigPtr[group].adcChannelConvTime.INPSAMP;

	  /* Set group state to BUSY. */
	  AdcConfigPtr->groupConfigPtr[group].status->groupStatus = ADC_BUSY;

	  for(uint8 i =0; i < AdcConfigPtr->groupConfigPtr[group].numberOfChannels; i++)
	  {
            groupChannelIdMask |= (1 << AdcConfigPtr->groupConfigPtr[group].channelList[i]);
	  }

        /* Enable Normal conversion */
        ADC_0.NCMR[1].R = groupChannelIdMask;

#ifndef DONT_USE_DMA_IN_ADC_MPC5606S
        ADC_0.DMAE.R = 0x01;

        /* Enable DMA Transfer */
        ADC_0.DMAR[1].R = groupChannelIdMask;
#endif

        /* Enable Channel Interrupt */
        ADC_0.CIMR[1].R = groupChannelIdMask;

        /* Enable ECH interrupt */
        ADC_0.IMR.B.MSKECH = 1;

#ifndef DONT_USE_DMA_IN_ADC_MPC5606S
        EDMA.SERQR.R = DMA_ADC_GROUP0_RESULT_CHANNEL;        /* Enable EDMA channel for ADC */

        EDMA.TCD[0].START  = 0;
#endif

        /* Trigger normal conversions for ADC0 */
        ADC_0.MCR.B.NSTART = 1;

        AdcConfigPtr->groupConfigPtr[group].status->isConversionStarted = TRUE;
  }
  else
  {
	/* Error have been set within Adc_CheckStartGroupConversion(). */
  }
}

void Adc_StopGroupConversion (Adc_GroupType group)
{
  if (E_OK == Adc_CheckStopGroupConversion (group))
  {
	/* Abort conversion */
	ADC_0.MCR.B.ABORTCHAIN = 1;

	/* Disable trigger normal conversions for ADC0 */
	ADC_0.MCR.B.NSTART = 0;

	/* Set group state to IDLE. */
	AdcConfigPtr->groupConfigPtr[group].status->groupStatus = ADC_IDLE;

	/* Disable group notification. */
	Adc_DisableGroupNotification (group);
  }
  else
  {
	/* Error have been set within Adc_CheckStartGroupConversion(). */
  }
}
#endif  /* endof #if (ADC_ENABLE_START_STOP_GROUP_API == STD_ON) */

#else
static void  Adc_ConfigureEQADC (const Adc_ConfigType *ConfigPtr)
{
  Adc_GroupType group;

  enum
  {
    ADC_ENABLE = 0x8000,
  };
  /* Enable ADC0. */
  Adc_WriteEQADCRegister (ADC0_CR, (ADC_ENABLE | ConfigPtr->hwConfigPtr->adcPrescale));

  /* Disable time stamp timer. */
  Adc_WriteEQADCRegister (ADC0_TSCR, 0);

  for (group = ADC_GROUP0; group < ConfigPtr->nbrOfGroups; group++)
  {
    /* Enable eDMA requests for commands and results. */
    EQADC.IDCR[group].B.CFFS = 1;
    EQADC.IDCR[group].B.RFDS = 1;

    /* Invalidate FIFO. */
    EQADC.CFCR[group].B.CFINV = 1;

    /* Enable FIFO fill requests. */
    EQADC.IDCR[group].B.CFFE = 1;
    EQADC.IDCR[group].B.RFDE = 1;
  }
}

void Adc_ConfigureEQADCInterrupts (void)
{
  Adc_GroupType group;

	ISR_INSTALL_ISR2( "Adc_Err", Adc_EQADCError, EQADC_FISR_OVER,     2, 0);
	ISR_INSTALL_ISR2( "Adc_Grp0", Adc_Group0ConversionComplete, EQADC_FISR0_EOQF0,     2, 0);
	ISR_INSTALL_ISR2( "Adc_Grp1", Adc_Group1ConversionComplete, EQADC_FISR1_EOQF1,     2, 0);
  for (group = ADC_GROUP0; group < AdcConfigPtr->nbrOfGroups; group++)
  {
    /* Enable end of queue, queue overflow/underflow interrupts. Clear corresponding flags. */
    EQADC.FISR[group].B.RFOF = 1;
    EQADC.IDCR[group].B.RFOIE = 1;

    EQADC.FISR[group].B.CFUF = 1;
    EQADC.IDCR[group].B.CFUIE = 1;

    EQADC.FISR[group].B.TORF = 1;
    EQADC.IDCR[group].B.TORIE = 1;

    EQADC.FISR[group].B.EOQF = 1;
    EQADC.IDCR[group].B.EOQIE = 1;
  }
}

#if (ADC_ENABLE_START_STOP_GROUP_API == STD_ON)
void Adc_StartGroupConversion (Adc_GroupType group)
{
  /* Run development error check. */
  if (E_OK == Adc_CheckStartGroupConversion (group))
  {
    /* Set conversion mode. */
    EQADC.CFCR[group].B.MODE = AdcConfigPtr->groupConfigPtr[group].conversionMode;

    /* Set single scan enable bit if this group is one shot. */
    if (AdcConfigPtr->groupConfigPtr[group].conversionMode == ADC_CONV_MODE_ONESHOT)
    {
      EQADC.CFCR[group].B.SSE = 1;

      /* Set group state to BUSY. */
      AdcConfigPtr->groupConfigPtr[group].status->groupStatus = ADC_BUSY;
    }
  }
  else
  {
    /* Error have been set within Adc_CheckStartGroupConversion(). */
  }
}


#endif
static void Adc_EQADCCalibrationSequence (void)
{
  Adc_ValueGroupType calibrationResult[sizeof(AdcCalibrationCommandQueue)/sizeof(AdcCalibrationCommandQueue[0])];
  int32_t point25Average, point75Average, i;
  Adc_EQADCRegister tempGCC, tempOCC;
  enum
  {
    IDEAL_RES25 = 0x1000,
    IDEAL_RES75 = 0x3000,
  };

  /* Use group 0 DMA channel for calibration. */
  Dma_ConfigureChannel ((struct tcd_t *)&AdcCalibrationDMACommandConfig,DMA_ADC_GROUP0_COMMAND_CHANNEL);
  Dma_ConfigureChannelTranferSize (sizeof(AdcCalibrationCommandQueue)/sizeof(AdcCalibrationCommandQueue[0]),
		                           DMA_ADC_GROUP0_COMMAND_CHANNEL);
  Dma_ConfigureChannelSourceCorr (-sizeof(AdcCalibrationCommandQueue), DMA_ADC_GROUP0_COMMAND_CHANNEL);

  Dma_ConfigureChannel ((struct tcd_t *)&AdcCalibrationDMAResultConfig, DMA_ADC_GROUP0_RESULT_CHANNEL);
  Dma_ConfigureChannelTranferSize (sizeof(calibrationResult)/sizeof(calibrationResult[0]),
		                           DMA_ADC_GROUP0_RESULT_CHANNEL);
  Dma_ConfigureChannelDestinationCorr (-sizeof(calibrationResult), DMA_ADC_GROUP0_RESULT_CHANNEL);
  Dma_ConfigureDestinationAddress ((uint32_t)calibrationResult, DMA_ADC_GROUP0_RESULT_CHANNEL);

  /* Invalidate queues. */
  EQADC.CFCR[ADC_EQADC_QUEUE_0].B.CFINV = 1;

  Dma_StartChannel (DMA_ADC_GROUP0_COMMAND_CHANNEL);
  Dma_StartChannel (DMA_ADC_GROUP0_RESULT_CHANNEL);

  /* Start conversion. */
  EQADC.CFCR[ADC_EQADC_QUEUE_0].B.MODE = ADC_CONV_MODE_ONESHOT;
  EQADC.CFCR[ADC_EQADC_QUEUE_0].B.SSE = 1;

  /* Wait for conversion to complete. */
  while(!Dma_ChannelDone (DMA_ADC_GROUP0_RESULT_CHANNEL))
  {
    ;
  }

  /* Stop DMA channels and write calibration data to ADC engine. */
  EQADC.CFCR[ADC_EQADC_QUEUE_0].B.MODE = ADC_CONV_MODE_DISABLED;
  Dma_StopChannel (DMA_ADC_GROUP0_COMMAND_CHANNEL);
  Dma_StopChannel (DMA_ADC_GROUP0_RESULT_CHANNEL);

  /* Calculate conversion factors and write to ADC. */
  point25Average = 0;
  point75Average = 0;
  for (i = 0; i < sizeof(calibrationResult)/sizeof(calibrationResult[0] / 2); i++)
  {
    point25Average += calibrationResult[i];
    point75Average += calibrationResult[i + sizeof(calibrationResult)/sizeof(calibrationResult[0]) / 2];
  }

  /* Calculate average and correction slope and offset.  */
  point25Average /= (sizeof(calibrationResult)/sizeof(calibrationResult[0]) / 2);
  point75Average /= (sizeof(calibrationResult)/sizeof(calibrationResult[0]) / 2);

  tempGCC = ((IDEAL_RES75 - IDEAL_RES25) << 14) / (point75Average - point25Average);
  tempOCC = IDEAL_RES75 - ((tempGCC * point75Average) >> 14) - 2;

  /* GCC field is only 15 bits. */
  tempGCC = tempGCC & ~(1 << 15);

  /* OCC field is only 14 bits. */
  tempOCC = tempOCC & ~(3 << 14);

  /* Write calibration data to ADC engine. */
  Adc_WriteEQADCRegister (ADC0_GCCR, tempGCC);
  Adc_WriteEQADCRegister (ADC0_OCCR, tempOCC);

  /* Read back and check calibration values. */
  if (Adc_ReadEQADCRegister (ADC0_GCCR) != tempGCC)
  {
    assert (0);
  }
  else if (Adc_ReadEQADCRegister (ADC0_OCCR) != tempOCC)
  {
    assert (0);
  }
}
#endif

/* Development error checking functions. */
#if (ADC_READ_GROUP_API == STD_ON)
static Std_ReturnType Adc_CheckReadGroup (Adc_GroupType group)
{
  Std_ReturnType returnValue;

#if ( ADC_DEV_ERROR_DETECT == STD_ON )

  if (ADC_UNINIT == adcState)
  {
    /* ADC296. */
    returnValue = E_NOT_OK;
    Det_ReportError(MODULE_ID_ADC,0,ADC_READGROUP_ID ,ADC_E_UNINIT );
  }
  else if ((group < ADC_GROUP0) || (group >= AdcConfigPtr->nbrOfGroups))
  {
    /* ADC152. */
    returnValue = E_NOT_OK;
    Det_ReportError(MODULE_ID_ADC,0,ADC_READGROUP_ID ,ADC_E_PARAM_GROUP );
  }
  else if ((ADC_IDLE == AdcConfigPtr->groupConfigPtr[group].status->groupStatus) 
    && (!AdcConfigPtr->groupConfigPtr[group].status->isConversionStarted) )
  {
    /* ADC388. */
    returnValue = E_NOT_OK;
    Det_ReportError(MODULE_ID_ADC,0,ADC_READGROUP_ID ,ADC_E_IDLE );
  }
  else
  {
    /* Nothing strange. Go on... */
    returnValue = E_OK;
  }
#else
  returnValue = E_OK;
#endif
  return (returnValue);
}
#endif

#if (ADC_ENABLE_START_STOP_GROUP_API == STD_ON)
static Std_ReturnType Adc_CheckStartGroupConversion (Adc_GroupType group)
{
  Std_ReturnType returnValue;
#if ( ADC_DEV_ERROR_DETECT == STD_ON )
  if (!(ADC_INIT == adcState))
  {
    /* ADC not initialised, ADC294. */
    Det_ReportError(MODULE_ID_ADC,0,ADC_STARTGROUPCONVERSION_ID, ADC_E_UNINIT );
    returnValue = E_NOT_OK;
  }
  else  if (!((group >= 0) && (group < AdcConfig->nbrOfGroups)))
  {
    /* Wrong group ID, ADC125 */
    Det_ReportError(MODULE_ID_ADC,0,ADC_STARTGROUPCONVERSION_ID, ADC_E_PARAM_GROUP );
    returnValue = E_NOT_OK;
  }
  else  if ( NULL == AdcConfigPtr->groupConfigPtr[group].status->resultBufferPtr )
    {
      /* ResultBuffer not set, ADC424 */
	  Det_ReportError(MODULE_ID_ADC,0,ADC_STARTGROUPCONVERSION_ID, ADC_E_BUFFER_UNINIT );
	  returnValue = E_NOT_OK;
    }
  else if (!(ADC_TRIGG_SRC_SW == AdcConfigPtr->groupConfigPtr[group].triggerSrc))
  {
    /* Wrong trig source, ADC133. */
    Det_ReportError(MODULE_ID_ADC,0,ADC_STARTGROUPCONVERSION_ID, ADC_E_WRONG_TRIGG_SRC);
    returnValue = E_NOT_OK;
  }
  else if (!((ADC_IDLE             == AdcConfigPtr->groupConfigPtr[group].status->groupStatus) ||
             (ADC_STREAM_COMPLETED == AdcConfigPtr->groupConfigPtr[group].status->groupStatus)))
  {
    /* Group status not OK, ADC351, ADC428 */
    Det_ReportError(MODULE_ID_ADC,0,ADC_STARTGROUPCONVERSION_ID, ADC_E_BUSY );

    /*
     * This is a BUG!
     * Sometimes the ADC-interrupt gets lost which means that the status is never reset to ADC_IDLE (done in Adc_ReadGroup).
     * Therefor another group conversion is never started...
     *
     * The temporary fix is to always return E_OK here. But the reason for the bug needs to be investigated further.
     */
    //returnValue = E_NOT_OK;
    returnValue = E_OK;
  }
  else
  {
    returnValue = E_OK;
  }
#else
  returnValue = E_OK;
#endif
  return (returnValue);
}

static Std_ReturnType Adc_CheckStopGroupConversion (Adc_GroupType group)
{
  Std_ReturnType returnValue;
	#if ( ADC_DEV_ERROR_DETECT == STD_ON )
	  if (!(ADC_INIT == adcState))
	  {
		/* ADC not initialized, ADC295. */
		Det_ReportError(MODULE_ID_ADC,0,ADC_STOPGROUPCONVERSION_ID, ADC_E_UNINIT );
		returnValue = E_NOT_OK;
	  }
	  else  if (!((group >= 0) && (group < AdcConfig->nbrOfGroups)))
	  {
		/* Wrong group ID, ADC126 */
		Det_ReportError(MODULE_ID_ADC,0,ADC_STOPGROUPCONVERSION_ID, ADC_E_PARAM_GROUP );
		returnValue = E_NOT_OK;
	  }
	  else if (!(ADC_TRIGG_SRC_SW == AdcConfigPtr->groupConfigPtr[group].triggerSrc))
	  {
		/* Wrong trig source, ADC164. */
		Det_ReportError(MODULE_ID_ADC,0,ADC_STOPGROUPCONVERSION_ID, ADC_E_WRONG_TRIGG_SRC);
		returnValue = E_NOT_OK;
	  }
	  else if (ADC_IDLE == AdcConfigPtr->groupConfigPtr[group].status->groupStatus)
	  {
		/* Group status not OK, ADC241 */
		Det_ReportError(MODULE_ID_ADC,0,ADC_STOPGROUPCONVERSION_ID, ADC_IDLE );
		returnValue = E_NOT_OK;
	  }
	  else
	  {
		returnValue = E_OK;
	  }
	#else
	  returnValue = E_OK;
	#endif
  return (returnValue);
}
#endif

static Std_ReturnType Adc_CheckInit (const Adc_ConfigType *ConfigPtr)
{
  Std_ReturnType returnValue;

#if ( ADC_DEV_ERROR_DETECT == STD_ON )
  if (!(ADC_UNINIT == adcState))
  {
    /* Oops, already initialised. */
    Det_ReportError(MODULE_ID_ADC,0,ADC_INIT_ID, ADC_E_ALREADY_INITIALIZED );
    returnValue = E_NOT_OK;
  }
  else if (ConfigPtr == NULL)
  {
    /* Wrong config! */
    Det_ReportError(MODULE_ID_ADC,0,ADC_INIT_ID, ADC_E_PARAM_CONFIG );
    returnValue = E_NOT_OK;
  }
  else
  {
    /* Looks good!! */
    returnValue = E_OK;
  }
#else
    returnValue = E_OK;
#endif
  return (returnValue);
}

static Std_ReturnType Adc_CheckDeInit (void)
{
	Std_ReturnType returnValue;

#if ( ADC_DEV_ERROR_DETECT == STD_ON )
	if (ADC_UNINIT == adcState)
	{
		/* Oops, already initialised. */
		Det_ReportError(MODULE_ID_ADC,0,ADC_DEINIT_ID, ADC_E_UNINIT );
		returnValue = E_NOT_OK;
	}
	else
	{
		/* Looks good!! */
		returnValue = E_OK;
	}
	for (Adc_GroupType group = ADC_GROUP0; group < AdcConfigPtr->nbrOfGroups; group++)
	{
		/*  Check ADC is IDLE or COMPLETE*/
		if((AdcConfigPtr->groupConfigPtr[group].status->groupStatus != ADC_IDLE)||(AdcConfigPtr->groupConfigPtr[group].status->groupStatus == ADC_STREAM_COMPLETED))
		{
			Det_ReportError(MODULE_ID_ADC,0,ADC_DEINIT_ID, ADC_E_BUSY );
			returnValue = E_NOT_OK;
		}
		else
		{
			returnValue = E_OK;
		}
	}
#else
	returnValue = E_OK;
#endif
	return (returnValue);
}

static Std_ReturnType Adc_CheckSetupResultBuffer (Adc_GroupType group)
{
  Std_ReturnType returnValue;

#if ( ADC_DEV_ERROR_DETECT == STD_ON )
  if (ADC_UNINIT == adcState)
  {
    /* Driver not initialised. */
    Det_ReportError(MODULE_ID_ADC,0,ADC_SETUPRESULTBUFFER_ID,ADC_E_UNINIT );
    returnValue = E_NOT_OK;
  }
  else if (group > AdcConfigPtr->nbrOfGroups)
  {
    /* ADC423 */
    Det_ReportError(MODULE_ID_ADC,0,ADC_SETUPRESULTBUFFER_ID,ADC_E_PARAM_GROUP );
    returnValue = E_NOT_OK;
  }
  else
  {
    /* Looks good!! */
    returnValue = E_OK;
  }
#else
  returnValue = E_OK;
#endif
  return (returnValue);
}


