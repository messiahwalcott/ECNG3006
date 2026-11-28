/*
 * ECNG 3006 - Lab 3, Question 2
 * Author: Ryan Seecharan
 * Student ID: 816033870
 * Public RTC interface - independent of the register or bus implementation.
 */
#ifndef LAB3_RTC_H
#define LAB3_RTC_H

#include <stdbool.h>
#include <stdint.h>

/* Full calendar year; the implementation supports 2000 through 2063. */
typedef struct
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} rtc_datetime_t;

/* Configure, start, and initialize the RTC to a known valid date/time. */
bool init_rtc(void);

/* Store one complete validated date/time record. False indicates failure. */
bool singlewrite_rtc(rtc_datetime_t date_time);

/* Retrieve one synchronized date/time record. Year 0 indicates failure. */
rtc_datetime_t singleread_rtc(void);

#endif
