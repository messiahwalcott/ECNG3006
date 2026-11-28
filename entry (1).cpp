/*
 * ECNG 3006 - Lab 3, Question 2
 * Author: Ryan Seecharan
 * Student ID: 816033870
 */
extern "C" void rtc_unit_test(void);

extern "C" void setup(void)
{
    rtc_unit_test();
}

extern "C" void loop(void)
{
    /* The C unit test runs continuously and does not return. */
}
