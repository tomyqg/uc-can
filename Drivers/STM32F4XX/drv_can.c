/*
*********************************************************************************************************
*                                              uC/CAN
*                                      The Embedded CAN suite
*
*                 Copyright by Embedded Office GmbH & Co. KG www.embedded-office.com
*                    Copyright 1992-2020 Silicon Laboratories Inc. www.silabs.com
*
*                                 SPDX-License-Identifier: APACHE-2.0
*
*               This software is subject to an open source license and is distributed by
*                Silicon Laboratories Inc. pursuant to the terms of the Apache License,
*                    Version 2.0 available at www.apache.org/licenses/LICENSE-2.0.
*
*********************************************************************************************************
*/


/*
****************************************************************************************************
* Filename : drv_can.c
* Version  : V2.42.01
****************************************************************************************************
*/

/*
****************************************************************************************************
*                                             INCLUDES
****************************************************************************************************
*/

#include "drv_can.h"
#include "drv_def.h"
#include "drv_can_reg.h"

/*
****************************************************************************************************
*                                            LOCAL DATA
****************************************************************************************************
*/

/*------------------------------------------------------------------------------------------------*/
/*!
* \brief    DRIVER IDENTIFICATION CODE
*
* \ingroup  STM32F4XX_CAN
*
*           This constant holds the unique driver identification code.
*/
/*------------------------------------------------------------------------------------------------*/
static const CPU_INT32U DrvIdent = 0x243F1501;

/*------------------------------------------------------------------------------------------------*/
/*!
* \brief    DRIVER ERROR CODE
*
* \ingroup  STM32F4XX_CAN
*
*           This variable holds the detailed error code if an error is detected.
*/
/*------------------------------------------------------------------------------------------------*/
static CPU_INT16U DrvError;

/*
****************************************************************************************************
*                                           GLOBAL DATA
****************************************************************************************************
*/

/*------------------------------------------------------------------------------------------------*/
/*!
* \brief    DEVICE RUNTIME DATA
*
* \ingroup  STM32F4XX_CAN
*
*           This variable holds the device runtime data.
*/
/*------------------------------------------------------------------------------------------------*/
STM32F4XX_CAN_DATA DevData[STM32F4XX_CAN_N_DEV];

/*
****************************************************************************************************
*                                            FUNCTIONS
****************************************************************************************************
*/

