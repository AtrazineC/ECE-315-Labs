#include "stdio.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xil_types.h"
#include "xtime_l.h"

#include "uart_driver.h"
#include "initialization.h"

#define CHAR_ESC                0x23   
#define CHAR_CARRIAGE_RETURN    0x0D   
#define CHAR_PERCENT    		0x25    

void printString(char countMessage[]);

TaskHandle_t task_receiveuarthandle = NULL;
TaskHandle_t task_transmituarthandle = NULL;
BaseType_t resetFlag;
BaseType_t endingSeq;
int Countbytes;                               
extern QueueHandle_t xQueue_for_transmit;
extern QueueHandle_t xQueue_for_receive;
extern int CountRxIrq;                            
extern int CountTxIrq;                            



//Function declaration for UART interrupt setup
extern int SetupInterruptSystem(INTC* IntcInstancePtr, 
            XUartPs* UartInstancePtr, u16 UartIntrId);

//Initialization function for UART
extern int Initialize_UART();

//the four driver functions whose definitions are in "uart_driver.h" file.
extern BaseType_t MyIsReceiveData(void);

extern u8 MyReceiveByte(void);

extern BaseType_t MyIsTransmitFull(void);

extern void MySendByte(u8 Data);


int main() {
    int Status;

    xTaskCreate(Task_UART_buffer_receive, "uart_receive_task", 
                1024, (void*) 0, tskIDLE_PRIORITY,
                &task_receiveuarthandle);
    xTaskCreate(Task_UART_buffer_send, "uart_transmit_task", 
                1024, (void*) 0, tskIDLE_PRIORITY,
                &task_transmituarthandle);

    xQueue_for_transmit = xQueueCreate(SIZE_OF_QUEUE, sizeof(u8));
    xQueue_for_receive = xQueueCreate(SIZE_OF_QUEUE, sizeof(u8));
    CountRxIrq = 0;
    CountTxIrq = 0;
    Countbytes = 0;
    resetFlag = pdFALSE;
    endingSeq = pdFALSE;

    Status = Initialize_UART();
    if (Status != XST_SUCCESS) {
        xil_printf("UART Initialization failed\n");
    }

    vTaskStartScheduler();

    while (1);

    return 0;

}

void Task_UART_buffer_receive(void* p) {

    int Status;

    Status = SetupInterruptSystem(&InterruptController, &UART, 
            UART_INT_IRQ_ID);
    if (Status != XST_SUCCESS) {
        xil_printf("UART PS interrupt failed\n");
    }

    u8 returnFlag = 0;
    u8 restartFlag = 0;

    printString("Please enter the text and then press ENTER\n");
    printString("To display the status messages, 
            enter the end-of-block sequence, \\r#\\r and then 
            press ENTER\n");
    printString(
            "To reset the ISR transmit and receive global 
            counters as well as byte counter, enter the 
            sequence \\r%\\r and then press ENTER\n");

    for (;;) {
        while (1) {
            u8 pcString;
            char write_to_queue_value;

            while (MyIsReceiveData() == pdFALSE) {};
			pcString = MyReceiveByte();
			while (MyIsTransmitFull() == pdTRUE) {};

            write_to_queue_value = (char) pcString;    //casted to "char" type.

            if (islower(write_to_queue_value)) {
            	write_to_queue_value = toupper(write_to_queue_value);
            } else {
            	write_to_queue_value = tolower(write_to_queue_value);
            }

            Countbytes++;
            MySendByte(write_to_queue_value);

            //detect \r#\r
            if (returnFlag == 2 
                    && write_to_queue_value == CHAR_CARRIAGE_RETURN) {
                returnFlag = 0;
                taskYIELD(); //force context switch
            } else if (returnFlag == 1 && write_to_queue_value == CHAR_ESC) {
                returnFlag = 2;
            } else if (write_to_queue_value == CHAR_CARRIAGE_RETURN) {
                returnFlag = 1;
            } else {
                returnFlag = 0;
            }

            if (restartFlag == 2 && write_to_queue_value == CHAR_CARRIAGE_RETURN) {
            	restartFlag = 0;
                CountRxIrq = 0;
    			CountTxIrq = 0;
    			Countbytes = 0;
    			xil_printf("\nByte Counter, CountRxIrq && 
                            CountTxIrq set to zero\n\n");
                taskYIELD(); //force context switch
            } else if (restartFlag == 1 
                    && write_to_queue_value == CHAR_PERCENT) {
            	restartFlag = 2;
            } else if (write_to_queue_value == CHAR_CARRIAGE_RETURN) {
            	restartFlag = 1;
            } else {
            	restartFlag = 0;
            }
        }
    }
}


//print the provided number using driver functions
void printNumber(char number[]) {
    for (int i = 0; i < 10; i++) {
        if (number[i] >= '0' && number[i] <= '9') {
            while (MyIsTransmitFull() == pdTRUE);
            MySendByte(number[i]);
        }
    }
}

//print the provided string using driver functions
void printString(char countMessage[]) {
    for (int i = 0; countMessage[i] != '\0'; i++) {
        while (MyIsTransmitFull() == pdTRUE);
        MySendByte(countMessage[i]);
    }
}

void Task_UART_buffer_send(void* p) {

    UBaseType_t uxPriority;

    for (;;) {
        uxPriority = uxTaskPriorityGet(NULL);
        while (1) {

            //print the special messages for ending sequence
            char countArray[10];
            char CountRxIrqArray[10];
            char CountTxIrqArray[10];
            sprintf(countArray, "%d", Countbytes);
            sprintf(CountRxIrqArray, "%d", CountRxIrq);
            sprintf(CountTxIrqArray, "%d", CountTxIrq);
            printString("Number of bytes processed: ");
            printNumber(countArray);
            MySendByte(CHAR_CARRIAGE_RETURN);
            printString("Number of Rx interrupts: ");
            printNumber(CountRxIrqArray);
            MySendByte(CHAR_CARRIAGE_RETURN);
            printString("Number of Tx interrupts: ");
            printNumber(CountTxIrqArray);
            MySendByte(CHAR_CARRIAGE_RETURN);


            //perform context switch
            taskYIELD();
        }
    }
}

