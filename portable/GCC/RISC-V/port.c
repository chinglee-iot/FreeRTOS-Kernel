/*
 * FreeRTOS Kernel <DEVELOPMENT BRANCH>
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

/*-----------------------------------------------------------
 * Implementation of functions defined in portable.h for the RISC-V port.
 *----------------------------------------------------------*/

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "portmacro.h"

#include "pmp_apis.h"

/* Standard includes. */
#include "string.h"

#ifdef configCLINT_BASE_ADDRESS
    #warning The configCLINT_BASE_ADDRESS constant has been deprecated.  configMTIME_BASE_ADDRESS and configMTIMECMP_BASE_ADDRESS are currently being derived from the (possibly 0) configCLINT_BASE_ADDRESS setting.  Please update to define configMTIME_BASE_ADDRESS and configMTIMECMP_BASE_ADDRESS dirctly in place of configCLINT_BASE_ADDRESS.  See https://www.FreeRTOS.org/Using-FreeRTOS-on-RISC-V.html
#endif

#ifndef configMTIME_BASE_ADDRESS
    #warning configMTIME_BASE_ADDRESS must be defined in FreeRTOSConfig.h.  If the target chip includes a memory-mapped mtime register then set configMTIME_BASE_ADDRESS to the mapped address.  Otherwise set configMTIME_BASE_ADDRESS to 0.  See https://www.FreeRTOS.org/Using-FreeRTOS-on-RISC-V.html
#endif

#ifndef configMTIMECMP_BASE_ADDRESS
    #warning configMTIMECMP_BASE_ADDRESS must be defined in FreeRTOSConfig.h.  If the target chip includes a memory-mapped mtimecmp register then set configMTIMECMP_BASE_ADDRESS to the mapped address.  Otherwise set configMTIMECMP_BASE_ADDRESS to 0.  See https://www.FreeRTOS.org/Using-FreeRTOS-on-RISC-V.html
#endif

/* Let the user override the pre-loading of the initial RA. */
#ifdef configTASK_RETURN_ADDRESS
    #define portTASK_RETURN_ADDRESS    configTASK_RETURN_ADDRESS
#else
    #define portTASK_RETURN_ADDRESS    0
#endif

/* The stack used by interrupt service routines.  Set configISR_STACK_SIZE_WORDS
 * to use a statically allocated array as the interrupt stack.  Alternative leave
 * configISR_STACK_SIZE_WORDS undefined and update the linker script so that a
 * linker variable names __freertos_irq_stack_top has the same value as the top
 * of the stack used by main.  Using the linker script method will repurpose the
 * stack that was used by main before the scheduler was started for use as the
 * interrupt stack after the scheduler has started. */
#ifdef configISR_STACK_SIZE_WORDS
    static __attribute__ ((aligned(16))) StackType_t xISRStack[ configISR_STACK_SIZE_WORDS ] = { 0 };
    const StackType_t xISRStackTop = ( StackType_t ) &( xISRStack[ configISR_STACK_SIZE_WORDS & ~portBYTE_ALIGNMENT_MASK ] );

    /* Don't use 0xa5 as the stack fill bytes as that is used by the kernerl for
    the task stacks, and so will legitimately appear in many positions within
    the ISR stack. */
    #define portISR_STACK_FILL_BYTE    0xee
#else
    extern const uint32_t __freertos_irq_stack_top[];
    const StackType_t xISRStackTop = ( StackType_t ) __freertos_irq_stack_top;
#endif

/*
 * Setup the timer to generate the tick interrupts.  The implementation in this
 * file is weak to allow application writers to change the timer used to
 * generate the tick interrupt.
 */
void vPortSetupTimerInterrupt( void ) __attribute__(( weak ));


#if ( configENABLE_MPU == 1 )

/**
 * @brief Setup the Memory Protection Unit (MPU).
 */
    static void prvSetupMPU( void ) PRIVILEGED_FUNCTION;
#endif /* configENABLE_MPU */
/*-----------------------------------------------------------*/

