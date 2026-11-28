/*
 * ECNG 3006 - Lab 3, Question 2
 * Author: Ryan Seecharan
 * Student ID: 816033870
 * SAMD21 RTC driver: XOSC32K -> GCLK4 /32 -> RTC /1024 -> 1 Hz.
 */

#include "rtc.h"
#include <sam.h>

#define RTC_BASE_YEAR       2000u
#define RTC_LAST_YEAR       2063u
#define RTC_GCLK_GENERATOR  4u
#define SYNC_WAIT_LIMIT     2000000u
#define OSC_WAIT_LIMIT      30000000u

static bool rtc_initialized = false;

/* Bounded polling prevents an infinite software wait if a clock fails.
 * Limits are iteration counts, not calibrated time measurements.
 */
static bool wait_rtc_sync(void)
{
    uint32_t remaining = SYNC_WAIT_LIMIT;
    while (RTC->MODE2.STATUS.bit.SYNCBUSY != 0u)
    {
        if (remaining-- == 0u)
        {
            return false;
        }
    }
    return true;
}

static bool wait_gclk_sync(void)
{
    uint32_t remaining = SYNC_WAIT_LIMIT;
    while (GCLK->STATUS.bit.SYNCBUSY != 0u)
    {
        if (remaining-- == 0u)
        {
            return false;
        }
    }
    return true;
}

static bool configure_rtc_clock(void)
{
    uint32_t remaining = OSC_WAIT_LIMIT;

    /* Keep the framework's existing XOSC32K configuration if compatible:
     * it also provides the reference for the main DFLL clock.
     */
    if (SYSCTRL->XOSC32K.bit.ENABLE == 0u)
    {
        SYSCTRL->XOSC32K.reg = SYSCTRL_XOSC32K_STARTUP(6u)
                            | SYSCTRL_XOSC32K_XTALEN
                            | SYSCTRL_XOSC32K_EN32K
                            | SYSCTRL_XOSC32K_RUNSTDBY;
        SYSCTRL->XOSC32K.bit.ENABLE = 1u;
    }
    if ((SYSCTRL->XOSC32K.bit.XTALEN == 0u)
        || (SYSCTRL->XOSC32K.bit.EN32K == 0u))
    {
        return false;
    }

    while (SYSCTRL->PCLKSR.bit.XOSC32KRDY == 0u)
    {
        if (remaining-- == 0u)
        {
            return false;
        }
    }

    if (!wait_gclk_sync())
    {
        return false;
    }

    /* CLKCTRL is indirectly addressed. An 8-bit write selects the RTC
     * channel without changing its generator or enable bits. Disable
     * the channel before changing its source (datasheet section 15.6.3).
     */
    *(volatile uint8_t *)&GCLK->CLKCTRL.reg = (uint8_t)RTC_GCLK_ID;
    if (GCLK->CLKCTRL.bit.WRTLOCK != 0u)
    {
        return false;
    }
    GCLK->CLKCTRL.bit.CLKEN = 0u;
    if (!wait_gclk_sync())
    {
        return false;
    }
    remaining = SYNC_WAIT_LIMIT;
    while (GCLK->CLKCTRL.bit.CLKEN != 0u)
    {
        if (remaining-- == 0u)
        {
            return false;
        }
    }

    /* This dedicated Q2 project owns GCLK4; do not reuse it for timers.
     * DIVSEL=1: division is 2^(DIV+1); DIV=4 therefore divides by 32.
     */
    GCLK->GENCTRL.reg = GCLK_GENCTRL_ID(RTC_GCLK_GENERATOR);
    if (!wait_gclk_sync())
    {
        return false;
    }

    GCLK->GENDIV.reg = GCLK_GENDIV_ID(RTC_GCLK_GENERATOR)
                     | GCLK_GENDIV_DIV(4u);
    if (!wait_gclk_sync())
    {
        return false;
    }

    GCLK->GENCTRL.reg = GCLK_GENCTRL_ID(RTC_GCLK_GENERATOR)
                      | GCLK_GENCTRL_SRC_XOSC32K
                      | GCLK_GENCTRL_DIVSEL
                      | GCLK_GENCTRL_RUNSTDBY
                      | GCLK_GENCTRL_GENEN;
    if (!wait_gclk_sync())
    {
        return false;
    }

    GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(RTC_GCLK_ID)
                      | GCLK_CLKCTRL_GEN_GCLK4
                      | GCLK_CLKCTRL_CLKEN;
    if (!wait_gclk_sync())
    {
        return false;
    }
    remaining = SYNC_WAIT_LIMIT;
    while (GCLK->CLKCTRL.bit.CLKEN == 0u)
    {
        if (remaining-- == 0u)
        {
            return false;
        }
    }
    return true;
}