/*------------------------------------------------------------------------------------------------*/
/*!
* \brief    CAN INITIALIZATION
*
* \ingroup  STM32F4XX_CAN
*
*           Initializes the CAN device with the given device name.
*
* \param    arg  the CAN device name
*
* \return   error code (0 if OK, -1 if an error occurred)
*/
/*------------------------------------------------------------------------------------------------*/
CPU_INT16S STM32F4XXCANInit (CPU_INT32U arg)
{
    STM32F4XX_CAN_DATA *dev;                          /* Local: Pointer to can device             */
    STM32F4XX_CAN_t    *can;                          /* Local: Pointer to can register           */
    STM32F4XX_CAN_t    *can0;                         /* Local: Pointer to can register           */
    CPU_INT32U          i;                            /* Local: loop variable                     */
    CPU_INT16S          result = -1;                  /* Local: Function result                   */
                                                      /*------------------------------------------*/
#if STM32F4XX_CAN_ARG_CHK_CFG > 0                     /* Checking arguments (if enabled)          */
    if (arg >= STM32F4XX_CAN_N_DEV) {                 /* can dev out of range?                    */
        DrvError = STM32F4XX_CAN_INIT_ERR;
        return (result);
    }
#endif                                                /*------------------------------------------*/
    DrvError = STM32F4XX_CAN_NO_ERR;                  /* reset to 0                               */
                                                      /*------------------------------------------*/
    dev = &DevData[arg];                              /* set pointer to can device                */
    dev->Use = 0;                                     /* set device to unused                     */
                                                      /*------------------------------------------*/
    STM32F4XXCAN_PinCfg(arg);                         /* apply pin configuration                  */
                                                      /*------------------------------------------*/
    if (arg == STM32F4XX_CAN_BUS_0) {
        dev->Base = (void *)STM32F4XX_CAN1_BASE;      /* set CAN 1 base address                   */
                                                      /* RCC_APB1ENR: enable CAN 1 clock          */
        *(volatile CPU_INT32U *)0x40023840 |= 0x02000000;
    } else {
                                                      /* set CAN 1 base address                   */
        DevData[STM32F4XX_CAN_BUS_0].Base = (void *)STM32F4XX_CAN1_BASE;
                                                      /* RCC_APB1ENR: enable CAN 1 clock also     */
        *(volatile CPU_INT32U *)0x40023840 |= 0x02000000;
        dev->Base = (void *)STM32F4XX_CAN2_BASE;      /* set CAN 2 base address                   */
                                                      /* RCC_APB1ENR: enable CAN 2 clock          */
        *(volatile CPU_INT32U *)0x40023840 |= 0x04000000;
    }

    can = (STM32F4XX_CAN_t *)dev->Base;
                                                      /*------------------------------------------*/
    can->MCR &= ~STM32F4XX_CAN_MCR_SLEEP;             /* exit sleep mode                          */
                                                      /*------------------------------------------*/
    can->MCR = STM32F4XX_CAN_MCR_INRQ;                /* set init bit                             */
    while ((can->MSR & STM32F4XX_CAN_MSR_INAK) == 0) {/* wait for init mode                       */
    }
                                                      /*------------------------------------------*/
    dev->Baudrate        = STM32F4XX_DEF_BAUDRATE;    /* Initialize parameter struct of the       */
    dev->SamplePoint     = STM32F4XX_DEF_SP;          /* current device                           */
    dev->ResynchJumpWith = STM32F4XX_DEF_RJW;
                                                      /*------------------------------------------*/
    STM32F4XXCAN_CalcTimingReg(dev);                  /* Calculate the values for the timing      */
                                                      /* register with the parameter setting      */
                                                      /*------------------------------------------*/
    can->BTR = (dev->RJW   << 24) |                   /* Set the bit timing register              */
               (dev->PSEG1 << 16) |
               (dev->PSEG2 << 20) |
               (dev->PRESDIV);
                                                      /*------------------------------------------*/
                                                      /* Only can 1 has the filter regs.          */
    can0 = (STM32F4XX_CAN_t *)DevData[STM32F4XX_CAN_BUS_0].Base;

    can0->FMR   |=  STM32F4XX_CAN_FMR_FINIT;          /* set filter initialization to init        */
    can0->FMR   &= ~0x00003F00;                       /* clear can 2 start bank                   */
    can0->FMR   |=  (14 << 8);                        /* set can 2 start bank to 14               */
    can0->FM1R   =  0;                                /* All filter modes mask                    */
    can0->FS1R   =  0x0FFFFFFF;                       /* All filter scale single                  */
    can0->FFA1R  =  0x0AAAAAAA;                       /* filters alternating assigned to fifo 0&1 */
    can0->FA1R   =  0x00000003 |                      /* can 1: filter  0&1 active                */
                    0x0000C000;                       /* can 2: filter 14&15 active               */

    dev->FilterIdx = 2;                               /* 0&1 are already in use                   */

    if (arg == STM32F4XX_CAN_BUS_0) {
        for (i = 0; i < 14; i++){
            can0->FilterRegister[i].FR1 = 0;          /* reset identifier/mask                    */
            can0->FilterRegister[i].FR2 = 0;
        }
    } else {
        for (i = 14; i < 28; i++){
            can0->FilterRegister[i].FR1 = 0;          /* reset identifier/mask                    */
            can0->FilterRegister[i].FR2 = 0;
        }
    }
    can0->FMR   &= ~STM32F4XX_CAN_FMR_FINIT;          /* set filter initialization to active      */
                                                      /*------------------------------------------*/
    can->MCR &= ~STM32F4XX_CAN_MCR_INRQ;              /* Request leave initialization             */
    while ((can->MSR & STM32F4XX_CAN_MSR_INAK) == 1) {/* wait for end of init mode                */
    }
                                                      /*------------------------------------------*/
    STM32F4XXCAN_EnableIrqs (arg);                    /* Enable interrupts                        */

    result = STM32F4XX_CAN_NO_ERR;
                                                      /*------------------------------------------*/
    return (result);                                  /* Return function result                   */
}

