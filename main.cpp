/*
 * File: main.cpp
 * Author: Messiah Walcott
 * ID Number: 816032825
 *
 * Course: ECNG 3006
 * Platform: Arduino MKR Zero
 * Microcontroller: ATSAMD21G18A
 * Processor: ARM Cortex-M0+
 * Compiler: GNU Arm Embedded GCC
 *
 * Description:
 * Adaptation of the Demonstration Cyclic Executive Code
 * presented in Rob Williams, Real-Time Systems Development,
 * Chapter 4, Section 4.7.
 *
 * TC3 provides a 10 ms hardware system tick. Its interrupt
 * service routine updates a software time structure used by
 * the replacement sleep and time functions.
 *
 * printf() is retargeted through SERCOM5 UART on MKR Zero
 * D14/TX (PB22). A 3.3 V USB-to-UART adapter allows the
 * serial output to be displayed using PuTTY.
 *
 * Arduino timing and serial APIs such as delay(), millis(),
 * Serial and SerialUSB are not used.
 */

#include <sam.h>
#include <stdint.h>
#include <stdio.h>


/*==========================================================
 * SYSTEM CONFIGURATION
 *==========================================================*/

#define CPU_CLOCK_HZ           (48000000UL)

#define SYSTEM_TICK_MS         (10UL)
#define SYSTEM_TICK_HZ         (100UL)

#define TIMER_PRESCALER        (64UL)

#define TIMER_COMPARE_VALUE    \
    ((CPU_CLOCK_HZ / TIMER_PRESCALER / SYSTEM_TICK_HZ) - 1UL)


/*==========================================================
 * CYCLIC EXECUTIVE CONFIGURATION
 *==========================================================*/

#define SLOT_COUNT             (4U)
#define CYCLE_COUNT            (5U)
#define SLOT_TIME_MS           (5000UL)


/*==========================================================
 * UART CONFIGURATION
 *==========================================================*/

#define UART_BAUD_RATE         (115200UL)

/*
 * MKR Zero D14/TX = PB22.
 * PB22 can be configured as SERCOM5 PAD[2].
 */
#define UART_TX_PIN            (22U)


/*==========================================================
 * TYPE DEFINITIONS
 *==========================================================*/

/*
 * Software time structure.
 *
 * volatile is required because the structure is modified
 * asynchronously by the TC3 interrupt service routine.
 */
typedef struct
{
    volatile uint32_t milliseconds;
    volatile uint32_t seconds;

} system_time_t;


/*
 * Function-pointer type used by the cyclic task table.
 */
typedef void (*task_function_t)(void);


/*==========================================================
 * GLOBAL VARIABLES
 *==========================================================*/

static volatile system_time_t system_time =
{
    0UL,
    0UL
};

static uint32_t slot_index = 0UL;
static uint32_t cycle_index = 0UL;
static uint32_t slot_start_ms = 0UL;


/*==========================================================
 * FRAMEWORK ENTRY POINTS
 *
 * The Arduino SAMD framework declares setup() and loop()
 * with C linkage. Since this file is compiled as C++, the
 * same linkage must be specified here.
 *==========================================================*/

extern "C" void setup(void);
extern "C" void loop(void);


/*==========================================================
 * INTERNAL FUNCTION PROTOTYPES
 *==========================================================*/

static void timer_init(void);

static void uart_init(void);
static void uart_write_char(char character);

static uint32_t time_ms(void);
static void sleep_seconds(uint32_t seconds);

static void task_one(void);
static void task_two(void);
static void task_three(void);
static void task_four(void);

/*
 * Williams defines Task 5 but does not place it in the
 * original task table. It is retained here as required.
 *
 * The GCC unused attribute prevents an unnecessary compiler
 * warning while preserving Williams' original structure.
 */
static void task_five(void) __attribute__((unused));

static void burn_task(void);


/*==========================================================
 * CYCLIC EXECUTIVE TASK TABLE
 *
 * This preserves Williams' original dispatch table.
 *
 * Task 5 is defined in Williams' demonstration but is not
 * scheduled in the table.
 *==========================================================*/

static task_function_t
task_table[SLOT_COUNT][CYCLE_COUNT] =
{
    {
        task_one,
        task_two,
        burn_task,
        burn_task,
        burn_task
    },

    {
        task_one,
        task_three,
        burn_task,
        burn_task,
        burn_task
    },

    {
        task_one,
        task_four,
        burn_task,
        burn_task,
        burn_task
    },

    {
        burn_task,
        burn_task,
        burn_task,
        burn_task,
        burn_task
    }
};


/*==========================================================
 * SETUP
 *
 * The Arduino SAMD framework supplies main(). The framework
 * calls this function after processor initialization.
 *
 * No Arduino serial or timing APIs are used.
 *==========================================================*/