/* Range 2000-2063 lies within a Gregorian century with no exception years. */
static bool valid_datetime(rtc_datetime_t value)
{
    static const uint8_t month_days[12] =
        {31u, 28u, 31u, 30u, 31u, 30u, 31u, 31u, 30u, 31u, 30u, 31u};
    uint8_t max_day;

    if ((value.year < RTC_BASE_YEAR) || (value.year > RTC_LAST_YEAR)
        || (value.month < 1u) || (value.month > 12u)
        || (value.hour > 23u) || (value.minute > 59u)
        || (value.second > 59u))
    {
        return false;
    }

    max_day = month_days[value.month - 1u];
    if ((value.month == 2u) && ((value.year % 4u) == 0u))
    {
        max_day = 29u;
    }
    return (value.day >= 1u) && (value.day <= max_day);
}

bool init_rtc(void)
{
    const rtc_datetime_t initial_time = {2000u, 1u, 1u, 0u, 0u, 0u};
    rtc_initialized = false;
    PM->APBAMASK.reg |= PM_APBAMASK_RTC;

    if (!configure_rtc_clock() || !wait_rtc_sync())
    {
        return false;
    }

    /* Mode and prescaler are enable-protected. Reset deliberately for Q2. */
    RTC->MODE2.CTRL.bit.ENABLE = 0u;
    if (!wait_rtc_sync())
    {
        return false;
    }

    RTC->MODE2.CTRL.reg = RTC_MODE2_CTRL_SWRST;
    if (!wait_rtc_sync() || (RTC->MODE2.CTRL.bit.SWRST != 0u))
    {
        return false;
    }

    /* Full register assignment selects 24-hour mode and MATCHCLR=0.
     * SWRST explicitly clears unused alarms, interrupts and correction.
     */
    RTC->MODE2.CTRL.reg = RTC_MODE2_CTRL_MODE_CLOCK
                        | RTC_MODE2_CTRL_PRESCALER_DIV1024;
    RTC->MODE2.EVCTRL.reg = 0u;
    RTC->MODE2.READREQ.reg = 0u;  /* On-demand read synchronization. */
    RTC->MODE2.CTRL.bit.ENABLE = 1u;

    if (!wait_rtc_sync())
    {
        return false;
    }

    rtc_initialized = true;
    if (!singlewrite_rtc(initial_time))
    {
        rtc_initialized = false;
        return false;
    }
    return true;
}

bool singlewrite_rtc(rtc_datetime_t date_time)
{
    uint32_t packed;

    if (!rtc_initialized || !valid_datetime(date_time) || !wait_rtc_sync())
    {
        return false;
    }

    /* The SAMD21 stores a year offset; the public API uses a full year. */
    packed = RTC_MODE2_CLOCK_YEAR(date_time.year - RTC_BASE_YEAR)
           | RTC_MODE2_CLOCK_MONTH(date_time.month)
           | RTC_MODE2_CLOCK_DAY(date_time.day)
           | RTC_MODE2_CLOCK_HOUR(date_time.hour)
           | RTC_MODE2_CLOCK_MINUTE(date_time.minute)
           | RTC_MODE2_CLOCK_SECOND(date_time.second);

    /* One 32-bit write prevents intermediate inconsistent calendar values. */
    RTC->MODE2.CLOCK.reg = packed;
    return wait_rtc_sync();
}

rtc_datetime_t singleread_rtc(void)
{
    const rtc_datetime_t invalid = {0u, 0u, 0u, 0u, 0u, 0u};
    rtc_datetime_t value;
    RTC_MODE2_CLOCK_Type snapshot;

    if (!rtc_initialized || !wait_rtc_sync())
    {
        return invalid;
    }

    /* Request a fresh snapshot, wait, then read the complete word once. */
    RTC->MODE2.READREQ.reg = RTC_READREQ_RREQ;
    if (!wait_rtc_sync())
    {
        return invalid;
    }
    snapshot.reg = RTC->MODE2.CLOCK.reg;

    value.year = (uint16_t)(RTC_BASE_YEAR + snapshot.bit.YEAR);
    value.month = (uint8_t)snapshot.bit.MONTH;
    value.day = (uint8_t)snapshot.bit.DAY;
    value.hour = (uint8_t)snapshot.bit.HOUR;
    value.minute = (uint8_t)snapshot.bit.MINUTE;
    value.second = (uint8_t)snapshot.bit.SECOND;

    if (!valid_datetime(value))
    {
        return invalid;
    }
    return value;
}
