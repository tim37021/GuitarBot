/* Includes ------------------------------------------------------------------*/

//#include "FreeRTOSConfig.h"
#include "FreeRTOS.h"
#include "task.h"

#include "main.h"
#include "stm32f4xx_conf.h"
#include "usart.h"

#include "queue.h"
#include "semphr.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define TESTRESULT_ADDRESS         0x080FFFFC
#define ALLTEST_PASS               0x00000000
#define ALLTEST_FAIL               0x55555555

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

uint16_t PrescalerValue = 0;

__IO uint32_t TimingDelay;
__IO uint8_t UserButtonPressed = 0x00;

volatile xSemaphoreHandle serial_tx_wait_sem = NULL;
/* Add for serial input */
volatile xQueueHandle serial_rx_queue = NULL;


/* Private function prototypes -----------------------------------------------*/
static void UR_task(void *pvParameters);

/**
 * @brief  This function handles EXTI0_IRQ Handler.
 * @param  None
 * @retval None
 */
void EXTI0_IRQHandler(void)
{
	/* Clear the EXTI line pending bit */
	EXTI_ClearITPendingBit(USER_BUTTON_EXTI_LINE);
	if(UserButtonPressed){
		UserButtonPressed=0x00;
	}else
		UserButtonPressed=0x01;

}

/* IRQ handler to handle USART2 interruptss (both transmit and receive
 * interrupts). */
void USART2_IRQHandler()
{
	static signed portBASE_TYPE xHigherPriorityTaskWoken;

	/* If this interrupt is for a transmit... */
	if (USART_GetITStatus(USART2, USART_IT_TXE) != RESET) {
		/* "give" the serial_tx_wait_sem semaphore to notfiy processes
		 * that the buffer has a spot free for the next byte.
		 */
		xSemaphoreGiveFromISR(serial_tx_wait_sem, &xHigherPriorityTaskWoken);

		/* Diables the transmit interrupt. */
		USART_ITConfig(USART2, USART_IT_TXE, DISABLE);
		/* If this interrupt is for a receive... */
	}else if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET){
		char msg = USART_ReceiveData(USART2);

		/* If there is an error when queueing the received byte, freeze! */
		if(!xQueueSendToBackFromISR(serial_rx_queue, &msg, &xHigherPriorityTaskWoken))
			while(1);
	}
	else {
		/* Only transmit and receive interrupts should be enabled.
		 * If this is another type of interrupt, freeze.
		 */
		while(1);
	}

	if (xHigherPriorityTaskWoken) {
		taskYIELD();
	}
}

void send_byte(char ch)
{
	/* Wait until the RS232 port can receive another byte (this semaphore
	 * is "given" by the RS232 port interrupt when the buffer has room for
	 * another byte.
	 */
	while (!xSemaphoreTake(serial_tx_wait_sem, portMAX_DELAY));

	/* Send the byte and enable the transmit interrupt (it is disabled by
	 * the interrupt).
	 */
	USART_SendData(USART2, ch);
	USART_ITConfig(USART2, USART_IT_TXE, ENABLE);
}

char recv_byte()
{
	USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
	char msg;
	while(!xQueueReceive(serial_rx_queue, &msg, portMAX_DELAY));
	return msg;
}

int main(void)
{
	RCC_ClocksTypeDef RCC_Clocks;

	/* Configure SysTick */
	RCC_GetClocksFreq(&RCC_Clocks);
	SysTick_Config(RCC_Clocks.HCLK_Frequency / 100);

	FLASH_ProgramWord(TESTRESULT_ADDRESS, ALLTEST_PASS);
	SystemInit();
	//while(TimingDelay);


	/* Initialize LEDs and User_Button on STM32F4-Discovery --------------------*/
	STM_EVAL_PBInit(BUTTON_USER, BUTTON_MODE_EXTI);

	/* Initialize LEDs to be managed by GPIO */
	STM_EVAL_LEDInit(LED4);
	STM_EVAL_LEDInit(LED3);
	STM_EVAL_LEDInit(LED5);
	STM_EVAL_LEDInit(LED6);

	/* Turn OFF all LEDs */
	STM_EVAL_LEDOff(LED4);
	STM_EVAL_LEDOff(LED3);
	STM_EVAL_LEDOff(LED5);
	STM_EVAL_LEDOff(LED6);

	/* Reset UserButton_Pressed variable */
	UserButtonPressed = 0x00;

        /* Create the queue used by the serial task.  Messages for write to
         * the RS232. */
        vSemaphoreCreateBinary(serial_tx_wait_sem);
        /* Add for serial input 
         * Reference: www.freertos.org/a00116.html */
        serial_rx_queue = xQueueCreate(1, sizeof(char));


	/* Create a task to button check. */
	xTaskCreate(UR_task,
			(signed portCHAR *) "LCD Task",
			512 /* stack size */, NULL,
			tskIDLE_PRIORITY + 5, NULL);

	/* Start running the tasks. */
	vTaskStartScheduler();

	return 0;
}


void USART_puts(USART_TypeDef* USARTx, volatile char *s){

        while(*s){
                /*// wait until data register is empty
                while( !(USARTx->SR & 0x00000040) ); 

        USART_SendData(USARTx, *s);
                *s++;*/
		send_byte(*s++);
        }
}

static void UR_task(void *pvParameters)
{
	init_USART();
	
	char ch[]={"You input: \r\n"};
	while(1){
		//ch[10]=recv_byte();
		USART_puts(USART2, ch );
		vTaskDelay(100);
	}
}


/**
 * @brief  This function handles the test program fail.
 * @param  None
 * @retval None
 */
void Fail_Handler(void)
{
	/* Erase last sector */
	FLASH_EraseSector(FLASH_Sector_11, VoltageRange_3);
	/* Write FAIL code at last word in the flash memory */
	FLASH_ProgramWord(TESTRESULT_ADDRESS, ALLTEST_FAIL);

	while(1)
	{
		/* Toggle Red LED */
		STM_EVAL_LEDToggle(LED5);
		vTaskDelay(5);
	}
}

void TimingDelay_Decrement(void)
{
  if (TimingDelay != 0x00)
  {
    TimingDelay--;
  }
}

void vApplicationTickHook()
{
}