extern "C" void setup(void)
{
    /*
     * Initialize the UART before attempting to print.
     */
    uart_init();

    /*
     * Initialize TC3 to generate a 10 ms system tick.
     */
    timer_init();

    /*
     * Disable stdout buffering so printf() output is sent
     * immediately through _write().
     */
    (void)setvbuf(stdout, NULL, _IONBF, 0);

    printf("\r\n");
    printf("SAMD21 Cyclic Executive\r\n");

    printf("System tick = %lu ms\r\n",
           (unsigned long)SYSTEM_TICK_MS);

    printf("UART baud rate = %lu\r\n\r\n",
           (unsigned long)UART_BAUD_RATE);

    /*
     * Record the reference time for the first slot.
     */
    slot_start_ms = time_ms();
}


/*==========================================================
 * CYCLIC DISPATCHER
 *
 * The Arduino framework repeatedly calls loop().
 *
 * This performs the same function as the infinite
 * dispatcher loop in Williams' demonstration.
 *==========================================================*/

extern "C" void loop(void)
{
    for (slot_index = 0UL;
         slot_index < SLOT_COUNT;
         slot_index++)
    {
        for (cycle_index = 0UL;
             cycle_index < CYCLE_COUNT;
             cycle_index++)
        {
            task_table[slot_index][cycle_index]();
        }
    }
}


/*==========================================================
 * TASK ONE
 *==========================================================*/

static void task_one(void)
{
    printf("task 1 running\r\n");

    sleep_seconds(1UL);
}


/*==========================================================
 * TASK TWO
 *==========================================================*/

static void task_two(void)
{
    printf("task 2 running\r\n");

    sleep_seconds(2UL);
}


/*==========================================================
 * TASK THREE
 *==========================================================*/

static void task_three(void)
{
    printf("task 3 running\r\n");

    sleep_seconds(3UL);
}


/*==========================================================
 * TASK FOUR
 *==========================================================*/

static void task_four(void)
{
    printf("task 4 running\r\n");

    sleep_seconds(4UL);
}


/*==========================================================
 * TASK FIVE
 *
 * Williams defines this task but does not schedule it in
 * the original cyclic task table.
 *==========================================================*/

static void task_five(void)
{
    printf("task 5 running\r\n");

    sleep_seconds(5UL);
}


/*==========================================================
 * BURN TASK
 *
 * Consumes the unused execution time remaining in the
 * current five-second scheduling slot.
 *==========================================================*/

static void burn_task(void)
{
    uint32_t burn_start_ms;
    uint32_t burn_time_ms;

    burn_start_ms = time_ms();

    /*
     * Wait until the current five-second slot has expired.
     *
     * TC3 interrupts remain enabled and therefore continue
     * updating system_time during this loop.
     */
    while ((time_ms() - slot_start_ms) < SLOT_TIME_MS)
    {
        /* Intentional idle/burn time. */
    }

    burn_time_ms = time_ms() - burn_start_ms;

    printf("burn time = %lu ms\r\n\r\n",
           (unsigned long)burn_time_ms);

    /*
     * Advance the reference by exactly one slot.
     *
     * This maintains the required periodic schedule rather
     * than accumulating scheduler execution time as drift.
     */
    slot_start_ms += SLOT_TIME_MS;

    /*
     * Williams sets the cycle counter to its limiting value
     * so the remainder of this table row is skipped.
     */
    cycle_index = CYCLE_COUNT;
}


/*==========================================================
 * SLEEP_SECONDS
 *
 * Replacement for the Linux sleep() function used in
 * Williams' original program.
 *
 * The delay is obtained from the TC3-generated system time
 * and does not use Arduino delay().
 *==========================================================*/

static void sleep_seconds(uint32_t seconds)
{
    uint32_t start_time_ms;
    uint32_t requested_delay_ms;

    requested_delay_ms = seconds * 1000UL;

    start_time_ms = time_ms();

    /*
     * Unsigned subtraction also permits normal uint32_t
     * timer wraparound without changing the comparison.
     */
    while ((time_ms() - start_time_ms) <
           requested_delay_ms)
    {
        /*
         * Busy wait.
         *
         * TC3 interrupts continue executing.
         */
    }
}


/*==========================================================
 * TIME_MS
 *
 * Replacement for the Linux times() functionality used
 * by Williams.
 *
 * Interrupts are briefly disabled to obtain one consistent
 * snapshot of the seconds and milliseconds fields.
 *==========================================================*/

