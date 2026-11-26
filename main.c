/*
 * ECNG 3006 - Lab 2 Question 2
 * Platform: Arduino MKR Zero
 * Processor: ATSAMD21G18A
 * RTOS: FreeRTOS_SAMD21
 *
 * Name: Messiah Walcott
 * ID: 816032825
 */

#include <stdint.h>
#include <sam.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/*----------------------------------------------------------*/
/* Application constants                                   */
/*----------------------------------------------------------*/

#define LED_PORT_GROUP          1U
#define LED_PIN_NUMBER          8U
#define LED_PIN_MASK            (1UL << LED_PIN_NUMBER)

#define UART_BAUD_RATE          115200UL
#define UART_REFERENCE_CLOCK    48000000UL
#define SERCOM5_GCLK_ID         0x19U

#define LED_ON_TIME_MS          500U
#define LED_OFF_DELAY_MS        1000U
#define UART_DELAY_MS           1000U

#define TASK_STACK_SIZE         200U


/*----------------------------------------------------------*/
/* Task priorities                                          */
/*----------------------------------------------------------*/

/*
 * Each task has its own priority level as required.
 */
static const int LED_ON_TASK_PRIORITY  = 3;
static const int LED_OFF_TASK_PRIORITY = 2;
static const int UART_TASK_PRIORITY    = 1;


/*----------------------------------------------------------*/
/* Mutex                                                    */
/*----------------------------------------------------------*/

/*
 * Mutex used to protect the shared built-in LED GPIO.
 */
static SemaphoreHandle_t xLedMutex = NULL;


/*----------------------------------------------------------*/
/* Function prototypes                                      */
/*----------------------------------------------------------*/

static void prvLedInit(void);
static void prvLedOn(void);
static void prvLedOff(void);

static void prvUartInit(void);
static void prvUartWriteChar(char character);
static void prvUartWriteString(const char *message);

static void prvActiveWaitMs(uint32_t delayMs);

static void vLedOnTask(void *pvParameters);
static void vLedOffTask(void *pvParameters);
static void vUartTask(void *pvParameters);

static void prvApplicationStart(void);


/*----------------------------------------------------------*/
/* Built-in LED - PB08                                      */
/*----------------------------------------------------------*/

static void prvLedInit(void)
{
    /*
     * Configure PB08 as an output.
     */
    PORT->Group[LED_PORT_GROUP].DIRSET.reg =
        LED_PIN_MASK;

    /*
     * LED initially OFF.
     */
    PORT->Group[LED_PORT_GROUP].OUTCLR.reg =
        LED_PIN_MASK;
}


static void prvLedOn(void)
{
    PORT->Group[LED_PORT_GROUP].OUTSET.reg =
        LED_PIN_MASK;
}


static void prvLedOff(void)
{
    PORT->Group[LED_PORT_GROUP].OUTCLR.reg =
        LED_PIN_MASK;
}


/*----------------------------------------------------------*/
/* Physical UART - SERCOM5                                  */
/* D14/PB22 = TX                                            */
/* D13/PB23 = RX                                            */
/*----------------------------------------------------------*/

