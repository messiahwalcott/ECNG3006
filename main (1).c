/*
 * ECNG 3006 - Lab 3, Question 2
 * Author: Ryan Seecharan
 * Student ID: 816033870
 * Repeated unit test: clock write, synchronized read, calendar progression.
 * D2 (PA10): PASS status; D3 (PA11): one toggle per verified calendar tick.
 * On-board LED (PB08): PASS status. On failure, PASS goes low and D3 stops.
 */

#include "rtc.h"
#include <sam.h>

#define PASS_MASK           (1ul << 10u)
#define TICK_MASK           (1ul << 11u)
#define LED_MASK            (1ul << 8u)
#define TICK_POLL_LIMIT     500u
#define CASE_LENGTH         4u

/* Independently specified expected results, not calculated by the driver. */
static const rtc_datetime_t test_cases[][CASE_LENGTH] =
{
    {
        {2026u, 11u, 26u, 12u, 34u, 58u},
        {2026u, 11u, 26u, 12u, 34u, 59u},
        {2026u, 11u, 26u, 12u, 35u,  0u},
        {2026u, 11u, 26u, 12u, 35u,  1u}
    },
    {
        {2024u,  2u, 28u, 23u, 59u, 58u},
        {2024u,  2u, 28u, 23u, 59u, 59u},
        {2024u,  2u, 29u,  0u,  0u,  0u},
        {2024u,  2u, 29u,  0u,  0u,  1u}
    },
    {
        {2026u, 12u, 31u, 23u, 59u, 58u},
        {2026u, 12u, 31u, 23u, 59u, 59u},
        {2027u,  1u,  1u,  0u,  0u,  0u},
        {2027u,  1u,  1u,  0u,  0u,  1u}
    }
};

/* Volatile diagnostics are available if a debugger is later connected.
 * 0: no failure; 1: init failure; 2: invalid write accepted;
 * 10+n: test case n failed (zero-based).
 */
volatile uint32_t test_failure_code = 0u;
volatile uint32_t completed_test_batches = 0u;

static void configure_test_pins(void)
{
    PM->APBBMASK.reg |= PM_APBBMASK_PORT;

    /* GPIO function, output latch initially low. */
    PORT->Group[0].PINCFG[10].reg = 0u;
    PORT->Group[0].PINCFG[11].reg = 0u;
    PORT->Group[1].PINCFG[8].reg = 0u;

    PORT->Group[0].OUTCLR.reg = PASS_MASK | TICK_MASK;
    PORT->Group[1].OUTCLR.reg = LED_MASK;
    PORT->Group[0].DIRSET.reg = PASS_MASK | TICK_MASK;
    PORT->Group[1].DIRSET.reg = LED_MASK;
}

static void report_pass(void)
{
    PORT->Group[0].OUTSET.reg = PASS_MASK;
    PORT->Group[1].OUTSET.reg = LED_MASK;
}

static void report_failure(uint32_t code)
{
    test_failure_code = code;
    PORT->Group[0].OUTCLR.reg = PASS_MASK | TICK_MASK;
    PORT->Group[1].OUTCLR.reg = LED_MASK;
    for (;;)
    {
        /* Latch failure until reset. Do not continue producing pass pulses. */
    }
}

static bool equal_datetime(rtc_datetime_t a, rtc_datetime_t b)
{
    return (a.year == b.year) && (a.month == b.month)
        && (a.day == b.day) && (a.hour == b.hour)
        && (a.minute == b.minute) && (a.second == b.second);
}

static bool verify_next_tick(rtc_datetime_t previous, rtc_datetime_t expected)
{
    for (uint32_t i = 0u; i < TICK_POLL_LIMIT; ++i)
    {
        const rtc_datetime_t actual = singleread_rtc();
        if (equal_datetime(actual, expected))
        {
            PORT->Group[0].OUTTGL.reg = TICK_MASK;
            return true;
        }
        if (!equal_datetime(actual, previous))
        {
            return false;
        }
    }
    return false;  /* Calendar failed to advance within poll limit. */
}

static bool verify_case(const rtc_datetime_t sequence[CASE_LENGTH])
{
    rtc_datetime_t actual;
    uint32_t next_index;

    if (!singlewrite_rtc(sequence[0]))
    {
        return false;
    }

    actual = singleread_rtc();

    /* Accept the first read at the written time, or the immediately
     * following second if a calendar tick coincides with the read.
     */
    if (equal_datetime(actual, sequence[0]))
    {
        next_index = 1u;
    }
    else if (equal_datetime(actual, sequence[1]))
    {
        PORT->Group[0].OUTTGL.reg = TICK_MASK;
        next_index = 2u;
    }
    else
    {
        return false;
    }

    for (; next_index < CASE_LENGTH; ++next_index)
    {
        if (!verify_next_tick(sequence[next_index - 1u], sequence[next_index]))
        {
            return false;
        }
    }
    return true;
}

/* C unit test called once by the minimal C++ framework entry point. */
void rtc_unit_test(void)
{
    const rtc_datetime_t invalid = {2025u, 2u, 29u, 0u, 0u, 0u};
    const uint32_t case_count = (uint32_t)(sizeof(test_cases)
                                        / sizeof(test_cases[0]));

    configure_test_pins();
    if (!init_rtc())
    {
        report_failure(1u);
    }

    for (;;)
    {
        /* 2025 is not a leap year. This write must be rejected. */
        if (singlewrite_rtc(invalid))
        {
            report_failure(2u);
        }

        for (uint32_t i = 0u; i < case_count; ++i)
        {
            if (!verify_case(test_cases[i]))
            {
                report_failure(10u + i);
            }
        }

        ++completed_test_batches;
        report_pass();  /* All tests passed. Leave PASS high unless one fails. */
    }
}