static uint32_t time_ms(void)
{
    uint32_t seconds_snapshot;
    uint32_t milliseconds_snapshot;
    uint32_t previous_primask;

    /*
     * Store the current interrupt state.
     */
    previous_primask = __get_PRIMASK();

    __disable_irq();

    seconds_snapshot = system_time.seconds;
    milliseconds_snapshot = system_time.milliseconds;

    /*
     * Restore the previous interrupt state.
     */
    if (previous_primask == 0UL)
    {
        __enable_irq();
    }

    return ((seconds_snapshot * 1000UL) +
            milliseconds_snapshot);
}


/*==========================================================
 * TC3 INTERRUPT SERVICE ROUTINE
 *
 * One interrupt occurs every 10 ms.
 *==========================================================*/

extern "C" void TC3_Handler(void)
{
    /*
     * Check whether Match/Compare Channel 0 caused the
     * interrupt.
     */
    if ((TC3->COUNT16.INTFLAG.reg &
         TC_INTFLAG_MC0) != 0U)
    {
        /*
         * Clear the MC0 interrupt flag.
         */
        TC3->COUNT16.INTFLAG.reg = TC_INTFLAG_MC0;

        /*
         * Increment the software time structure by one
         * hardware timer tick.
         */
        system_time.milliseconds += SYSTEM_TICK_MS;

        if (system_time.milliseconds >= 1000UL)
        {
            system_time.milliseconds = 0UL;
            system_time.seconds++;
        }
    }
}


/*==========================================================
 * TC3 INITIALIZATION
 *
 * GCLK0 frequency = 48 MHz
 *
 * Timer clock:
 *
 *     48,000,000 / 64
 *         = 750,000 Hz
 *
 * Required system tick:
 *
 *     10 ms = 100 Hz
 *
 * Required counts:
 *
 *     750,000 / 100
 *         = 7,500 counts
 *
 * Therefore:
 *
 *     CC0 = 7,500 - 1
 *         = 7,499
 *==========================================================*/

static void timer_init(void)
{
    /*
     * Enable the APBC bus clock for TC3.
     */
    PM->APBCMASK.reg |= PM_APBCMASK_TC3;

    /*
     * Route Generic Clock Generator 0 to TC3.
     */
    GCLK->CLKCTRL.reg =
        GCLK_CLKCTRL_ID_TCC2_TC3 |
        GCLK_CLKCTRL_GEN_GCLK0 |
        GCLK_CLKCTRL_CLKEN;

    while (GCLK->STATUS.bit.SYNCBUSY != 0U)
    {
        /* Wait for GCLK synchronization. */
    }

    /*
     * Reset TC3 before configuration.
     */
    TC3->COUNT16.CTRLA.reg = TC_CTRLA_SWRST;

    while ((TC3->COUNT16.CTRLA.bit.SWRST != 0U) ||
           (TC3->COUNT16.STATUS.bit.SYNCBUSY != 0U))
    {
        /* Wait until reset is complete. */
    }

    /*
     * Configure:
     *
     * - 16-bit timer
     * - Match Frequency mode
     * - divide-by-64 prescaler
     */
    TC3->COUNT16.CTRLA.reg =
        TC_CTRLA_MODE_COUNT16 |
        TC_CTRLA_WAVEGEN_MFRQ |
        TC_CTRLA_PRESCALER_DIV64;

    while (TC3->COUNT16.STATUS.bit.SYNCBUSY != 0U)
    {
        /* Wait for synchronization. */
    }

    /*
     * Load the compare value that produces the 10 ms tick.
     */
    TC3->COUNT16.CC[0].reg =
        (uint16_t)TIMER_COMPARE_VALUE;

    while (TC3->COUNT16.STATUS.bit.SYNCBUSY != 0U)
    {
        /* Wait for synchronization. */
    }

    /*
     * Clear any stale Match/Compare interrupt.
     */
    TC3->COUNT16.INTFLAG.reg = TC_INTFLAG_MC0;

    /*
     * Enable the Match/Compare Channel 0 interrupt.
     */
    TC3->COUNT16.INTENSET.reg = TC_INTENSET_MC0;

    /*
     * Configure the Cortex-M0+ NVIC.
     */
    NVIC_ClearPendingIRQ(TC3_IRQn);
    NVIC_SetPriority(TC3_IRQn, 1U);
    NVIC_EnableIRQ(TC3_IRQn);

    /*
     * Start TC3.
     */
    TC3->COUNT16.CTRLA.bit.ENABLE = 1U;

    while (TC3->COUNT16.STATUS.bit.SYNCBUSY != 0U)
    {
        /* Wait for synchronization. */
    }
}


/*==========================================================
 * SERCOM5 UART INITIALIZATION
 *
 * Physical connection:
 *
 * MKR Zero D14/TX/PB22 ----> USB-UART RX
 * MKR Zero GND ------------> USB-UART GND
 *
 * UART settings:
 *
 * 115200 baud
 * 8 data bits
 * no parity
 * 1 stop bit
 * no flow control
 *==========================================================*/