/* Used to program the machine timer compare register. */
uint64_t ullNextTime = 0ULL;
const uint64_t *pullNextTime = &ullNextTime;
const size_t uxTimerIncrementsForOneTick = ( size_t ) ( ( configCPU_CLOCK_HZ ) / ( configTICK_RATE_HZ ) ); /* Assumes increment won't go over 32-bits. */
uint32_t const ullMachineTimerCompareRegisterBase = configMTIMECMP_BASE_ADDRESS;
volatile uint64_t * pullMachineTimerCompareRegister = NULL;

/* Holds the critical nesting value - deliberately non-zero at start up to
 * ensure interrupts are not accidentally enabled before the scheduler starts. */
size_t xCriticalNesting = ( size_t ) 0xaaaaaaaa;
size_t *pxCriticalNesting = &xCriticalNesting;

/* Used to catch tasks that attempt to return from their implementing function. */
size_t xTaskReturnAddress = ( size_t ) portTASK_RETURN_ADDRESS;

/* Set configCHECK_FOR_STACK_OVERFLOW to 3 to add ISR stack checking to task
 * stack checking.  A problem in the ISR stack will trigger an assert, not call
 * the stack overflow hook function (because the stack overflow hook is specific
 * to a task stack, not the ISR stack). */
#if defined( configISR_STACK_SIZE_WORDS ) && ( configCHECK_FOR_STACK_OVERFLOW > 2 )
    #warning This path not tested, or even compiled yet.

    static const uint8_t ucExpectedStackBytes[] = {
                                    portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE,        \
                                    portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE,        \
                                    portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE,        \
                                    portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE,        \
                                    portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE };    \

    #define portCHECK_ISR_STACK() configASSERT( ( memcmp( ( void * ) xISRStack, ( void * ) ucExpectedStackBytes, sizeof( ucExpectedStackBytes ) ) == 0 ) )
#else
    /* Define the function away. */
    #define portCHECK_ISR_STACK()
#endif /* configCHECK_FOR_STACK_OVERFLOW > 2 */

/*-----------------------------------------------------------*/

#if( configMTIME_BASE_ADDRESS != 0 ) && ( configMTIMECMP_BASE_ADDRESS != 0 )

    void vPortSetupTimerInterrupt( void )
    {
    uint32_t ulCurrentTimeHigh, ulCurrentTimeLow;
    volatile uint32_t * const pulTimeHigh = ( volatile uint32_t * const ) ( ( configMTIME_BASE_ADDRESS ) + 4UL ); /* 8-byte type so high 32-bit word is 4 bytes up. */
    volatile uint32_t * const pulTimeLow = ( volatile uint32_t * const ) ( configMTIME_BASE_ADDRESS );
    volatile uint32_t ulHartId;

        __asm volatile( "csrr %0, mhartid" : "=r"( ulHartId ) );
        pullMachineTimerCompareRegister  = ( volatile uint64_t * ) ( ullMachineTimerCompareRegisterBase + ( ulHartId * sizeof( uint64_t ) ) );

        do
        {
            ulCurrentTimeHigh = *pulTimeHigh;
            ulCurrentTimeLow = *pulTimeLow;
        } while( ulCurrentTimeHigh != *pulTimeHigh );

        ullNextTime = ( uint64_t ) ulCurrentTimeHigh;
        ullNextTime <<= 32ULL; /* High 4-byte word is 32-bits up. */
        ullNextTime |= ( uint64_t ) ulCurrentTimeLow;
        ullNextTime += ( uint64_t ) uxTimerIncrementsForOneTick;
        *pullMachineTimerCompareRegister = ullNextTime;

        /* Prepare the time to use after the next tick interrupt. */
        ullNextTime += ( uint64_t ) uxTimerIncrementsForOneTick;
    }

#endif /* ( configMTIME_BASE_ADDRESS != 0 ) && ( configMTIME_BASE_ADDRESS != 0 ) */
/*-----------------------------------------------------------*/

BaseType_t xPortStarted = pdFALSE;

