#ifndef SRC_UART_DRIVER_H_
#define SRC_UART_DRIVER_H_

#include "xil_io.h"
#include "xuartps.h"
#include "xscugic.h"

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "initialization.h"

void Task_UART_buffer_receive(void *p);

void Task_UART_buffer_send(void *p);

QueueHandle_t xQueue_for_transmit;
QueueHandle_t xQueue_for_receive;
int CountRxIrq;
int CountTxIrq;

#define UART_BASEADDR       XPAR_XUARTPS_0_BASEADDR

void Interrupt_Handler(void *CallBackRef, u32 Event, unsigned int EventData) {
    u8 receive_buffer;
    u8 transmit_data;
    u32 mask;
    if (Event == XUARTPS_EVENT_RECV_DATA) {
        BaseType_t xHigherPriorityTaskWoken;
        xHigherPriorityTaskWoken = pdFALSE;

        CountRxIrq++;

        while (XUartPs_IsReceiveData(UART_BASEADDR)) {
            receive_buffer = XUartPs_RecvByte(UART_BASEADDR);
            xQueueSendToBackFromISR(xQueue_for_receive, 
					&receive_buffer, &xHigherPriorityTaskWoken);
        }
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    } else if (Event == XUARTPS_EVENT_SENT_DATA) {
        BaseType_t xHigherPriorityTaskWoken;
        xHigherPriorityTaskWoken = pdFALSE;
        CountTxIrq++;
        while (uxQueueMessagesWaitingFromISR(xQueue_for_transmit) > 0 &&
               !XUartPs_IsTransmitFull(XPAR_XUARTPS_0_BASEADDR)) {
            xQueueReceiveFromISR(xQueue_for_transmit, 
					&transmit_data, &xHigherPriorityTaskWoken);
            XUartPs_SendByte(UART_BASEADDR, transmit_data);

        }
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        if (uxQueueMessagesWaitingFromISR(xQueue_for_transmit) <= 0) {
            mask = XUartPs_GetInterruptMask(&UART);
            XUartPs_SetInterruptMask(&UART, mask & ~XUARTPS_IXR_TXEMPTY);
        }
    } else {
        xil_printf("Nor a RECEIVE event nor a SEND event\n");
    }
}

BaseType_t MyIsReceiveData(void) {
    if (uxQueueMessagesWaiting(xQueue_for_receive) > 0) {
        return pdTRUE;
    } else {
        return pdFALSE;
    }
}

u8 MyReceiveByte(void) {
    u8 recv;
    taskENTER_CRITICAL();
    xQueueReceive(xQueue_for_receive, &recv, portMAX_DELAY);
    taskEXIT_CRITICAL();
    return recv;
}

BaseType_t MyIsTransmitFull(void) {
    if (uxQueueSpacesAvailable(xQueue_for_transmit) == 0) {
        return pdTRUE;
    } else {
        return pdFALSE;
    }
}

void MySendByte(u8 Data) {
    u32 mask;
    //secure it
    taskENTER_CRITICAL();

    mask = XUartPs_GetInterruptMask(&UART);
    XUartPs_SetInterruptMask(&UART, mask | XUARTPS_IXR_TXEMPTY);

    // if transmit FIFO empty, use polling,
    // otherwise insert to queue for interrupt method
    if (XUartPs_IsTransmitEmpty(&UART)
        && uxQueueMessagesWaiting(xQueue_for_transmit) == 0) {
        XUartPs_WriteReg(XPAR_XUARTPS_0_BASEADDR, XUARTPS_FIFO_OFFSET, Data);
    } else if (xQueueSend(xQueue_for_transmit, &Data, 0UL) != pdPASS) {
        xil_printf("Fail to send the data\n");
    }
    taskEXIT_CRITICAL();
}

#endif /* SRC_UART_DRIVER_H_ */
