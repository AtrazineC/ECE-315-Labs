#include "xparameters.h"
#include "xplatform_info.h"
#include "xuartps.h"
#include "xil_exception.h"
#include "xil_printf.h"
#include "xscugic.h"

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "stdio.h"
#include "string.h"

// header file containing the morse code for different characters
#include "morseTranslator.h"

/************************** Constant Definitions *****************************/

//UART definitions from xparameters.h
#define UART_DEVICE_ID              XPAR_XUARTPS_0_DEVICE_ID
#define UART_BASEADDR                XPAR_XUARTPS_0_BASEADDR

//definitions for the terminating sequence characters
#define CHAR_HASH                0x23   
#define CHAR_CARRIAGE_RETURN    0x0D   

#define TASK_PROCESS_BUFFER    500
#define RECEIVER_POLL_PERIOD_MS 20 

//queue limit definitions
#define XQUEUE_12_CAPACITY        20
#define XQUEUE_23_CAPACITY        30
//error message
#define ERROR_MESSAGE "Maximum message length exceeded. \
Message ignored.\rType in the termination sequence to \
translate the 'excess' characters that went above the array \
limit and caused overflow.\r\r\n"

/***************** Macros (Inline Functions) Definitions *********************/

static void TaskMorseMsgReceiver(void* pvParameters);

static TaskHandle_t xTask_msgreceive;

static void TaskMorseMsgProcessor(void* pvParameters);

static TaskHandle_t xTask_msgprocess;

static void TaskDecodedMsgTransmitter(void* pvParameters);

static TaskHandle_t xTask_msgtransmit;

static void UART_print_queueerror_msg(char* str);

static QueueHandle_t xQueue_12 = NULL;    
static QueueHandle_t xQueue_23 = NULL;  

void morseToTextConverter(char input_char);

/************************** Function Prototypes ******************************/

int Intialize_UART(u16 DeviceId);

/************************** Variable Definitions *****************************/

XUartPs Uart_PS;     
XUartPs_Config* Config;

char char_morse_sequence[10];
char output_text_sequence[500];
int output_length;
int char_seq_length;

int main() {
    int Status;

    xTaskCreate(TaskMorseMsgReceiver,
                (const char*) "T1",
                configMINIMAL_STACK_SIZE * 5,
                NULL,
                tskIDLE_PRIORITY,
                &xTask_msgreceive);

    xTaskCreate(TaskMorseMsgProcessor,
                (const char*) "T2",
                configMINIMAL_STACK_SIZE * 5,
                NULL,
                tskIDLE_PRIORITY,
                &xTask_msgprocess);

    xTaskCreate(TaskDecodedMsgTransmitter,
                (const char*) "T3",
                configMINIMAL_STACK_SIZE * 5,
                NULL,
                tskIDLE_PRIORITY,
                &xTask_msgtransmit);


    Status = Intialize_UART(UART_DEVICE_ID);
    if (Status != XST_SUCCESS) {
        xil_printf("UART Polled Mode Example Test Failed\r\n");
    }

    xQueue_12 = xQueueCreate(XQUEUE_12_CAPACITY, sizeof(u8));

    xQueue_23 = xQueueCreate(XQUEUE_23_CAPACITY, sizeof(u8));


    /* Check the queue was created. */
    configASSERT(xQueue_12);
    configASSERT(xQueue_23);

    xil_printf("Please type in your morse code below to be translated to text\n\n");
    xil_printf("Remember the following: \r	1) Add a '|' after every letter\r");
    xil_printf("	2) One space between each word\r");
    xil_printf("	3) One carriage return (ENTER) to write on a new line\r");
    xil_printf("	4) And when you are done, type in the termination sequence\n\n");

    vTaskStartScheduler();

    while (1);
    return 0;
}