static void uart_init(void)
{
    uint16_t baud_register;

    /*
     * Enable the APBC clock for SERCOM5.
     */
    PM->APBCMASK.reg |= PM_APBCMASK_SERCOM5;

    /*
     * Route GCLK0 to SERCOM5.
     */
    GCLK->CLKCTRL.reg =
        GCLK_CLKCTRL_ID_SERCOM5_CORE |
        GCLK_CLKCTRL_GEN_GCLK0 |
        GCLK_CLKCTRL_CLKEN;

    while (GCLK->STATUS.bit.SYNCBUSY != 0U)
    {
        /* Wait for GCLK synchronization. */
    }

    /*
     * Enable peripheral multiplexing on PB22.
     */
    PORT->Group[1].PINCFG[UART_TX_PIN].bit.PMUXEN = 1U;

    /*
     * PB22 is even, therefore PMUXE controls this pin.
     *
     * Peripheral Function D has the numeric value 3 and
     * connects PB22 to SERCOM5 PAD[2].
     */
    PORT->Group[1].PMUX[UART_TX_PIN / 2U].bit.PMUXE = 3U;

    /*
     * Reset SERCOM5 USART.
     */
    SERCOM5->USART.CTRLA.reg =
        SERCOM_USART_CTRLA_SWRST;

    while ((SERCOM5->USART.CTRLA.bit.SWRST != 0U) ||
           (SERCOM5->USART.SYNCBUSY.bit.SWRST != 0U))
    {
        /* Wait until reset is complete. */
    }

    /*
     * Configure:
     *
     * - internal-clock USART mode
     * - least-significant bit first
     * - TX on PAD[2]
     *
     * TXPO = 1 selects PAD[2] as the transmitter.
     */
    SERCOM5->USART.CTRLA.reg =
        SERCOM_USART_CTRLA_MODE_USART_INT_CLK |
        SERCOM_USART_CTRLA_TXPO(1U) |
        SERCOM_USART_CTRLA_DORD;

    /*
     * CHSIZE = 0 selects eight data bits.
     *
     * TXEN enables the USART transmitter.
     *
     * Default settings provide:
     * - no parity
     * - one stop bit
     */
    SERCOM5->USART.CTRLB.reg =
        SERCOM_USART_CTRLB_CHSIZE(0U) |
        SERCOM_USART_CTRLB_TXEN;

    while (SERCOM5->USART.SYNCBUSY.bit.CTRLB != 0U)
    {
        /* Wait for synchronization. */
    }

    /*
     * Calculate the asynchronous arithmetic baud register:
     *
     * BAUD =
     * 65536 x [1 - (16 x fBAUD / fREF)]
     *
     * 64-bit intermediate arithmetic prevents overflow.
     */
    baud_register =
        (uint16_t)
        (65536ULL -
         ((65536ULL *
           16ULL *
           UART_BAUD_RATE) /
          CPU_CLOCK_HZ));

    SERCOM5->USART.BAUD.reg = baud_register;

    /*
     * Enable SERCOM5 USART.
     */
    SERCOM5->USART.CTRLA.bit.ENABLE = 1U;

    while (SERCOM5->USART.SYNCBUSY.bit.ENABLE != 0U)
    {
        /* Wait for synchronization. */
    }
}


/*==========================================================
 * UART CHARACTER TRANSMISSION
 *==========================================================*/

static void uart_write_char(char character)
{
    /*
     * Wait until the transmit data register is empty.
     */
    while (SERCOM5->USART.INTFLAG.bit.DRE == 0U)
    {
        /* Wait for transmitter. */
    }

    SERCOM5->USART.DATA.reg =
        (uint16_t)(uint8_t)character;
}


/*==========================================================
 * PRINTF RETARGET
 *
 * Output path:
 *
 * printf()
 *      |
 *      v
 * <stdio.h> / C library
 *      |
 *      v
 * _write()
 *      |
 *      v
 * uart_write_char()
 *      |
 *      v
 * SERCOM5
 *      |
 *      v
 * D14/TX/PB22
 *      |
 *      v
 * USB-to-UART adapter
 *      |
 *      v
 * Windows COM port
 *      |
 *      v
 * PuTTY
 *==========================================================*/

extern "C"
int _write(int file,
           char *buffer,
           int length)
{
    int index;

    /*
     * stdout and stderr use the same UART in this program.
     */
    (void)file;

    if ((buffer == NULL) || (length <= 0))
    {
        return 0;
    }

    for (index = 0; index < length; index++)
    {
        uart_write_char(buffer[index]);
    }

    return length;
}