static void prvUartInit(void)
{
    uint64_t baudValue;

    /*
     * Enable SERCOM5 peripheral clock.
     */
    PM->APBCMASK.reg |= PM_APBCMASK_SERCOM5;


    /*
     * Route Generic Clock Generator 0 to SERCOM5.
     */
    GCLK->CLKCTRL.reg =
        GCLK_CLKCTRL_ID(SERCOM5_GCLK_ID) |
        GCLK_CLKCTRL_GEN_GCLK0 |
        GCLK_CLKCTRL_CLKEN;


    while ((GCLK->STATUS.reg &
            GCLK_STATUS_SYNCBUSY) != 0U)
    {
        /*
         * Wait for clock synchronization.
         */
    }


    /*
     * PB22 = SERCOM5 PAD2 = TX
     * PB23 = SERCOM5 PAD3 = RX
     *
     * Peripheral function D selects SERCOM5.
     */
    PORT->Group[1].PINCFG[22].bit.PMUXEN = 1U;
    PORT->Group[1].PINCFG[23].bit.PMUXEN = 1U;

    PORT->Group[1].PMUX[11].bit.PMUXE =
        PORT_PMUX_PMUXE_D_Val;

    PORT->Group[1].PMUX[11].bit.PMUXO =
        PORT_PMUX_PMUXO_D_Val;


    /*
     * Reset SERCOM5 USART.
     */
    SERCOM5->USART.CTRLA.bit.SWRST = 1U;


    while ((SERCOM5->USART.CTRLA.bit.SWRST != 0U) ||
           (SERCOM5->USART.SYNCBUSY.bit.SWRST != 0U))
    {
        /*
         * Wait for reset to finish.
         */
    }


    /*
     * Configure USART:
     *
     * Internal clock
     * 16x oversampling
     * RX = PAD3
     * TX = PAD2
     * LSB first
     */
    SERCOM5->USART.CTRLA.reg =
        SERCOM_USART_CTRLA_MODE_USART_INT_CLK |
        SERCOM_USART_CTRLA_SAMPR(0U) |
        SERCOM_USART_CTRLA_RXPO(3U) |
        SERCOM_USART_CTRLA_TXPO(1U) |
        SERCOM_USART_CTRLA_DORD;


    /*
     * Configure:
     *
     * 8 data bits
     * No parity
     * One stop bit
     * TX enabled
     * RX enabled
     */
    SERCOM5->USART.CTRLB.reg =
        SERCOM_USART_CTRLB_CHSIZE(0U) |
        SERCOM_USART_CTRLB_TXEN |
        SERCOM_USART_CTRLB_RXEN;


    while (SERCOM5->USART.SYNCBUSY.bit.CTRLB != 0U)
    {
        /*
         * Wait for synchronization.
         */
    }


    /*
     * Baud-rate calculation for 16x oversampling.
     */
    baudValue =
        (65536ULL * 16ULL * UART_BAUD_RATE) /
        UART_REFERENCE_CLOCK;


    SERCOM5->USART.BAUD.reg =
        (uint16_t)(65536ULL - baudValue);


    /*
     * Enable SERCOM5 USART.
     */
    SERCOM5->USART.CTRLA.bit.ENABLE = 1U;


    while (SERCOM5->USART.SYNCBUSY.bit.ENABLE != 0U)
    {
        /*
         * Wait for synchronization.
         */
    }
}


/*----------------------------------------------------------*/
/* UART functions                                           */
/*----------------------------------------------------------*/

static void prvUartWriteChar(char character)
{
    /*
     * Wait until the transmit register is available.
     */
    while (SERCOM5->USART.INTFLAG.bit.DRE == 0U)
    {
    }


    SERCOM5->USART.DATA.reg =
        (uint16_t)(uint8_t)character;
}


static void prvUartWriteString(const char *message)
{
    while (*message != '\0')
    {
        prvUartWriteChar(*message);

        message++;
    }
}

static void prvUartWriteNumber(uint32_t value)
{
    char buffer[11];
    int index = 0;

    if (value == 0U)
    {
        prvUartWriteChar('0');
        return;
    }

    while ((value > 0U) && (index < 10))
    {
        buffer[index] =
            (char)('0' + (value % 10U));

        value /= 10U;
        index++;
    }

    while (index > 0)
    {
        index--;
        prvUartWriteChar(buffer[index]);
    }
}


static void prvLogEvent(const char *message)
{
    prvUartWriteString("[");
    prvUartWriteNumber(
        (uint32_t)xTaskGetTickCount());
    prvUartWriteString(" ms] ");

    prvUartWriteString(message);
    prvUartWriteString("\r\n");
}

/*----------------------------------------------------------*/
/* Active wait                                              */
/*----------------------------------------------------------*/

static void prvActiveWaitMs(uint32_t delayMs)
{
    const TickType_t xStartTime =
        xTaskGetTickCount();

    const TickType_t xDelayTicks =
        pdMS_TO_TICKS(delayMs);


    /*
     * Active/busy wait.
     *
     * vTaskDelay() is deliberately NOT used.
     */
    while ((xTaskGetTickCount() - xStartTime)
           < xDelayTicks)
    {
        __asm volatile ("nop");
    }
}


/*----------------------------------------------------------*/
/* Task 1 - LED ON                                          */
/*----------------------------------------------------------*/