int Intialize_UART(u16 DeviceId) {
    int Status;
    /*
     * Initialize the UART driver so that it's ready to use.
     * Look up the configuration in the config table, then initialize it.
     */
    Config = XUartPs_LookupConfig(DeviceId);
    if (NULL == Config) {
        return XST_FAILURE;
    }

    Status = XUartPs_CfgInitialize(&Uart_PS, Config, Config->BaseAddress);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    /* Use NORMAL UART mode. */
    XUartPs_SetOperMode(&Uart_PS, XUARTPS_OPER_MODE_NORMAL);

    return XST_SUCCESS;
}

static void TaskMorseMsgReceiver(void* pvParameters) {
    u8 RecvChar;
    const TickType_t xPeriod = pdMS_TO_TICKS(RECEIVER_POLL_PERIOD_MS);
    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
    	vTaskDelayUntil(&xLastWakeTime, xPeriod);

        // Receive characters over UART if there's room in the queue
        if (uxQueueMessagesWaiting(xQueue_12) < XQUEUE_12_CAPACITY) {
        	RecvChar = XUartPs_RecvByte(UART_BASEADDR);
            if (xQueueSendToBack(xQueue_12, &RecvChar, portMAX_DELAY) != pdPASS) {
                UART_print_queueerror_msg("xQueueSendToBack: Could not 
                    write character to xQueue_12\n");
            }
        }
    }
}

static void TaskMorseMsgProcessor(void* pvParameters) {
    static char a[TASK_PROCESS_BUFFER];
    int no_of_characters_read;
    BaseType_t break_loop;
    int error_flag;

    for (;;) {
        no_of_characters_read = 0;
        break_loop = FALSE;
        output_length = 0;
        error_flag = 0;
        memset(output_text_sequence, 0, TASK_PROCESS_BUFFER);
    	int sucess = pdFALSE;

    	while (no_of_characters_read < TASK_PROCESS_BUFFER || sucess != pdTRUE) {
        	no_of_characters_read++;
        	sucess = xQueueReceive(xQueue_12, 
                    &a[no_of_characters_read-1], portMAX_DELAY);
			if (no_of_characters_read >= 4 &&
				a[no_of_characters_read - 3] == CHAR_CARRIAGE_RETURN &&
				a[no_of_characters_read - 2] == CHAR_HASH &&
				a[no_of_characters_read-1] == CHAR_CARRIAGE_RETURN) {
				for (int i = 0; i < no_of_characters_read; i++) {
					char c = a[i];
					morseToTextConverter(c);
				}
				break_loop = TRUE;
				break;
			}

        }

        if (!break_loop) {
        	strcpy(a, ERROR_MESSAGE);
			output_length = strlen(ERROR_MESSAGE);
			error_flag = 1;
        }

        char* queue;
        if (error_flag == 1) {
            queue = a;
        } else {
            queue = output_text_sequence;
        }

		for (int i = 0; i < output_length; i++) {
			xQueueSendToBack(xQueue_23, &queue[i], portMAX_DELAY);
		}
    }
}

static void TaskDecodedMsgTransmitter(void* pvParameters) {

    u8 write_to_console;
    const TickType_t xPeriod = pdMS_TO_TICKS(RECEIVER_POLL_PERIOD_MS);
    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        while (xQueueReceive(xQueue_23, &write_to_console, 
            portMAX_DELAY) != pdTRUE);

        while (1) {
            if (XUartPs_IsTransmitFull(UART_BASEADDR)) {
                vTaskDelayUntil(&xLastWakeTime, xPeriod);
            } else {
				XUartPs_SendByte(UART_BASEADDR, write_to_console);
                break;
            }
        }
    }
}

/*
 * Writes a null-terminated string to the UART device 
 * set by UART_DEVICE_ID.
 */
static void UART_print_queueerror_msg(char* str) {
    int i = 0;
    while (TRUE) {
        XUartPs_SendByte(UART_BASEADDR, str[i]);
        ++i;

        // Do not print the null byte terminator
        if (str[i] == '\0')
            return;
    }
}