/*------------------------------------------------------------------------------------------------*/
/*!
* \brief    OPEN CAN DEVICE
*
* \ingroup  STM32F4XX_CAN
*
*           Unlocks the device, i.e. IoCtl/Read/Write-function will take effect.
*
* \param    devId    the bus node name which must be used by the interrupt routine to access the
*                    CAN bus layer.
* \param    devName  the CAN device name
* \param    mode     the mode in which the CAN device will be used
*
* \return   the parameter identifier for further access or -1 if an error occurred
*/
/*------------------------------------------------------------------------------------------------*/
CPU_INT16S STM32F4XXCANOpen (CPU_INT16S devId, CPU_INT32U devName, CPU_INT16U mode)
{
    STM32F4XX_CAN_DATA *dev;                          /* Local: Pointer to can device             */
    CPU_INT16S          result = -1;                  /* Local: Function result                   */
    CPU_SR_ALLOC();                                   /* Allocate storage for CPU status reg.     */
                                                      /*------------------------------------------*/
#if STM32F4XX_CAN_ARG_CHK_CFG > 0
    if (devName >= STM32F4XX_CAN_N_DEV) {             /* check that device name is in range       */
        DrvError = STM32F4XX_CAN_BUS_ERR;
        return (result);                              /* return function result                   */
    }
    if (mode != DEV_RW) {                             /* mode not supported?                      */
        DrvError = STM32F4XX_CAN_MODE_ERR;
        return (result);
    }
#endif
    dev = &DevData[devName];                          /* set pointer to can device                */

    CPU_CRITICAL_ENTER();
    if (dev->Use == 0) {                              /* check, that can device is unused         */
        dev->Use = 1;                                 /* mark can device as used                  */
#if ((STM32F4XX_CAN1_RX_INTERRUPT_EN > 0) || \
     (STM32F4XX_CAN1_TX_INTERRUPT_EN > 0) || \
     (STM32F4XX_CAN1_NS_INTERRUPT_EN > 0) || \
     (STM32F4XX_CAN2_RX_INTERRUPT_EN > 0) || \
     (STM32F4XX_CAN2_TX_INTERRUPT_EN > 0) || \
     (STM32F4XX_CAN2_NS_INTERRUPT_EN > 0))
        STM32F4XXCAN_SetDevIds(devId, devName);       /* store the received Node Id for the irqs  */
#else
        devId    = devId;                             /* prevent compiler warning                 */
#endif
        result   = (CPU_INT16S)devName;               /* Okay, device is opened                   */
    } else {
        DrvError = STM32F4XX_CAN_OPEN_ERR;
    }
    CPU_CRITICAL_EXIT();
                                                      /*------------------------------------------*/
    return (result);                                  /* return function result                   */
}

/*------------------------------------------------------------------------------------------------*/
/*!
* \brief    CLOSE CAN DEVICE
*
* \ingroup  STM32F4XX_CAN
*
*           Locks the device, i.e. IoCtl/Read/Write-function will have no effect.
*
* \param    paraId  the parameter identifier, returned by XXXCANOpen()
*
* \return   error code (0 if OK, -1 if an error occurred)
*/
/*------------------------------------------------------------------------------------------------*/
CPU_INT16S STM32F4XXCANClose (CPU_INT16S paraId)
{
    STM32F4XX_CAN_DATA *dev;                          /* Local: Pointer to can device             */
    CPU_INT16S          result = -1;                  /* Local: Function result                   */
    CPU_SR_ALLOC();                                   /* Allocate storage for CPU status reg.     */
                                                      /*------------------------------------------*/
#if STM32F4XX_CAN_ARG_CHK_CFG > 0
    if ((paraId >= STM32F4XX_CAN_N_DEV) || (paraId < 0)) { /* check that paraId is in range       */
        DrvError = STM32F4XX_CAN_BUS_ERR;
        return (result);                              /* return function result                   */
    }
#endif
    dev = &DevData[paraId];                           /* Set pointer to can device                */

    CPU_CRITICAL_ENTER();
    if (dev->Use != 0) {                              /* check, that can device is used           */
        dev->Use = 0;                                 /* mark can device as unused                */
        result   = STM32F4XX_CAN_NO_ERR;              /* Indicate successful function execution   */
    } else {
        DrvError = STM32F4XX_CAN_CLOSE_ERR;
    }
    CPU_CRITICAL_EXIT();
                                                      /*------------------------------------------*/
    return (result);                                  /* return function result                   */
}