static void vLedOnTask(void *pvParameters)
{
    (void)pvParameters;

    for (;;)
    {
        prvLogEvent(
            "T1 waiting for LED mutex");

        if (xSemaphoreTake(
                xLedMutex,
                portMAX_DELAY) == pdTRUE)
        {
            prvLogEvent(
                "T1 acquired mutex - LED ON");

            prvLedOn();

            prvActiveWaitMs(
                LED_ON_TIME_MS);

            prvLogEvent(
                "T1 finished 500 ms active wait");

            xSemaphoreGive(
                xLedMutex);

            prvLogEvent(
                "T1 released mutex - yielding");
        }

        taskYIELD();
    }
}


/*----------------------------------------------------------*/
/* Task 2 - LED OFF                                         */
/*----------------------------------------------------------*/

static void vLedOffTask(void *pvParameters)
{
    (void)pvParameters;

    for (;;)
    {
        prvLogEvent(
            "T2 waiting for LED mutex");

        if (xSemaphoreTake(
                xLedMutex,
                portMAX_DELAY) == pdTRUE)
        {
            prvLogEvent(
                "T2 acquired mutex - LED OFF");

            prvLedOff();

            prvLogEvent(
                "T2 blocked for 1000 ms");

            vTaskDelay(
                pdMS_TO_TICKS(
                    LED_OFF_DELAY_MS));

            xSemaphoreGive(
                xLedMutex);

            prvLogEvent(
                "T2 released mutex");
        }

        vTaskDelay(
            pdMS_TO_TICKS(1U));
    }
}

/*----------------------------------------------------------*/
/* Task 3 - UART status                                     */
/*----------------------------------------------------------*/
static void vUartTask(void *pvParameters)
{
    (void)pvParameters;

    for (;;)
    {
        prvLogEvent(
            "T3 status task running");

        vTaskDelay(
            pdMS_TO_TICKS(
                UART_DELAY_MS));
    }
}

/*----------------------------------------------------------*/
/* Application initialization                              */
/*----------------------------------------------------------*/

static void prvApplicationStart(void)
{
    BaseType_t xResult;


    /*
     * Initialise hardware.
     */
    prvLedInit();
    prvUartInit();


    /*
     * Create mutex protecting the LED GPIO.
     */
    xLedMutex =
        xSemaphoreCreateMutex();


    if (xLedMutex == NULL)
    {
        for (;;)
        {
            /*
             * Mutex creation failed.
             */
        }
    }


    /*
     * Create Task 1 - LED ON.
     */
    xResult = xTaskCreate(
        vLedOnTask,
        "LED_ON",
        TASK_STACK_SIZE,
        NULL,
        (UBaseType_t)
            LED_ON_TASK_PRIORITY,
        NULL
    );


    if (xResult != pdPASS)
    {
        for (;;)
        {
        }
    }


    /*
     * Create Task 2 - LED OFF.
     */
    xResult = xTaskCreate(
        vLedOffTask,
        "LED_OFF",
        TASK_STACK_SIZE,
        NULL,
        (UBaseType_t)
            LED_OFF_TASK_PRIORITY,
        NULL
    );


    if (xResult != pdPASS)
    {
        for (;;)
        {
        }
    }


    /*
     * Create Task 3 - UART.
     */
    xResult = xTaskCreate(
        vUartTask,
        "UART",
        TASK_STACK_SIZE,
        NULL,
        (UBaseType_t)
            UART_TASK_PRIORITY,
        NULL
    );


    if (xResult != pdPASS)
    {
        for (;;)
        {
        }
    }


    /*
     * Start the FreeRTOS scheduler.
     */
    vTaskStartScheduler();


    /*
     * Execution should never reach this point.
     */
    for (;;)
    {
    }
}


/*----------------------------------------------------------*/
/* Arduino framework entry functions                        */
/*----------------------------------------------------------*/

/*
 * The Arduino SAMD framework supplies the startup code,
 * but this application itself does not use Arduino.h.
 */

void setup(void)
{
    /*
     * Initialise the application and start FreeRTOS.
     */
    prvApplicationStart();
}


void loop(void)
{
    /*
     * Empty because all application functionality
     * is performed by FreeRTOS tasks.
     */
}