BaseType_t xPortStartScheduler( void )
{
extern void xPortStartFirstTask( void );
    xPortStarted = pdTRUE;

    #if( configASSERT_DEFINED == 1 )
    {
        /* Check alignment of the interrupt stack - which is the same as the
         * stack that was being used by main() prior to the scheduler being
         * started. */
        configASSERT( ( xISRStackTop & portBYTE_ALIGNMENT_MASK ) == 0 );

        #ifdef configISR_STACK_SIZE_WORDS
        {
            memset( ( void * ) xISRStack, portISR_STACK_FILL_BYTE, sizeof( xISRStack ) );
        }
        #endif /* configISR_STACK_SIZE_WORDS */
    }
    #endif /* configASSERT_DEFINED */

    #if ( configENABLE_MPU == 1 )
    {
        /* Setup the Memory Protection Unit (MPU). */
        prvSetupMPU();
    }
    #endif /* configENABLE_MPU */

    /* If there is a CLINT then it is ok to use the default implementation
     * in this file, otherwise vPortSetupTimerInterrupt() must be implemented to
     * configure whichever clock is to be used to generate the tick interrupt. */
    vPortSetupTimerInterrupt();

    #if( ( configMTIME_BASE_ADDRESS != 0 ) && ( configMTIMECMP_BASE_ADDRESS != 0 ) )
    {
        /* Enable mtime and external interrupts.  1<<7 for timer interrupt,
         * 1<<11 for external interrupt.  _RB_ What happens here when mtime is
         * not present as with pulpino? */
        __asm volatile( "csrs mie, %0" :: "r"(0x880) );
    }
    #endif /* ( configMTIME_BASE_ADDRESS != 0 ) && ( configMTIMECMP_BASE_ADDRESS != 0 ) */

    xPortStartFirstTask();

    /* Should not get here as after calling xPortStartFirstTask() only tasks
     * should be executing. */
    return pdFAIL;
}
/*-----------------------------------------------------------*/

void vPortEndScheduler( void )
{
    /* Not implemented. */
    for( ;; );
}
/*-----------------------------------------------------------*/

#define PMP_ENTRY_PER_CONFIG        4

#define SETUP_PMP_CONFIG( xPmpConfigs, xPmpEnteryIndex, xPmpConfigValue )           \
do{                                                                                 \
    char temp[64];                                                                  \
    uint32_t xPmpConfigIndex = ( xPmpEnteryIndex ) / PMP_ENTRY_PER_CONFIG;          \
    uint32_t xPmpConfigOffsetIndex = ( xPmpEnteryIndex ) % PMP_ENTRY_PER_CONFIG;    \
    uint32_t xTempPmpConfig;                                                        \
                                                                                    \
    xTempPmpConfig = ( xPmpConfigs )[ xPmpConfigIndex ];                            \
    xTempPmpConfig = xTempPmpConfig & ( ( 0xffffffff ) ^ ( ( ( uint32_t ) 0xff ) << ( xPmpConfigOffsetIndex * 8 ) ) ); \
    xTempPmpConfig = xTempPmpConfig | ( ( xPmpConfigValue ) << ( xPmpConfigOffsetIndex * 8 ) ); \
    ( xPmpConfigs )[ xPmpConfigIndex ] = xTempPmpConfig;                            \
    snprintf( temp, 64, "0x%08x, 0x%08x, 0x%08x, 0x%08x", xPmpConfigIndex, xPmpConfigOffsetIndex, xPmpConfigValue, ( xPmpConfigs )[ xPmpConfigIndex ] ); \
    vSendString( temp );\
}while( 0 )

/*
 * Defines the memory ranges allocated to the task when an MPU is used.
 */