/*------------------------------------------------------------------------------------------------*/
/*!
* \brief    CAN I/O CONTROL
*
* \ingroup  STM32F4XX_CAN
*
*           This function performs a special action on the opened device. The function code func
*           defines what the caller wants to do. Description of function codes as defined in
*           header file.
*
* \param    paraId  parameter identifier, returned by XXXCANOpen()
* \param    func    function code
* \param    argp    argument list, specific to the function code
*
* \return   error code (0 if OK, -1 if an error occurred)
*/
/*------------------------------------------------------------------------------------------------*/
CPU_INT16S STM32F4XXCANIoCtl (CPU_INT16S paraId, CPU_INT16U func, void *argp)
{
    STM32F4XX_CAN_DATA *dev;                          /* Local: Pointer to can device             */
    STM32F4XX_CAN_t    *can;                          /* Local: Pointer to can register           */
    STM32F4XX_CAN_t    *can0;                         /* Local: Pointer to can register           */
    CPU_INT32U          status_reg;                   /* Local: register value                    */
    CPU_INT16S          TransmitMailbox = -1;         /* Local: Tx mailbox Id                     */
    CPU_INT32U          i;                            /* Local: loop variable                     */
    CPU_INT32U          mask;                         /* Local: mask for filter                   */
    CPU_INT32U          canId;                        /* Local: id for filter                     */
    CPU_INT16S          result          = -1;         /* Local: Function result                   */
    CPU_SR_ALLOC();                                   /* Allocate storage for CPU status reg.     */
                                                      /*------------------------------------------*/
#if STM32F4XX_CAN_ARG_CHK_CFG > 0
    if ((paraId >= STM32F4XX_CAN_N_DEV) || (paraId < 0)) { /* check that paraId is in range       */
        DrvError = STM32F4XX_CAN_BUS_ERR;
        return (result);                              /* return function result                   */
    }
#endif
    dev = &DevData[paraId];                           /* set pointer to can device                */
#if STM32F4XX_CAN_ARG_CHK_CFG > 0
    if (dev->Use != 1) {                              /* check, that can device is opened         */
        DrvError = STM32F4XX_CAN_OPEN_ERR;
        return (result);                              /* return function result                   */
    }
#endif
    can  = (STM32F4XX_CAN_t *)dev->Base;
                                                      /* Only can 0 has filter regs.              */
    can0 = (STM32F4XX_CAN_t *)DevData[STM32F4XX_CAN_BUS_0].Base;

    CPU_CRITICAL_ENTER();
                                                      /*------------------------------------------*/
    switch (func) {                                   /* select: function code                    */
                                                      /*------------------------------------------*/
        case IO_STM32F4XX_CAN_GET_IDENT:              /* GET IDENT                                */
            (*(CPU_INT32U *)argp) = DrvIdent;         /* return driver ident code                 */
            result = STM32F4XX_CAN_NO_ERR;            /* Indicate successful function execution   */
            break;                                    /*------------------------------------------*/
        case IO_STM32F4XX_CAN_GET_ERRNO:              /* GET ERRORCODE                            */
            (*(CPU_INT16U *)argp) = DrvError;         /* return last detected errorcode           */
            result = STM32F4XX_CAN_NO_ERR;            /* Indicate successful function execution   */
            break;                                    /*------------------------------------------*/
        case IO_STM32F4XX_CAN_GET_DRVNAME:            /* GET DRIVER NAME                          */
            (*(CPU_INT08U **)argp) = (CPU_INT08U *)   /* return human readable driver name        */
                                     STM32F4XX_CAN_NAME;
            result = STM32F4XX_CAN_NO_ERR;            /* Indicate successful function execution   */
            break;                                    /*------------------------------------------*/
        case IO_STM32F4XX_CAN_SET_BAUDRATE:           /* SET BAUDRATE                             */
            dev->Baudrate = *(CPU_INT32U *)argp;      /* Set baudrate parameter to given argument */
                                                      /* and calculate the values for the timing  */
                                                      /* register with the parameter setting      */
            if (STM32F4XXCAN_CalcTimingReg(dev) != STM32F4XX_CAN_NO_ERR) {
                DrvError = STM32F4XX_CAN_ARG_ERR;     /* indicate error on function result.       */
            } else {                                  /* set can timing registers if timing       */
                                                      /* calculation success.                     */
                can->MCR |=  STM32F4XX_CAN_MCR_INRQ;  /* set init bit                             */
                while ((can->MSR & STM32F4XX_CAN_MSR_INAK) == 0) { /* wait for init mode          */
                }
                can->BTR  =  (dev->RJW   << 24) |     /* Set the bit timing register              */
                             (dev->PSEG1 << 16) |
                             (dev->PSEG2 << 20) |
                             (dev->PRESDIV);
                can->MCR &= ~STM32F4XX_CAN_MCR_INRQ;  /* Request leave initialization             */
                while ((can->MSR & STM32F4XX_CAN_MSR_INAK) == 1) { /* wait for end of init mode   */
                }
                result = STM32F4XX_CAN_NO_ERR;        /* Indicate successful function execution   */
            }
            break;                                    /*------------------------------------------*/
        case IO_STM32F4XX_CAN_TX_READY:               /* TX READY?                                */
                                                      /* check for empty transmit mailbox         */
            if ((can->TSR & STM32F4XX_CAN_TSR_TME0) == STM32F4XX_CAN_TSR_TME0) {
                TransmitMailbox = 0;
            } else if ((can->TSR & STM32F4XX_CAN_TSR_TME1) == STM32F4XX_CAN_TSR_TME1) {
                TransmitMailbox = 1;
            } else if ((can->TSR & STM32F4XX_CAN_TSR_TME2) == STM32F4XX_CAN_TSR_TME2) {
                TransmitMailbox = 2;
            }

            if (TransmitMailbox < 0) {
                *((CPU_INT08U *)argp) = 0;            /* Transmit Channel is not available        */
            } else {
                *((CPU_INT08U *)argp) = 1;            /* Transmit Channel is available            */
            }
            result = STM32F4XX_CAN_NO_ERR;            /* Indicate successful function execution   */
            break;                                    /*------------------------------------------*/
        case IO_STM32F4XX_CAN_START:                  /* START CAN                                */
            can->MCR &= ~STM32F4XX_CAN_MCR_INRQ;      /* reset init bit                           */
            while ((can->MSR & STM32F4XX_CAN_MSR_INAK) == 1) { /* wait for end of init mode       */
            }
            result = STM32F4XX_CAN_NO_ERR;            /* Indicate successful function execution   */
            break;                                    /*------------------------------------------*/
        case IO_STM32F4XX_CAN_STOP:                   /* STOP CAN                                 */
            can->MCR |=  STM32F4XX_CAN_MCR_INRQ;      /* set init bit                             */
            while ((can->MSR & STM32F4XX_CAN_MSR_INAK) == 0) { /* wait for init mode              */
            }
            can->TSR = 0x00808080;                    /* abort pending transmissions              */
            result = STM32F4XX_CAN_NO_ERR;            /* Indicate successful function execution   */
            break;                                    /*------------------------------------------*/
        case IO_STM32F4XX_CAN_RX_STANDARD:            /* RX STANDARD                              */
            can0->FMR |=  STM32F4XX_CAN_FMR_FINIT;    /* set filter initialization to init        */
            if (paraId == STM32F4XX_CAN_BUS_0) {
                for (i = 0; i < 14; i++){
                    can0->FilterRegister[i].FR1 &= ~0x4; /* reset ide bit                         */
                    can0->FilterRegister[i].FR2 |=  0x4;
                }
            } else {
                for (i = 14; i < 28; i++){
                    can0->FilterRegister[i].FR1 &= ~0x4; /* reset ide bit                         */
                    can0->FilterRegister[i].FR2 |=  0x4;
                }
            }
            can0->FMR &= ~STM32F4XX_CAN_FMR_FINIT;    /* set filter initialization to active      */
            result = STM32F4XX_CAN_NO_ERR;            /* Indicate successful function execution   */
            break;                                    /*------------------------------------------*/
        case IO_STM32F4XX_CAN_RX_EXTENDED:            /* RX EXTENDED                              */
            can0->FMR |=  STM32F4XX_CAN_FMR_FINIT;    /* set filter initialization to init        */
            if (paraId == STM32F4XX_CAN_BUS_0) {
                for (i = 0; i < 14; i++){
                    can0->FilterRegister[i].FR1 |= 0x4; /* set ide bit                            */
                    can0->FilterRegister[i].FR2 |= 0x4;
                }
            } else {
                for (i = 14; i < 28; i++){
                    can0->FilterRegister[i].FR1 |= 0x4; /* set ide bit                            */
                    can0->FilterRegister[i].FR2 |= 0x4;
                }
            }
            can0->FMR &= ~STM32F4XX_CAN_FMR_FINIT;    /* set filter initialization to active      */
            result = STM32F4XX_CAN_NO_ERR;            /* Indicate successful function execution   */
            break;                                    /*------------------------------------------*/
        case IO_STM32F4XX_CAN_GET_NODE_STATUS:        /* GET NODE STATUS                          */
            status_reg = can->ESR;                    /* get error status register                */
            if ((status_reg & 0x03) != 0) {           /* Bit 1/2 Error Warning/Passive Bit        */
                *((CPU_INT08U *)argp) = 1;
            }
            if ((status_reg & 0x04) != 0) {           /* Bit 3 Bus Off Bit                        */
                *((CPU_INT08U *)argp) = 2;
            }
            if ((status_reg & 0x07) == 0) {           /* Bit 1-3 not set - bus active             */
                *((CPU_INT08U *)argp) = 0;
            }
            result = STM32F4XX_CAN_NO_ERR;            /* Indicate successful function execution   */
            break;                                    /*------------------------------------------*/
        case IO_STM32F4XX_CAN_SET_RX_FILTER_1:        /* SET RX FILTER 1                          */
            mask  = ((CPU_INT32U*)argp)[0];
            canId = ((CPU_INT32U*)argp)[1];
            can0->FMR |=  STM32F4XX_CAN_FMR_FINIT;    /* set filter initialization to init        */
            if (canId > 0x7FF) {                      /* depending on the CAN ID                  */
                                                      /* use bit 0..28                            */
                                                      /* set ext canId + ide bit and mask         */
                if (paraId == STM32F4XX_CAN_BUS_0) {
                    can0->FilterRegister[0].FR1  = (canId << 3) + 4;
                    can0->FilterRegister[0].FR2  = (mask  << 3) + 4;
                    can0->FilterRegister[1].FR1  = (canId << 3) + 4;
                    can0->FilterRegister[1].FR2  = (mask  << 3) + 4;
                } else {
                    can0->FilterRegister[14].FR1 = (canId << 3) + 4;
                    can0->FilterRegister[14].FR2 = (mask  << 3) + 4;
                    can0->FilterRegister[15].FR1 = (canId << 3) + 4;
                    can0->FilterRegister[15].FR2 = (mask  << 3) + 4;
                }
            } else {
                                                      /* use bit 0..10                            */
                                                      /* set std canId and mask                   */
                if (paraId == STM32F4XX_CAN_BUS_0) {
                    can0->FilterRegister[0].FR1  = (canId << 21);
                    can0->FilterRegister[0].FR2  = (mask  << 21);
                    can0->FilterRegister[1].FR1  = (canId << 21);
                    can0->FilterRegister[1].FR2  = (mask  << 21);
                } else {
                    can0->FilterRegister[14].FR1 = (canId << 21);
                    can0->FilterRegister[14].FR2 = (mask  << 21);
                    can0->FilterRegister[15].FR1 = (canId << 21);
                    can0->FilterRegister[15].FR2 = (mask  << 21);
                }
            }
            can0->FMR &= ~STM32F4XX_CAN_FMR_FINIT;    /* set filter initialization to active      */
            result = STM32F4XX_CAN_NO_ERR;            /* Indicate successful function execution   */
            break;                                    /*------------------------------------------*/
        case IO_STM32F4XX_CAN_SET_RX_FILTER_2:        /* SET RX FILTER 2                          */
            mask  = ((CPU_INT32U*)argp)[0];
            canId = ((CPU_INT32U*)argp)[1];
            can0->FMR |=  STM32F4XX_CAN_FMR_FINIT;    /* set filter initialization to init        */
            if (canId > 0x7FF) {                      /* depending on the CAN ID                  */
                                                      /* use bit 0..28                            */
                                                      /* set ext canId + ide bit and mask         */
                if (paraId == STM32F4XX_CAN_BUS_0) {
                    can0->FilterRegister[dev->FilterIdx].FR1  = (canId << 3) + 4;
                    can0->FilterRegister[dev->FilterIdx].FR2  = (mask  << 3) + 4;
                    can0->FilterRegister[dev->FilterIdx+1].FR1  = (canId << 3) + 4;
                    can0->FilterRegister[dev->FilterIdx+1].FR2  = (mask  << 3) + 4;
                } else {
                    can0->FilterRegister[dev->FilterIdx+14].FR1 = (canId << 3) + 4;
                    can0->FilterRegister[dev->FilterIdx+14].FR2 = (mask  << 3) + 4;
                    can0->FilterRegister[dev->FilterIdx+15].FR1 = (canId << 3) + 4;
                    can0->FilterRegister[dev->FilterIdx+15].FR2 = (mask  << 3) + 4;
                }
            } else {
                                                      /* use bit 0..10                            */
                                                      /* set std canId + ide bit and mask         */
                if (paraId == STM32F4XX_CAN_BUS_0) {
                    can0->FilterRegister[dev->FilterIdx].FR1  = (canId << 21);
                    can0->FilterRegister[dev->FilterIdx].FR2  = (mask  << 21);
                    can0->FilterRegister[dev->FilterIdx+1].FR1  = (canId << 21);
                    can0->FilterRegister[dev->FilterIdx+1].FR2  = (mask  << 21);
                } else {
                    can0->FilterRegister[dev->FilterIdx+14].FR1 = (canId << 21);
                    can0->FilterRegister[dev->FilterIdx+14].FR2 = (mask  << 21);
                    can0->FilterRegister[dev->FilterIdx+15].FR1 = (canId << 21);
                    can0->FilterRegister[dev->FilterIdx+15].FR2 = (mask  << 21);
                }
            }
            if (dev->FilterIdx < 12) {
                dev->FilterIdx += 2;                  /* increment to next pair ..                */
            } else {
                dev->FilterIdx = 0;                   /* .. or wrap around                        */
            }
            can0->FMR &= ~STM32F4XX_CAN_FMR_FINIT;    /* set filter initialization to active      */
            result = STM32F4XX_CAN_NO_ERR;            /* Indicate successful function execution   */
            break;                                    /*------------------------------------------*/
        default:
            break;
    }
    CPU_CRITICAL_EXIT();

    if (result == -1) {
        DrvError = STM32F4XX_CAN_FUNC_ERR;
    }
                                                       /*------------------------------------------*/
    return (result);                                   /* Return function result                   */
}

