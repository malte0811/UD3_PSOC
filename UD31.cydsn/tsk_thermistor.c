/*
 * UD3
 *
 * Copyright (c) 2018 Jens Kerrinnes
 * Copyright (c) 2015 Steve Ward
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
*/

#include <cytypes.h>
#include "cyapicallbacks.h"   

#include "tsk_thermistor.h"

#include "FreeRTOS.h"
#include "FreeRTOS_task.h"
#include "FreeRTOS_semphr.h"

xTaskHandle tsk_thermistor_TaskHandle;
uint8 tsk_thermistor_initVar = 0u;

#if (1 == 1)
	xSemaphoreHandle tsk_thermistor_Mutex;
#endif

/* ------------------------------------------------------------------------ */
/*
 * Place user included headers, defines and task global data in the
 * below merge region section.
 */
/* `#START USER_INCLUDE SECTION` */
#include "tsk_priority.h"
#include "telemetry.h"
#include "charging.h"
#include "cli_common.h"

#define TEMP_INC 5
#define RES_TABLE_SIZE 32

#define RES_SHORT       270 //internal resistance to DAC
#define THERM_DAC_VAL   23
#define THERM_DAC_STEP  8   //micro amps

#define TEMP1_FAULT     0b0001
#define TEMP2_FAULT     0b0010
    
#define TEMP_FAULT_LOW  0xFFFF


#define TEMP_FAULT_COUNTER_MAX  5

/* DMA Configuration for therm_dma */
#define therm_dma_BYTES_PER_BURST 2
#define therm_dma_REQUEST_PER_BURST 1
#define therm_dma_SRC_BASE (CYDEV_PERIPH_BASE)
#define therm_dma_DST_BASE (therm_sample)


uint16_t therm_sample[2];
uint16_t temp_fault_counter;

//temperature *128
const uint16 count_temp_table[] ={
15508,	10939,	8859,	7536,	6579,	5826,	5220,	4715,	4275,	3879,	3551,	3229,	2965,	2706,	2469,	2261,
2054,	1861,	1695,	1530,	1364,	1215,	1084,	953,	822,	690,	576,	473,	369,	266,	163,	59
};



/* `#END` */
/* ------------------------------------------------------------------------ */
/*
 * User defined task-local code that is used to process commands, control
 * operations, and/or generrally do stuff to make the taks do something
 * meaningful.
 */
/* `#START USER_TASK_LOCAL_CODE` */

void initialize_thermistor (void)
{
    IDAC_therm_Start();
    ADC_therm_Start();
    IDAC_therm_SetValue(THERM_DAC_VAL);
    //this enables the thermistor conversion ISR  
//    therm_sample_StartEx(therm_sample_ISR);

    temp_fault_counter = 0;
        
    /* Variable declarations for therm1_dma */
    /* Move these variable declarations to the top of the function */
    uint8 therm_dma_Chan;
    uint8 therm_dma_TD[1];
    therm_dma_Chan = therm_dma_DmaInitialize(therm_dma_BYTES_PER_BURST, therm_dma_REQUEST_PER_BURST, 
        HI16(therm_dma_SRC_BASE), HI16(therm_dma_DST_BASE));
    therm_dma_TD[0] = CyDmaTdAllocate();
    CyDmaTdSetConfiguration(therm_dma_TD[0], 4, therm_dma_TD[0], TD_INC_DST_ADR);
    CyDmaTdSetAddress(therm_dma_TD[0], LO16((uint32)ADC_therm_SAR_WRK0_PTR), LO16((uint32)therm_sample));
    CyDmaChSetInitialTd(therm_dma_Chan, therm_dma_TD[0]);
    CyDmaChEnable(therm_dma_Chan, 1);
    
}

uint16_t get_temp_128(uint16_t counts){
    uint16_t counts_div = counts /128;
    if(!counts_div) return TEMP_FAULT_LOW;
    uint32_t counts_window = counts_div *128; 

    counts_div--;
    if(counts > counts_window){
        return count_temp_table[counts_div]-(((uint32_t)(count_temp_table[counts_div] - count_temp_table[counts_div+1])*((uint32_t)counts-counts_window))/128);
    }else{
        return count_temp_table[counts_div]-(((uint32_t)(count_temp_table[counts_div-1] - count_temp_table[counts_div])*((uint32_t)counts-counts_window))/128);
    }   
}
  

uint8 run_temp_check (void){
//this function looks at all the thermistor temperatures, compares them against limits and returns any faults
    uint8 fault = 0;
    telemetry.temp1 = get_temp_128(therm_sample[0])/128;
    telemetry.temp2 = get_temp_128(therm_sample[1])/128;
    
    if(telemetry.temp1 > confparam[CONF_TEMP1_MAX].value)
    {
        fault |= TEMP1_FAULT;
    }
    if(telemetry.temp2 > confparam[CONF_TEMP2_MAX].value)
    {
        fault |= TEMP2_FAULT;
    }
    
    if(telemetry.temp1 > confparam[CONF_TEMP1_SETPOINT].value)
    {
        fan_ctrl_Control = 1;
    }
    else if(telemetry.temp1 < confparam[CONF_TEMP1_SETPOINT].value)
    {
        fan_ctrl_Control = 0;
    }    
    return fault;
}



/* `#END` */
/* ------------------------------------------------------------------------ */
/*
 * This is the main procedure that comprises the task.  Place the code required
 * to preform the desired function within the merge regions of the task procedure
 * to add functionality to the task.
 */
void tsk_thermistor_TaskProc( void *pvParameters )
{
	/*
	 * Add and initialize local variables that are allocated on the Task stack
	 * the the section below.
	 */
	/* `#START TASK_VARIABLES` */
    
  
	/* `#END` */
	
	/*
	 * Add the task initialzation code in the below merge region to be included
	 * in the task.
	 */
	/* `#START TASK_INIT_CODE` */
    
    initialize_thermistor();
    
    
	/* `#END` */
	
	for(;;) {
		/* `#START TASK_LOOP_CODE` */
 
        if(run_temp_check()){
            temp_fault_counter++;
            if(temp_fault_counter > TEMP_FAULT_COUNTER_MAX){
                telemetry.bus_status = BUS_TEMP1_FAULT;
                bus_command = BUS_COMMAND_FAULT;
            }    
        }else{
        //system_fault_Control = 1;
        temp_fault_counter = 0;
        }

		/* `#END` */

		vTaskDelay( 1000/ portTICK_PERIOD_MS);
	}
}
/* ------------------------------------------------------------------------ */
void tsk_thermistor_Start( void )
{
	/*
	 * Insert task global memeory initialization here. Since the OS does not
	 * initialize ANY global memory, execute the initialization here to make
	 * sure that your task data is properly 
	 */
	/* `#START TASK_GLOBAL_INIT` */

	/* `#END` */
	
	if (tsk_thermistor_initVar != 1) {
		#if (1 == 1)
			tsk_thermistor_Mutex = xSemaphoreCreateMutex();
		#endif
	
		/*
	 	* Create the task and then leave. When FreeRTOS starts up the scheduler
	 	* will call the task procedure and start execution of the task.
	 	*/
		xTaskCreate(tsk_thermistor_TaskProc,"Therm",512,NULL,PRIO_THERMISTOR,&tsk_thermistor_TaskHandle);
		tsk_thermistor_initVar = 1;
	}
}
/* ------------------------------------------------------------------------ */
/* ======================================================================== */
/* [] END OF FILE */
