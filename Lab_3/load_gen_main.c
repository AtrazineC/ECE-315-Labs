/*
* ECE - 315 : WINTER 2022
* LAB 3: Interfacing Using the Zynq-7000 SPI Interface - PART 2
* ------------------------------------------------------------------------------
*
* The code consists of 3 tasks. Task 1(loopCountProcessorTask) is responsible for incrementing loop count and applying a certain delay after each increment.
* During this delay, Task 2(cpuLoadGenTask) then takes over and executes a simple bitwise operation loop-count number of times.
* Task 3 (printRunTimeStatsTask) is responsible for printing the load percentages of each task at constant time intervals (every 4 seconds).
* The idea behind this is that as the loop count increases, the amount of time cpuLoadGenTask is running on the CPU will increase (i.e the load percentage).
* And as the load percentage of the cpuLoadGenTask increases, the amount of time the CPU is IDLE will decrease (i.e IDLE load percentage decreases).
* Students have to find the loop count values at which IDLE% is 90%, 80%, 70%, 60%, 50% and 40%.
* Students have to add the code at the designated commented sections to achieve the working model of this exercise.
*
* Created by: Shyama M. Gandhi, Mazen ElBaz
*/

/*****************************  FreeRTOS includes. ****************************/
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "stdbool.h"
#include <stdio.h>

/***************************** Include Files *********************************/

#include "xparameters.h"    /* SDK generated parameters */
#include "xuartps.h"        /* UART device driver */
#include "xspips.h"            /* SPI device driver */
#include "xil_printf.h"
#include <ctype.h>

/************************** Constant Definitions *****************************/
#define UART_DEVICE_ID_0            XPAR_XUARTPS_0_DEVICE_ID
#define UART_BASEADDR                XPAR_XUARTPS_0_BASEADDR

#define CHAR_CARRIAGE_RETURN        0x0D

/***************** Macros (Inline Functions) Definitions *********************/
static void TaskCpuLoadGen(void *pvParameters);

static TaskHandle_t xTask_cpuload;

static void TaskPrintRunTimeStats(void *pvParameters);

static TaskHandle_t xTask_print_run_time_stats_task_handle;

static void TaskLoopCountProcessor(void *pvParameters);

static TaskHandle_t xTask_loop_count_processor_task_handle;

/************************* Global Variables *********************************/
/*
 * 90%: 50000
 * 80%: 125000
 * 70%: 200000
 * 60%: 275000
 * 50%: 350000
 * 40%: 400000
 */
u32 loop_count = 50000;  //GLOBAL variable for loop_count to generate a fake CPU load
u32 run;

int main(void) {
    xTaskCreate(TaskLoopCountProcessor,
                (const char *)"Loop_Count_Manager",
                configMINIMAL_STACK_SIZE * 5,
                NULL,
                tskIDLE_PRIORITY + 1,
                &xTask_loop_count_processor_task_handle);

    xTaskCreate(TaskPrintRunTimeStats,
                (const char *)"Stats",
                configMINIMAL_STACK_SIZE * 5,
                NULL,
                tskIDLE_PRIORITY + 1,
                &xTask_print_run_time_stats_task_handle);


    xTaskCreate(TaskCpuLoadGen,
                (const char *)"Load_Gen",
                configMINIMAL_STACK_SIZE * 5,
                NULL,
                tskIDLE_PRIORITY,
                &xTask_cpuload);

    vTaskStartScheduler();

    while (1);

    return 0;
}


static void TaskLoopCountProcessor(void *pvParameters) {
    TickType_t delay = pdMS_TO_TICKS(60000);

    for (;;) {

        /*
        * Increment loop count by a specified amount of your choice (maybe increments of 25K or 50K)
        * For debugging purposes, you can print the loop count everytime it's incremented
        * After incrementing, apply a delay (you should initialize it outside of the for loop)
        * Find a delay that gives enough time for the cpuLoadGenTask to stabilize at the current value of loop count
        * Hint: After a certain loop count, you might observe that the delay isn't long enough anymore and needs to be adjusted
        * That means, incase if you choose a loop count increment higher 50000, you will have to provide more delay to let this loop count stabilize!
        * Above statements explain the overview of this FreeRTOS task. Please add the code in the comments below to achieve the implementation.
        */

        if (loop_count >= 500000) {
            /**************************************************/
            //update the "delay" variable here to provide the delay of 90000
            delay = pdMS_TO_TICKS(90000);
            /**************************************************/
        }
        if (loop_count >= 1000000) {
            /**************************************************/
            //update the "delay" variable here to provide the delay of 120000
            delay = pdMS_TO_TICKS(120000);
            /**************************************************/
        }

        /**************************************************/
        //write a line of code that increments the "loop_count" variable by 25000/50000
        loop_count += 25000;
        /**************************************************/
        run = 0;
        xil_printf("\n\nCurrent loop count value = %d\n\n", loop_count);
        vTaskDelay(delay);

        vTaskDelay(1);
    }
}


//Generate fake load on the CPU
static void TaskCpuLoadGen(void *pvParameters) {
    u32 var = 0x12345678;

    while (1) {
        /**************************************************/
        /*
        * Setup a for loop where this task is executing a simple bitwise complement operation for "loop_count" number of times on the variable "var"
        */
        for (u32 i = 0; i < loop_count; i++) {
            var = !var;
        }
        /*************************************************/
        vTaskDelay(1);
    }
}

//Task to print the statistics
static void TaskPrintRunTimeStats(void *pvParameters) {
    const TickType_t delay = pdMS_TO_TICKS(4000);

    while (1) {
        xil_printf("\nPrinting run time stats (loop count = %d)\n", loop_count);

        /**************************************************/
        /*
        * Use the vTaskGetRunTimeStats() function to find the load percentages of all running tasks
        * Print the results of the vTaskGetRunTimeStats() function onto the terminal
        * Declare the output variable that you want to use with the vTaskGetRunTimeStats() function
        */
        char b[1024];
        vTaskGetRunTimeStats(b);
        xil_printf(b);
        /*************************************************/
        run++;
        vTaskDelay(delay);
    }
}