/*------------------------------------------------------------------------------------------------*/
/*!
* \brief    CAN READ DATA
*
* \ingroup  STM32F4XX_CAN
*
*           Read a received can frame from a message buffer. The buffer must have space for only
*           one can frame.
*
* \param    paraId  parameter identifier, returned by XXXCANOpen()
* \param    buffer  pointer to CAN frame
* \param    size    length of CAN frame memory
*
* \return   error code (CAN frame DLC if OK, -1 if an error occurred)
*/
/*------------------------------------------------------------------------------------------------*/
CPU_INT16S STM32F4XXCANRead (CPU_INT16S paraId, CPU_INT08U *buffer, CPU_INT16U size)
{
    STM32F4XX_CAN_DATA *dev;                          /* Local: Pointer to can device             */
    STM32F4XX_CAN_t    *can;                          /* Local: Pointer to can register           */
    CPU_INT16S          result = -1;                  /* Local: return value                      */
    STM32F4XX_CANFRM   *frm;                          /* Local: Pointer to can frame              */
    CPU_INT32U          Word;                         /* Local: A word for extended IO access     */
    CPU_INT32S          FIFONumber;                   /* Local: receive fifo                      */
    CPU_SR_ALLOC();                                   /* Allocate storage for CPU status reg.     */
                                                      /*------------------------------------------*/
#if STM32F4XX_CAN_ARG_CHK_CFG > 0
    if ((paraId >= STM32F4XX_CAN_N_DEV) || (paraId < 0)) { /* check that paraId is in range       */
        DrvError = STM32F4XX_CAN_BUS_ERR;
        return (result);
    }
    if (size != sizeof(STM32F4XX_CANFRM)) {           /* check that size is plausible             */
        DrvError = STM32F4XX_CAN_NO_DATA_ERR;
        return (result);
    }
    if (buffer == (void *)0) {                        /* invalid buffer pointer                   */
        DrvError = STM32F4XX_CAN_ARG_ERR;
        return (result);
    }
#endif
    dev = &DevData[paraId];                           /* set pointer to can device                */
#if STM32F4XX_CAN_ARG_CHK_CFG > 0
    if (dev->Use != 1) {                              /* check, that can device is opened         */
        DrvError = STM32F4XX_CAN_OPEN_ERR;
        return (result);
    }
#endif
    can = (STM32F4XX_CAN_t *)dev->Base;

    frm = (STM32F4XX_CANFRM *)buffer;                 /* Set pointer to can frame                 */
    FIFONumber = -1;

    CPU_CRITICAL_ENTER();
    if ((can->RF0R & 0x03) >= 1) {
        FIFONumber = 0;
    } else if ((can->RF1R & 0x03) >= 1) {             /* data is available                        */
        FIFONumber = 1;
    }
    if (FIFONumber >= 0) {
                                                      /* Get the Id                               */
        Word = can->FIFOMailBox[FIFONumber].RIR & 0x04; /* get IDE bit                            */
        if (Word == 0) {                              /* is std id                                */
            frm->Identifier  = can->FIFOMailBox[FIFONumber].RIR >> 21;
        } else {
            frm->Identifier  = can->FIFOMailBox[FIFONumber].RIR >> 3;
            frm->Identifier |= STM32F4XX_FF_FRAME_BIT;/* mark ext. ID                             */
        }
        if ((can->FIFOMailBox[FIFONumber].RIR & 0x02) != 0) { /* check remote frame               */
            frm->Identifier |= STM32F4XX_RTR_FRAME_BIT;/* mark remote frame                       */
        }
                                                      /* Get the DLC                              */
        frm->DLC = can->FIFOMailBox[FIFONumber].RDTR & 0x0F;
                                                      /* Get the data field                       */
        frm->Data[0] = (CPU_INT08U)(can->FIFOMailBox[FIFONumber].RDLR      );
        frm->Data[1] = (CPU_INT08U)(can->FIFOMailBox[FIFONumber].RDLR >>  8);
        frm->Data[2] = (CPU_INT08U)(can->FIFOMailBox[FIFONumber].RDLR >> 16);
        frm->Data[3] = (CPU_INT08U)(can->FIFOMailBox[FIFONumber].RDLR >> 24);

        frm->Data[4] = (CPU_INT08U)(can->FIFOMailBox[FIFONumber].RDHR      );
        frm->Data[5] = (CPU_INT08U)(can->FIFOMailBox[FIFONumber].RDHR >>  8);
        frm->Data[6] = (CPU_INT08U)(can->FIFOMailBox[FIFONumber].RDHR >> 16);
        frm->Data[7] = (CPU_INT08U)(can->FIFOMailBox[FIFONumber].RDHR >> 24);

        result   = size;
    } else {
        DrvError = STM32F4XX_CAN_NO_DATA_ERR;
    }
    CPU_CRITICAL_EXIT();
                                                      /*------------------------------------------*/
    return (result);                                  /* Return function result                   */
}

