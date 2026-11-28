/*
 * ECNG 3006 - Lab 3, Question 3
 * Author: Messiah Walcott
 * Student ID: 816032825
 * RTC routine execution-time test using GPIO pulse widths.
 * D2 / PA10 (CH2): singlewrite_rtc(); D3 / PA11 (CH1): singleread_rtc().
 * PB08: built-in LED, on after a verified iteration; off on failure.
 * Reuses the unchanged Question 2 RTC driver and C++ startup bridge.
 */

#include "rtc.h"
#include <sam.h>

/* Select 0 = ordinary date, 1 = valid leap date, 2 = invalid leap date.
 * Rebuild and upload after changing the case.
 */
#ifndef Q3_TEST_CASE
#define Q3_TEST_CASE 0
#endif

#if (Q3_TEST_CASE < 0) || (Q3_TEST_CASE > 2)
#error "Q3_TEST_CASE must be 0, 1 or 2."
#endif

#define WRITE_MASK (1ul << 10u)
#define READ_MASK  (1ul << 11u)
#define LED_MASK   (1ul << 8u)

static const rtc_datetime_t test_dates[3] =
{
    {2026u, 11u, 26u, 12u, 34u, 10u},
    {2024u,  2u, 29u, 23u, 15u, 20u},
    {2025u,  2u, 29u, 23u, 15u, 20u}
};

/* Available for inspection with a debugger, if one is connected. */
volatile uint32_t q3_completed_trials = 0u;
volatile uint32_t q3_failure_code = 0u;

static void configure_test_pins(void)
{
    PM->APBBMASK.reg |= PM_APBBMASK_PORT;

    PORT->Group[0].PINCFG[10].reg = 0u;
    PORT->Group[0].PINCFG[11].reg = 0u;
    PORT->Group[1].PINCFG[8].reg = 0u;

    PORT->Group[0].OUTCLR.reg = WRITE_MASK | READ_MASK;
    PORT->Group[1].OUTCLR.reg = LED_MASK;
    PORT->Group[0].DIRSET.reg = WRITE_MASK | READ_MASK;
    PORT->Group[1].DIRSET.reg = LED_MASK;
}

static void report_failure(uint32_t code)
{
    q3_failure_code = code;
    PORT->Group[0].OUTCLR.reg = WRITE_MASK | READ_MASK;
    PORT->Group[1].OUTCLR.reg = LED_MASK;
    for (;;)
    {
        /* Stop producing pulses until the board is reset. */
    }
}

/* These iterations only separate measurements and vary launch phase.
 * They are outside the measured intervals; they are not calibrated delays.
 */
static void gap(uint32_t iterations)
{
    while (iterations-- != 0u)
    {
        __NOP();
    }
}

static bool read_matches(rtc_datetime_t expected, rtc_datetime_t actual)
{
    /* Both successful test dates have seconds well below 59.
     * Accept the next second if a calendar tick overlaps the read.
     */
    return (actual.year == expected.year)
        && (actual.month == expected.month)
        && (actual.day == expected.day)
        && (actual.hour == expected.hour)
        && (actual.minute == expected.minute)
        && ((actual.second == expected.second)
            || (actual.second == (expected.second + 1u)));
}

/* Invoked by the unchanged Q2 entry.cpp instead of the Q2 main.c. */
void rtc_unit_test(void)
{
    const rtc_datetime_t input = test_dates[Q3_TEST_CASE];
    const bool expected_write = (Q3_TEST_CASE != 2);
    uint32_t trial = 0u;

    configure_test_pins();
}

