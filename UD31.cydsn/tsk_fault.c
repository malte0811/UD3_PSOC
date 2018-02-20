/*
 * Copyright (c) 2014 Steve Ward
 * Copyright (c) 2018 Jens Kerrinnes
 * LICENCE: MIT License (look at /LICENCE.md)
 */

#include <cytypes.h>
#include "cyapicallbacks.h"   

#include "tsk_fault.h"

#include "FreeRTOS.h"
#include "FreeRTOS_task.h"
#include "FreeRTOS_semphr.h"

xTaskHandle tsk_fault_TaskHandle;
uint8 tsk_fault_initVar = 0u;

#if (1 == 1)
	xSemaphoreHandle tsk_fault_Mutex;
#endif

/* ------------------------------------------------------------------------ */
/*
 * Place user included headers, defines and task global data in the
 * below merge region section.
 */
/* `#START USER_INCLUDE SECTION` */
#include "tsk_priority.h"
#include "telemetry.h"
#include "cli_common.h"
#include "charging.h"




/* `#END` */
/* ------------------------------------------------------------------------ */
/*
 * User defined task-local code that is used to process commands, control
 * operations, and/or generrally do stuff to make the taks do something
 * meaningful.
 */
/* `#START USER_TASK_LOCAL_CODE` */

void handle_watchdog_reset(void){
    //for test use: set wd_kill = 1 to defeat the watchdog timeout
    if(!confparam[CONF_WD].value)
    {
        watchdog_reset_Control = 1;
        watchdog_reset_Control = 0;    
    }
}

void handle_UVLO(void){
    //UVLO feedback via system_fault (LED2)
    telemetry.uvlo_stat = UVLO_status_Status;
    if((bus_command == BUS_COMMAND_FAULT)||(telemetry.uvlo_stat == 0)||(telemetry.bus_status == BUS_CHARGING))
    {
        system_fault_Control = 0;
    }
    else
    {
        system_fault_Control = 1;
    }  
}



/* `#END` */
/* ------------------------------------------------------------------------ */
/*
 * This is the main procedure that comprises the task.  Place the code required
 * to preform the desired function within the merge regions of the task procedure
 * to add functionality to the task.
 */
void tsk_fault_TaskProc( void *pvParameters )
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
    
    
    
    
	/* `#END` */
	
	for(;;) {
		/* `#START TASK_LOOP_CODE` */
        handle_UVLO();
        handle_watchdog_reset();
     
		/* `#END` */

		vTaskDelay( 100/ portTICK_PERIOD_MS);
	}
}
/* ------------------------------------------------------------------------ */
void tsk_fault_Start( void )
{
	/*
	 * Insert task global memeory initialization here. Since the OS does not
	 * initialize ANY global memory, execute the initialization here to make
	 * sure that your task data is properly 
	 */
	/* `#START TASK_GLOBAL_INIT` */

	/* `#END` */
	
	if (tsk_fault_initVar != 1) {
		#if (1 == 1)
			tsk_fault_Mutex = xSemaphoreCreateMutex();
		#endif
	
		/*
	 	* Create the task and then leave. When FreeRTOS starts up the scheduler
	 	* will call the task procedure and start execution of the task.
	 	*/
		xTaskCreate(tsk_fault_TaskProc,"Fault-Svc",128,NULL,PRIO_FAULT,&tsk_fault_TaskHandle);
		tsk_fault_initVar = 1;
	}
}
/* ------------------------------------------------------------------------ */
/* ======================================================================== */
/* [] END OF FILE */