/*------------------------------------------------------------------------------------------------*/
/*!
* \brief    CAN WRITE DATA
*
* \ingroup  STM32F4XX_CAN
*
*           Write a can frame to a message buffer. The buffer must contain only one can frame,
*           which will be written to a predefined message buffer.
*
* \param    paraId  parameter identifier, returned by XXXCANOpen()
* \param    buffer  pointer to CAN frame
* \param    size    length of CAN frame memory
*
* \return   error code (CAN frame DLC if OK, -1 if an error occurred)
*/
/*------------------------------------------------------------------------------------------------*/
CPU_INT16S STM32F4XXCANWrite (CPU_INT16S paraId, CPU_INT08U *buffer, CPU_INT16U size)
{
    STM32F4XX_CAN_DATA *dev;                          /* Local: Pointer to can device             */
    STM32F4XX_CAN_t    *can;                          /* Local: Pointer to can register           */
    CPU_INT16S          result          = -1;         /* Local: return value                      */
    STM32F4XX_CANFRM   *frm;                          /* Local: Pointer to can frame              */
    CPU_INT32U          Word;                         /* Local: A word for extended IO access     */
    CPU_INT16S          TransmitMailbox = -1;         /* Local: transmit mailbox number           */
    CPU_SR_ALLOC();                                   /* Allocate storage for CPU status reg.     */
                                                      /*------------------------------------------*/
#if STM32F4XX_CAN_ARG_CHK_CFG > 0
    if ((paraId >= STM32F4XX_CAN_N_DEV) || (paraId < 0)) { /* check that paraId is in range       */
        DrvError = STM32F4XX_CAN_BUS_ERR;
        return (result);
    }
    if (size != sizeof(STM32F4XX_CANFRM)) {           /* check that size is plausible             */
        DrvError = STM32F4XX_CAN_NO_DATA_ERR;
        return (result);
    }
    if (buffer == (void *)0) {                        /* invalid buffer pointer                   */
        DrvError = STM32F4XX_CAN_ARG_ERR;
        return (result);
    }
#endif
    dev = &DevData[paraId];                           /* set pointer to can device                */
#if STM32F4XX_CAN_ARG_CHK_CFG > 0
    if (dev->Use != 1) {                              /* check, that can device is opened         */
        DrvError = STM32F4XX_CAN_OPEN_ERR;
        return (result);
    }
#endif
    can = (STM32F4XX_CAN_t *)dev->Base;

    CPU_CRITICAL_ENTER();
    frm = (STM32F4XX_CANFRM *)buffer;                 /* Set pointer to can frame                 */

    if (frm->Identifier > 0x7FF) {
        Word = 0x80000000L;                           /* identifier extended format               */
    } else {
        Word = 0x00000000L;                           /* identifier standard format               */
    }
    Word |= (frm->DLC << 16);
                                                      /* Select one empty transmit mailbox        */
    if ((can->TSR & STM32F4XX_CAN_TSR_TME0) == STM32F4XX_CAN_TSR_TME0) {
        TransmitMailbox = 0;
    } else if ((can->TSR & STM32F4XX_CAN_TSR_TME1) == STM32F4XX_CAN_TSR_TME1) {
        TransmitMailbox = 1;
    } else if ((can->TSR & STM32F4XX_CAN_TSR_TME2) == STM32F4XX_CAN_TSR_TME2) {
        TransmitMailbox = 2;
    }

    if (TransmitMailbox >= 0) {
                                                      /* Set up the Id                            */
        can->TxMailBox[TransmitMailbox].TIR &= STM32F4XX_CAN_TIxR_TXRQ;
        if (frm->Identifier <= 0x7FF) {
            Word = frm->Identifier << 21;

            can->TxMailBox[TransmitMailbox].TIR |= Word;
        } else {
            Word = frm->Identifier & 0x1FFFFFFF;
            Word = Word << 3;

            can->TxMailBox[TransmitMailbox].TIR |= (Word | 0x04 );
        }
                                                      /* Set up the DLC                           */
        can->TxMailBox[TransmitMailbox].TDTR &= 0xFFFFFFF0;
        can->TxMailBox[TransmitMailbox].TDTR |= (CPU_INT32U)frm->DLC;
                                                      /* Set up the data field                    */
        can->TxMailBox[TransmitMailbox].TDLR = (((CPU_INT32U)frm->Data[3] << 24) |
                                                ((CPU_INT32U)frm->Data[2] << 16) |
                                                ((CPU_INT32U)frm->Data[1] <<  8) |
                                                ((CPU_INT32U)frm->Data[0]));
        can->TxMailBox[TransmitMailbox].TDHR = (((CPU_INT32U)frm->Data[7] << 24) |
                                                ((CPU_INT32U)frm->Data[6] << 16) |
                                                ((CPU_INT32U)frm->Data[5] <<  8) |
                                                ((CPU_INT32U)frm->Data[4]));
                                                      /* Request transmission                     */
        can->TxMailBox[TransmitMailbox].TIR |= STM32F4XX_CAN_TIxR_TXRQ;

        result   = size;
    } else {
        DrvError = STM32F4XX_CAN_BUSY_ERR;
    }
    CPU_CRITICAL_EXIT();
                                                      /*------------------------------------------*/
    return (result);                                  /* Return function result                   */
}