void vPortStoreTaskMPUSettings( xMPU_SETTINGS * xMPUSettings,
                                const struct xMEMORY_REGION * const xRegions,
                                StackType_t * pxBottomOfStack,
                                uint32_t ulStackDepth )
{
    uint32_t i;
    struct pmp_config xPmpConfig;
    size_t uxTaskStackPointer = ( size_t )pxBottomOfStack;
    uint32_t xPmpConfig0 = 0;
    uint32_t uxMpuSettingIndex = 0;
    size_t uxMpuSettingStartAddr = 0;
    size_t uxMpuSettingEndAddr = 0;

    memset( xMPUSettings, 0, sizeof( xMPU_SETTINGS ) );

    if( ulStackDepth > 0 )
    {
        /* Setup the stack start address. */
        xPmpConfig.R = 0;
        xPmpConfig.W = 0;
        xPmpConfig.L = METAL_PMP_UNLOCKED;
        xPmpConfig.X = 0;
        xPmpConfig.A = METAL_PMP_OFF;
        // xPortPmpSetRegion( 0, xPmpConfig, uxTaskStackPointer >> 2 );
        SETUP_PMP_CONFIG( xMPUSettings->pmpcfg, uxMpuSettingIndex, CONFIG_TO_INT( xPmpConfig ) );
        // xPmpConfig0 = ( xPmpConfig0 & 0xffffff00 ) | ( CONFIG_TO_INT( xPmpConfig ) << 0 );
        xMPUSettings->pmpaddress[ uxMpuSettingIndex ] = uxTaskStackPointer >> 2;
        uxMpuSettingIndex = uxMpuSettingIndex + 1;

        /* Setup the stack end address. */
        xPmpConfig.R = 1;
        xPmpConfig.W = 1;
        xPmpConfig.L = METAL_PMP_UNLOCKED;
        xPmpConfig.X = 0;
        xPmpConfig.A = METAL_PMP_TOR;
        // xPortPmpSetRegion( 1, xPmpConfig, ( uxTaskStackPointer + ulStackDepth * sizeof( StackType_t ) ) >> 2 );
        SETUP_PMP_CONFIG( xMPUSettings->pmpcfg, uxMpuSettingIndex, CONFIG_TO_INT( xPmpConfig ) );
        // xPmpConfig0 = ( xPmpConfig0 & 0xffff00ff ) | ( CONFIG_TO_INT( xPmpConfig ) << 8 );
        xMPUSettings->pmpaddress[ uxMpuSettingIndex ] = ( uxTaskStackPointer + ulStackDepth * sizeof( StackType_t ) ) >> 2;
        uxMpuSettingIndex = uxMpuSettingIndex + 1;
    }

    if( xRegions == NULL )
    {
        /* TODO : implements the xRegions is NULL scenario. */
    }
    else
    {
        uxMpuSettingIndex = 2;
        for( i = 0; i < portNUM_CONFIGURABLE_REGIONS; i++ )
        {
            if( xRegions[ i ].ulLengthInBytes > 0 )
            {
                uxMpuSettingStartAddr = ( size_t )( xRegions[ i ].pvBaseAddress );
                uxMpuSettingEndAddr = uxMpuSettingStartAddr + xRegions[ i ].ulLengthInBytes;

                /* Set the empty PMP region. */
                xPmpConfig.R = 0;
                xPmpConfig.W = 0;
                xPmpConfig.L = METAL_PMP_UNLOCKED;
                xPmpConfig.X = 0;
                xPmpConfig.A = METAL_PMP_OFF;
                
                SETUP_PMP_CONFIG( xMPUSettings->pmpcfg, uxMpuSettingIndex, CONFIG_TO_INT( xPmpConfig ) );
                xMPUSettings->pmpaddress[ uxMpuSettingIndex ] = ( uxMpuSettingStartAddr ) >> 2;
                uxMpuSettingIndex = uxMpuSettingIndex + 1;

                /* Set the TOR PMP region. */
                /* Translate the xRegions settings. */
                xPmpConfig.R = 0;
                xPmpConfig.W = 0;
                xPmpConfig.L = METAL_PMP_UNLOCKED;
                xPmpConfig.X = 0;
                xPmpConfig.A = METAL_PMP_TOR;
                if( ( ( xRegions[ i ].ulParameters & portMPU_REGION_READ_ONLY ) == portMPU_REGION_READ_ONLY ) ||
                    ( ( xRegions[ i ].ulParameters & portMPU_REGION_PRIVILEGED_READ_WRITE_UNPRIV_READ_ONLY ) == portMPU_REGION_PRIVILEGED_READ_WRITE_UNPRIV_READ_ONLY ) )
                {
                    xPmpConfig.R = 1;
                }
                if( ( xRegions[ i ].ulParameters & portMPU_REGION_READ_WRITE ) == portMPU_REGION_READ_WRITE )
                {
                    xPmpConfig.R = 1;
                    xPmpConfig.W = 1;
                }
                SETUP_PMP_CONFIG( xMPUSettings->pmpcfg, uxMpuSettingIndex, CONFIG_TO_INT( xPmpConfig ) );
                xMPUSettings->pmpaddress[ uxMpuSettingIndex ] = ( uxMpuSettingEndAddr ) >> 2;
                uxMpuSettingIndex = uxMpuSettingIndex + 1;
            }
        }
    }
}

/*-----------------------------------------------------------*/

#if ( configENABLE_MPU == 1 )
    #include "pmp_apis.h"

    #define BITS_SIZE_MASK( x )  (~( ( 1 << x ) - 1 ) )
    #define BITS_SIZE_NAPOT( x )  ( ( 1 << ( x - 3 ) ) - 1 )

    /* Declaration when these variable are defined in code instead of being
     * exported from linker scripts. */
    extern uint32_t __privileged_functions_start__[];
    extern uint32_t __privileged_functions_end__[];
    extern uint32_t __FLASH_segment_start__[];
    extern uint32_t __FLASH_segment_end__[];
    extern uint32_t __privileged_data_start__[];
    extern uint32_t __privileged_data_end__[];

    struct RegionTable
    {
        size_t startAddress;
        size_t regionSize;
        struct pmp_config pmpConfig;
    };

    struct RegionTable xPmpTable[] =
    {
        /* user stack start. */
        { .startAddress = 0x0, .regionSize = 0, /* ( __privileged_functions_end__ - __privileged_functions_start__ ), */
            .pmpConfig = { .L = 0, .R = 0, .W = 0, .X = 0, .A = METAL_PMP_OFF } },
        /* user stack end. */
        { .startAddress = 0x0, .regionSize = 0, /* ( __privileged_functions_end__ - __privileged_functions_start__ ), */
            .pmpConfig = { .L = 0, .R = 0, .W = 0, .X = 0, .A = METAL_PMP_OFF } },

        /* user defined start. */
        { .startAddress = 0x0, .regionSize = 0, /* ( __privileged_functions_end__ - __privileged_functions_start__ ), */
            .pmpConfig = { .L = 0, .R = 0, .W = 0, .X = 0, .A = METAL_PMP_OFF } },
        /* user defined end. */
        { .startAddress = 0x0, .regionSize = 0, /* ( __privileged_functions_end__ - __privileged_functions_start__ ), */
            .pmpConfig = { .L = 0, .R = 0, .W = 0, .X = 0, .A = METAL_PMP_OFF } },

        /* user defined2 start. */
        { .startAddress = 0x0, .regionSize = 0, /* ( __privileged_functions_end__ - __privileged_functions_start__ ), */
            .pmpConfig = { .L = 0, .R = 0, .W = 0, .X = 0, .A = METAL_PMP_OFF } },
        /* user defined2 end. */
        { .startAddress = 0x0, .regionSize = 0, /* ( __privileged_functions_end__ - __privileged_functions_start__ ), */
            .pmpConfig = { .L = 0, .R = 0, .W = 0, .X = 0, .A = METAL_PMP_OFF } },

        /* user defined3 start. */
        { .startAddress = 0x0, .regionSize = 0, /* ( __privileged_functions_end__ - __privileged_functions_start__ ), */
            .pmpConfig = { .L = 0, .R = 0, .W = 0, .X = 0, .A = METAL_PMP_OFF } },
        /* user defined3 end. */
        { .startAddress = 0x0, .regionSize = 0, /* ( __privileged_functions_end__ - __privileged_functions_start__ ), */
            .pmpConfig = { .L = 0, .R = 0, .W = 0, .X = 0, .A = METAL_PMP_OFF } },

        /* privileged_functions unprivileged access. */
        { .startAddress = __privileged_functions_start__, .regionSize = 32*1024, /* ( __privileged_functions_end__ - __privileged_functions_start__ ), */
            .pmpConfig = { .L = 0, .R = 0, .W = 0, .X = 0, .A = METAL_PMP_NAPOT } },
        /* privileged_data unprivileged access. */
        { .startAddress = __privileged_data_start__, .regionSize = 64*1024, /* ( __privileged_data_end___ - __privileged_data_start__ ), */
            .pmpConfig = { .L = 0, .R = 0, .W = 0, .X = 0, .A = METAL_PMP_NAPOT } },

        /* unprivileged_functions unprivileged access. */
        { .startAddress = __privileged_functions_start__, .regionSize = 512*1024, /* ( __FLASH_segment_end__ - __FLASH_segment_start__ ),*/
            .pmpConfig = { .L = 0, .R = 1, .W = 0, .X = 1, .A = METAL_PMP_NAPOT } },
        /* unprivileged_data unprivileged access. */
        { .startAddress = __privileged_data_start__, .regionSize = 512*1024, /* ( 256 * 1024 ), */
            .pmpConfig = { .L = 0, .R = 1, .W = 1, .X = 0, .A = METAL_PMP_NAPOT } },

        /* Peripheral for UART. */
        { .startAddress = 0x10000000, .regionSize = 128, /* ( 256 * 1024 ), */
            .pmpConfig = { .L = 0, .R = 1, .W = 1, .X = 0, .A = METAL_PMP_NAPOT } },
    };

    static void prvListPmpRegions( void )
    {
        char temp[ 128 ];
        int xPmpRegions;
        struct pmp_config xPmpConfig;
        size_t pmp_address;
        int i;

        /* List all the PMP region. */
        xPmpRegions = xPortPmpGetNumRegions();
        for( i = 0; i < xPmpRegions; i++ )
        {
            xPortPmpGetRegion( i, &xPmpConfig, &pmp_address );
            snprintf( temp, 128, "PMP %d : 0x%08x R %d W %d X %d L %d A %d", i,
                pmp_address,
                xPmpConfig.R,
                xPmpConfig.W,
                xPmpConfig.X,
                xPmpConfig.L,
                xPmpConfig.A );
            vSendString( temp );
        }
    }

    static void prvSetupMPU( void ) /* PRIVILEGED_FUNCTION */
    {
        uint32_t uxPmpIndex;
        struct pmp_config xPmpConfig;
        size_t xPmpAddress = 0x0;

        /* Initialize all the PMP regions to off state. */
        xPmpConfig.R = 0;
        xPmpConfig.W = 0;
        xPmpConfig.L = METAL_PMP_UNLOCKED;
        xPmpConfig.X = 0;
        xPmpConfig.A = METAL_PMP_OFF;
        for( uxPmpIndex = 0; uxPmpIndex < xPortPmpGetNumRegions(); uxPmpIndex++ )
        {
            xPortPmpSetRegion( uxPmpIndex, xPmpConfig, 0 );
        }

        /* Setup common PMP regions. */
        for( uxPmpIndex = 0; uxPmpIndex < 13; uxPmpIndex++ )
        {
            if( xPmpTable[ uxPmpIndex ].pmpConfig.A == METAL_PMP_NAPOT )
            {
                xPmpAddress = xConvertNAPOTSize( xPmpTable[ uxPmpIndex ].startAddress, xPmpTable[ uxPmpIndex ].regionSize );
            }
            xPortPmpSetRegion( uxPmpIndex, xPmpTable[ uxPmpIndex ].pmpConfig, xPmpAddress );
        }

        /* Dump all the table. */
        prvListPmpRegions();
    }
#endif /* configENABLE_MPU */
