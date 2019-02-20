/**
 * Copyright (c) 2018-2019 Sergey Burnevsky (sergey.burnevsky @ gmail.com)
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <new>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstddef>

#include "cron_calc.hpp"

/* ---------------------------------------------------------------------------- */

#ifdef CRON_CALC_NO_EXCEPT

static uint32_t g_new_fail_count = 0;

void* operator new  ( std::size_t count )
{
    if (g_new_fail_count > 0)
    {
        g_new_fail_count--;
        return NULL;
    }
    return malloc(count);
}

#endif

/* ---------------------------------------------------------------------------- */

int gNumErrors = 0;

#define CHECK_TRUE_LN(arg_, line_) { bool tmp = (arg_); if (!tmp) { \
    gNumErrors++; \
    printf("Line %3d: CHECK_TRUE: " # arg_ "\n", line_); \
    } }

#define CHECK_TRUE(arg_) CHECK_TRUE_LN(arg_, __LINE__)

#define CHECK_EQ_INT_LN(arg1_, arg2_, line_) { \
    uint32_t a1 = (arg1_); \
    uint32_t a2 = (arg2_); \
    if (a1 != a2) { \
        gNumErrors++; \
        printf("Line %3d: CHECK_EQ_INT: %u != %u <= (" # arg1_ " != " # arg2_ ")\n", line_, a1, a2); \
    } }

#define CHECK_EQ_INT(arg1_, arg2_) CHECK_EQ_INT_LN(arg1_, arg2_, __LINE__)

/* ---------------------------------------------------------------------------- */

static const char* TM_FORMAT = "%Y-%m-%d_%H:%M:%S";
static const char* TM_SCAN_FMT = "%u-%u-%u_%u:%u:%u";

/* ---------------------------------------------------------------------------- */

time_t parseTimeString(const char* tm_str)
{
    time_t t = CRON_CALC_INVALID_TIME;
    struct tm tm_val = { 0 };

    if (sscanf(tm_str, TM_SCAN_FMT,
        &tm_val.tm_year, &tm_val.tm_mon, &tm_val.tm_mday,
        &tm_val.tm_hour, &tm_val.tm_min, &tm_val.tm_sec) == 6)
    {
        CHECK_TRUE(tm_val.tm_year >= 1900);
        tm_val.tm_year -= 1900;
        CHECK_TRUE(tm_val.tm_mon >= 1);
        tm_val.tm_mon -= 1;
        tm_val.tm_isdst = -1;

        /* normalize to get other fields */
        t = mktime(&tm_val);
        if (t == CRON_CALC_INVALID_TIME)
        {
            printf("ERROR: Can't make time out of '%s'\n", tm_str);
        }
    }
    else
    {
        printf("ERROR: Can't parse time string '%s'\n", tm_str);
    }
    return t;
}

#define TS(str_) parseTimeString(str_)

/* ---------------------------------------------------------------------------- */

void print_test(const char* kind, const char* expr, cron_calc_option_mask options)
{
#if CRON_CALC_TEST_VERBOSE
    printf("Testing %s expression '%s' (%s) ...\n",
        kind, expr,
        CRON_CALC_OPT_WITH_YEARS ? "years" :
        CRON_CALC_OPT_WITH_SECONDS ? "seconds" :
        CRON_CALC_OPT_FULL ? "full" : "default");
#else
    (void) expr;
    (void) options;
#endif
}

/* ---------------------------------------------------------------------------- */

bool check_eq_time(time_t t1, time_t t2, const char* expr1, const char* expr2, int lineno)
{
    enum { TSTR_SIZE = 20 };
    char s1[TSTR_SIZE], s2[TSTR_SIZE];
    struct tm* stm;
    if (t1 != t2)
    {
        gNumErrors++;

        stm = localtime(&t1);
        strftime(s1, TSTR_SIZE, TM_FORMAT, stm);
        stm = localtime(&t2);
        strftime(s2, TSTR_SIZE, TM_FORMAT, stm);

        printf("Line %3d: CHECK_EQ_TIME: %ld != %ld ('%s' != '%s') <= ( %s != %s )\n",
            lineno, t1, t2, s1, s2, expr1, expr2);
        return false;
    }
    return true;
}

#define CHECK_EQ_TIME_LN(arg1_, arg2_, line_) check_eq_time((arg1_), (arg2_), # arg1_, # arg2_, line_)

#define CHECK_EQ_TIME(arg1_, arg2_) CHECK_EQ_TIME_LN(arg1_, arg2_, __LINE__)

/* ---------------------------------------------------------------------------- */

void check_invalid(
    const char* expr,
    cron_calc_option_mask options,
    cron_calc_error err,
    size_t err_offset,
    int lineno)
{
    CronCalc cron;
    const char* err_location = NULL;
    print_test("invalid", expr, options);
    CHECK_EQ_INT_LN(err, cron.addRule(expr, options, &err_location), lineno);
    CHECK_EQ_INT_LN(err_offset, err_location - expr, lineno);
}

#define CHECK_INVALID(expr_, opts_, err_, err_offset_) \
    check_invalid(expr_, opts_, err_, err_offset_, __LINE__)

/* ---------------------------------------------------------------------------- */

bool check_next(
    const char* expr,
    cron_calc_option_mask options,
    const char* initial,
    const char* next)
{
    int numErrors = gNumErrors;

    CronCalc cron;
    const char* err_location = NULL;
    cron_calc_error err = CRON_CALC_OK;

    print_test("valid", expr, options);

    err = cron.addRule(expr, options, &err_location);
    CHECK_EQ_INT(CRON_CALC_OK, err);
    CHECK_TRUE(NULL == err_location);
    if (NULL != err_location)
    {
        printf("ERROR at char %u: '%s'\n", (uint32_t) (err_location - expr), err_location);
    }

    if (err != CRON_CALC_OK)
    {
        return false;
    }

    time_t tinit = parseTimeString(initial);
    CHECK_TRUE(tinit != CRON_CALC_INVALID_TIME);

    while (next)
    {
        time_t tnext = CRON_CALC_INVALID_TIME;

        if (*next != '-')
        {
            tnext = parseTimeString(next);
            CHECK_TRUE(tnext != CRON_CALC_INVALID_TIME);
        }

        CHECK_EQ_TIME(tnext, cron.next(tinit));
        tinit = tnext;

        next = strchr(next, ',');
        if (next)
        {
            next++;
        }
    }

    return (numErrors == gNumErrors);
}

#define CHECK_NEXT(expr_, opts_, start_, next_) \
    CHECK_TRUE_LN(check_next(expr_, opts_, start_, next_), __LINE__)

/* ---------------------------------------------------------------------------- */

bool check_same(
    const char* expr1,
    cron_calc_option_mask options1,
    const char* expr2,
    cron_calc_option_mask options2)
{
    int numErrors = gNumErrors;

    cron_calc cc1, cc2;
    const char* err_location = NULL;
    cron_calc_error err = CRON_CALC_OK;
    bool same = false;

    print_test("1st", expr1, options1);
    print_test("2nd", expr2, options2);

    err = cron_calc_parse(&cc1, expr1, options1, &err_location);
    CHECK_EQ_INT(CRON_CALC_OK, err);
    CHECK_TRUE(NULL == err_location);
    if (NULL != err_location)
    {
        printf("ERROR1 at char %u: '%s'\n", (uint32_t)(err_location - expr1), err_location);
    }

    err = cron_calc_parse(&cc2, expr2, options2, &err_location);
    CHECK_EQ_INT(CRON_CALC_OK, err);
    CHECK_TRUE(NULL == err_location);
    if (NULL != err_location)
    {
        printf("ERROR2 at char %u: '%s'\n", (uint32_t)(err_location - expr2), err_location);
    }

    CHECK_TRUE(same = (cron_calc_is_same(&cc1, &cc2)));
    if (!same)
    {
        CHECK_EQ_INT(cc1.seconds, cc2.seconds);
        CHECK_EQ_INT(cc1.minutes, cc2.minutes);
        CHECK_EQ_INT(cc1.hours, cc2.hours);
        CHECK_EQ_INT(cc1.days, cc2.days);
        CHECK_EQ_INT(cc1.weekDays, cc2.weekDays);
        CHECK_EQ_INT(cc1.months, cc2.months);
        CHECK_EQ_INT(cc1.years, cc2.years);
        CHECK_EQ_INT(cc1.options, cc2.options);
    }

    return (numErrors == gNumErrors);
}

#define CHECK_SAME(expr1_, opts1_, expr2_, opts2_) \
    CHECK_TRUE_LN(check_same(expr1_, opts1_, expr2_, opts2_), __LINE__)

/* ---------------------------------------------------------------------------- */

int main()
{
    /* bad invocation */
    cron_calc cc = { 0 }, cc2 = { 0 };
    const char* err_location = NULL;
    CHECK_EQ_INT(cron_calc_parse(NULL, "* * * * *", CRON_CALC_OPT_DEFAULT, &err_location), CRON_CALC_ERROR_ARGUMENT);
    CHECK_EQ_INT(cron_calc_parse(&cc, NULL, CRON_CALC_OPT_DEFAULT, &err_location), CRON_CALC_ERROR_ARGUMENT);
    CHECK_EQ_INT(cron_calc_next(NULL, 1549747649), CRON_CALC_INVALID_TIME);

    cc = (cron_calc) { 1, 1, 1, 1, 1, 1, 1, 1 };
    cc2 = cc;
    cc.seconds = 0;
    CHECK_EQ_INT(cron_calc_next(&cc, 1549747649), CRON_CALC_INVALID_TIME);
    CHECK_TRUE(!cron_calc_is_same(&cc, &cc2));
    cc.minutes = 0; cc.seconds = 1;
    CHECK_EQ_INT(cron_calc_next(&cc, 1549747649), CRON_CALC_INVALID_TIME);
    CHECK_TRUE(!cron_calc_is_same(&cc, &cc2));
    cc.hours = 0; cc.minutes = 1;
    CHECK_EQ_INT(cron_calc_next(&cc, 1549747649), CRON_CALC_INVALID_TIME);
    CHECK_TRUE(!cron_calc_is_same(&cc, &cc2));
    cc.weekDays = 0; cc.hours = 1;
    CHECK_EQ_INT(cron_calc_next(&cc, 1549747649), CRON_CALC_INVALID_TIME);
    CHECK_TRUE(!cron_calc_is_same(&cc, &cc2));
    cc.days = 0; cc.weekDays = 1;
    CHECK_EQ_INT(cron_calc_next(&cc, 1549747649), CRON_CALC_INVALID_TIME);
    CHECK_TRUE(!cron_calc_is_same(&cc, &cc2));
    cc.months = 0; cc.days = 1;
    CHECK_EQ_INT(cron_calc_next(&cc, 1549747649), CRON_CALC_INVALID_TIME);
    CHECK_TRUE(!cron_calc_is_same(&cc, &cc2));
    cc.years = 0; cc.months = 1; cc.options = CRON_CALC_OPT_WITH_YEARS;
    CHECK_EQ_INT(cron_calc_next(&cc, 1549747649), CRON_CALC_INVALID_TIME);
    CHECK_TRUE(!cron_calc_is_same(&cc, &cc2));
    cc.options = 0; cc.years = 1;
    CHECK_EQ_INT(cron_calc_next(&cc, 1549747649), CRON_CALC_INVALID_TIME);
    CHECK_TRUE(!cron_calc_is_same(&cc, &cc2));
    cc.options = 1;
    CHECK_TRUE(cron_calc_is_same(&cc, &cc2));

    if (sizeof(time_t) > sizeof(int))
    {
        CHECK_EQ_INT(cron_calc_next(&cc, 2L<<56), CRON_CALC_INVALID_TIME);
    }
    CHECK_EQ_INT(cron_calc_next(&cc, -2), CRON_CALC_INVALID_TIME);

#ifdef CRON_CALC_NO_EXCEPT
    /*  */
    g_new_fail_count = 100;
    CronCalc cron1_fail;
    CHECK_EQ_INT(CRON_CALC_ERROR_USAGE, cron1_fail.addRule("* * * * *", CRON_CALC_OPT_DEFAULT, &err_location));
    CHECK_EQ_INT(CRON_CALC_INVALID_TIME, cron1_fail.next(1549747649));
    g_new_fail_count = 0;
#endif

    /* bad format */
    CHECK_EQ_INT(cron_calc_parse(&cc, " * * * * *", CRON_CALC_OPT_DEFAULT, NULL), CRON_CALC_ERROR_NUMBER_EXPECTED);
    CHECK_INVALID(" * * * * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_EXPECTED, 0);
    CHECK_INVALID("a * * * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_EXPECTED, 0);
    CHECK_INVALID("ab * * * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_EXPECTED, 0);
    CHECK_INVALID("** * * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_FIELD_FORMAT, 1);
    CHECK_INVALID("*/* * * * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_EXPECTED, 2);
    CHECK_INVALID("*/a * * * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_EXPECTED, 2);
    CHECK_INVALID("2+2 * * * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_FIELD_FORMAT, 1);
    CHECK_INVALID("2-- * * * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_EXPECTED, 2);
    CHECK_INVALID("-2 * * * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_EXPECTED, 0);
    CHECK_INVALID("2-a * * * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_EXPECTED, 2);
    CHECK_INVALID("/2 * * * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_EXPECTED, 0);
    CHECK_INVALID("* * * 2-? *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_EXPECTED, 8);
    CHECK_INVALID("* * * 2-a *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_INVALID_NAME, 8);
    CHECK_INVALID("* * * 2-Z *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_INVALID_NAME, 8);
    CHECK_INVALID("* * * + *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_EXPECTED, 6);
    CHECK_INVALID("* * * b *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_INVALID_NAME, 6);
    CHECK_INVALID("* * * * a", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_INVALID_NAME, 8);
    CHECK_INVALID("* * * * Z", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_INVALID_NAME, 8);
    CHECK_INVALID("* * * * ?", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_EXPECTED, 8);
    CHECK_INVALID("* 2,a * * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_EXPECTED, 4);

    /* number ranges */
    CHECK_INVALID("60 * * * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_RANGE, 0);
    CHECK_INVALID("0-60 * * * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_RANGE, 2);
    CHECK_INVALID("59,60 * * * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_RANGE, 3);
    CHECK_INVALID("59 24 * * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_RANGE, 3);
    CHECK_INVALID("59 0-24 * * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_RANGE, 5);
    CHECK_INVALID("59 23,24 * * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_RANGE, 6);
    CHECK_INVALID("59 23 0 * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_RANGE, 6);
    CHECK_INVALID("59 23 32 * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_RANGE, 6);
    CHECK_INVALID("59 23 0-31 * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_RANGE, 6);
    CHECK_INVALID("59 23 1-32 * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_RANGE, 8);
    CHECK_INVALID("59 23 31,32 * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_RANGE, 9);
    CHECK_INVALID("59 23 31 0 *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_RANGE, 9);
    CHECK_INVALID("59 23 31 13 *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_RANGE, 9);
    CHECK_INVALID("59 23 31 0-12 *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_RANGE, 9);
    CHECK_INVALID("59 23 31 1-13 *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_RANGE, 11);
    CHECK_INVALID("59 23 31 12,13 *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_RANGE, 12);
    CHECK_INVALID("59 23 31 12 8", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_RANGE, 12);
    CHECK_INVALID("59 23 31 12 0-8", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_RANGE, 14);
    CHECK_INVALID("60 59 23 31 12 6", CRON_CALC_OPT_WITH_SECONDS, CRON_CALC_ERROR_NUMBER_RANGE, 0);
    CHECK_INVALID("0-60 59 23 31 12 6", CRON_CALC_OPT_WITH_SECONDS, CRON_CALC_ERROR_NUMBER_RANGE, 2);
    CHECK_INVALID("59,60 59 23 31 12 6", CRON_CALC_OPT_WITH_SECONDS, CRON_CALC_ERROR_NUMBER_RANGE, 3);
    CHECK_INVALID("59 23 31 12 6 1999", CRON_CALC_OPT_WITH_YEARS, CRON_CALC_ERROR_NUMBER_RANGE, 14);
    CHECK_INVALID("59 23 31 12 6 2000,1999", CRON_CALC_OPT_WITH_YEARS, CRON_CALC_ERROR_NUMBER_RANGE, 19);
    CHECK_INVALID("59 23 31 12 6 1999-2063", CRON_CALC_OPT_WITH_YEARS, CRON_CALC_ERROR_NUMBER_RANGE, 14);
    CHECK_INVALID("59 23 31 12 6 2000-2063,1999", CRON_CALC_OPT_WITH_YEARS, CRON_CALC_ERROR_NUMBER_RANGE, 24);
    CHECK_INVALID("59 23 31 12 6 2064", CRON_CALC_OPT_WITH_YEARS, CRON_CALC_ERROR_NUMBER_RANGE, 14);
    CHECK_INVALID("59 23 31 12 6 2000,2064", CRON_CALC_OPT_WITH_YEARS, CRON_CALC_ERROR_NUMBER_RANGE, 19);
    CHECK_INVALID("59 23 31 12 6 2000-2064", CRON_CALC_OPT_WITH_YEARS, CRON_CALC_ERROR_NUMBER_RANGE, 19);
    CHECK_INVALID("59 59 23 31 12 6 1999", CRON_CALC_OPT_FULL, CRON_CALC_ERROR_NUMBER_RANGE, 17);
    CHECK_INVALID("59 59 23 31 12 6 1999-2063", CRON_CALC_OPT_FULL, CRON_CALC_ERROR_NUMBER_RANGE, 17);
    CHECK_INVALID("59 59 23 31 12 6 2064", CRON_CALC_OPT_FULL, CRON_CALC_ERROR_NUMBER_RANGE, 17);
    CHECK_INVALID("59 59 23 31 12 6 2000-2064", CRON_CALC_OPT_FULL, CRON_CALC_ERROR_NUMBER_RANGE, 22);
    CHECK_INVALID("59-58 * * * * * *", CRON_CALC_OPT_FULL, CRON_CALC_ERROR_NUMBER_RANGE, 5); /* after wrong number is parsed */
    CHECK_INVALID("59-0 * * * * * *", CRON_CALC_OPT_FULL, CRON_CALC_ERROR_NUMBER_RANGE, 4);
    CHECK_INVALID("* 59-58 * * * * *", CRON_CALC_OPT_FULL, CRON_CALC_ERROR_NUMBER_RANGE, 7);
    CHECK_INVALID("* * 23-1 * * * *", CRON_CALC_OPT_FULL, CRON_CALC_ERROR_NUMBER_RANGE, 8);
    CHECK_INVALID("* * * 30-1 * * *", CRON_CALC_OPT_FULL, CRON_CALC_ERROR_NUMBER_RANGE, 10);
    CHECK_INVALID("* * * * 10-4 * *", CRON_CALC_OPT_FULL, CRON_CALC_ERROR_NUMBER_RANGE, 12);
    CHECK_INVALID("* * * * OCT-APR * *", CRON_CALC_OPT_FULL, CRON_CALC_ERROR_NUMBER_RANGE, 15);
    CHECK_INVALID("* * * * * 6-0 *", CRON_CALC_OPT_FULL, CRON_CALC_ERROR_NUMBER_RANGE, 13);
    CHECK_INVALID("* * * * * SAT-SUN *", CRON_CALC_OPT_FULL, CRON_CALC_ERROR_NUMBER_RANGE, 17);
    CHECK_INVALID("* * * * * * 2019-2018", CRON_CALC_OPT_FULL, CRON_CALC_ERROR_NUMBER_RANGE, 21);

    /* steps */
    CHECK_INVALID("*/0 * * * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_RANGE, 2);
    CHECK_INVALID("5-50/0 * * * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_RANGE, 5);

    CHECK_INVALID("0/2 * * * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_FIELD_FORMAT, 1);
    CHECK_INVALID("* 1/2 * * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_FIELD_FORMAT, 3);
    CHECK_INVALID("* * 2/2 * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_FIELD_FORMAT, 5);
    CHECK_INVALID("* * * 8/2 *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_FIELD_FORMAT, 7);
    CHECK_INVALID("* * * AUG/2 *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_FIELD_FORMAT, 9);
    CHECK_INVALID("* * * * 2/2", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_FIELD_FORMAT, 9);
    CHECK_INVALID("* * * * TUE/2", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_FIELD_FORMAT, 11);
    CHECK_INVALID("* * * * * 2010/2", CRON_CALC_OPT_WITH_YEARS, CRON_CALC_ERROR_FIELD_FORMAT, 14);

    /* names */
    CHECK_INVALID("* * * M", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_INVALID_NAME, 6);
    CHECK_INVALID("* * * M *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_INVALID_NAME, 6);
    CHECK_INVALID("* * * MA *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_INVALID_NAME, 6);
    CHECK_INVALID("* * * MAN *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_INVALID_NAME, 6);
    CHECK_INVALID("* * * @AN *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_EXPECTED, 6);
    CHECK_INVALID("* * * [AN *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_EXPECTED, 6);
    CHECK_INVALID("* * * {AN *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_EXPECTED, 6);
    CHECK_INVALID("* * * MA@ *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_INVALID_NAME, 6);
    CHECK_INVALID("* * * MA[ *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_INVALID_NAME, 6);
    CHECK_INVALID("* * * Ma{ *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_INVALID_NAME, 6);
    CHECK_INVALID("* * * MAY,B *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_INVALID_NAME, 10);
    CHECK_INVALID("* * * MAY,aB *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_INVALID_NAME, 10);
    CHECK_INVALID("* * * MAY,ApP *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_INVALID_NAME, 10);
    CHECK_INVALID("* * * MAY,ABr *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_INVALID_NAME, 10);
    CHECK_INVALID("* * * MAY M", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_INVALID_NAME, 10);
    CHECK_INVALID("* * * MAY MO", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_INVALID_NAME, 10);
    CHECK_INVALID("* * * MAY MOY", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_INVALID_NAME, 10);
    CHECK_INVALID("* * * MAY-AUF MOY", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_INVALID_NAME, 10);
    CHECK_INVALID("* * * MAY-AUG MON-TUF", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_INVALID_NAME, 18);
    CHECK_INVALID("* * * MAY-AUG,JAP *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_INVALID_NAME, 14);
    CHECK_INVALID("* * * MAY-AUG,JAN-DEE *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_INVALID_NAME, 18);
    CHECK_INVALID("* * * MON *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_INVALID_NAME, 6);
    CHECK_INVALID("* * * MAY-FRI *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_INVALID_NAME, 10);
    CHECK_INVALID("* * * AUG AUG", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_INVALID_NAME, 10);

    /* last is only allowed for days */
    CHECK_INVALID("L * * * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_EXPECTED, 0);
    CHECK_INVALID("L * * * * *", CRON_CALC_OPT_WITH_SECONDS, CRON_CALC_ERROR_NUMBER_EXPECTED, 0);
    CHECK_INVALID("* L * * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_EXPECTED, 2);
    CHECK_INVALID("* * * L *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_EXPECTED, 6);
    CHECK_INVALID("* * * * L", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_EXPECTED, 8);
    CHECK_INVALID("* * * * * L", CRON_CALC_OPT_WITH_YEARS, CRON_CALC_ERROR_NUMBER_EXPECTED, 10);
    /* last cannot be used in ranges */
    CHECK_INVALID("* * 3L * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_FIELD_FORMAT, 5);
    CHECK_INVALID("* * 3-L * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_EXPECTED, 6);
    CHECK_INVALID("* * 2-5/L * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_EXPECTED, 8);
    CHECK_INVALID("* * L-L * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_FIELD_FORMAT, 5);
    CHECK_INVALID("* * L/2 * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_FIELD_FORMAT, 5);

    /* short */
    CHECK_INVALID("* * * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_EXPR_SHORT, 7);
    CHECK_INVALID("* * *  *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_EXPR_SHORT, 8);
    CHECK_INVALID("0 1 2 3", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_EXPR_SHORT, 7);

    CHECK_INVALID("* * * * *", CRON_CALC_OPT_WITH_SECONDS, CRON_CALC_ERROR_EXPR_SHORT, 9);
    CHECK_INVALID("0 1 2 3 4", CRON_CALC_OPT_FULL, CRON_CALC_ERROR_EXPR_SHORT, 9);
    CHECK_INVALID("* * * * *", CRON_CALC_OPT_WITH_YEARS, CRON_CALC_ERROR_EXPR_SHORT, 9);
    CHECK_INVALID("* * * * *", CRON_CALC_OPT_FULL, CRON_CALC_ERROR_EXPR_SHORT, 9);
    CHECK_INVALID("* * * * * *", CRON_CALC_OPT_FULL, CRON_CALC_ERROR_EXPR_SHORT, 11);
    CHECK_INVALID("0 1 2 3 4 5", CRON_CALC_OPT_FULL, CRON_CALC_ERROR_EXPR_SHORT, 11);

    /* long */
    CHECK_INVALID("* * * * * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_EXPR_LONG, 10);
    CHECK_INVALID("* * * * * * *", CRON_CALC_OPT_WITH_SECONDS, CRON_CALC_ERROR_EXPR_LONG, 12);
    CHECK_INVALID("* * * * * * *", CRON_CALC_OPT_WITH_YEARS, CRON_CALC_ERROR_EXPR_LONG, 12);
    CHECK_INVALID("* * * * * * * *", CRON_CALC_OPT_FULL, CRON_CALC_ERROR_EXPR_LONG, 14);

    /* impossible */
    CHECK_INVALID("* * 30 FEB *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_IMPOSSIBLE_DATE, 12);
    CHECK_INVALID("* * 31 FeB *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_IMPOSSIBLE_DATE, 12);
    CHECK_INVALID("* * 31 APr *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_IMPOSSIBLE_DATE, 12);
    CHECK_INVALID("* * 31 jUN *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_IMPOSSIBLE_DATE, 12);
    CHECK_INVALID("* * 31 SEP *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_IMPOSSIBLE_DATE, 12);
    CHECK_INVALID("* * 31 11 *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_IMPOSSIBLE_DATE, 11);
    CHECK_INVALID("* * 29 FEB * 2001", CRON_CALC_OPT_WITH_YEARS, CRON_CALC_ERROR_IMPOSSIBLE_DATE, 17);
    CHECK_INVALID("* * 29 FEB * 2001-2003", CRON_CALC_OPT_WITH_YEARS, CRON_CALC_ERROR_IMPOSSIBLE_DATE, 22);

    /* Next */

    CHECK_NEXT("* * * * *", CRON_CALC_OPT_DEFAULT,
        "2018-12-31_23:58:00",
        "2018-12-31_23:59:00,"
        "2019-01-01_00:00:00," /* !!! champagne time !!! */
        "2019-01-01_00:01:00");

    CHECK_NEXT("* * * * * *", CRON_CALC_OPT_WITH_SECONDS,
        "2018-12-31_23:59:58",
        "2018-12-31_23:59:59,"
        "2019-01-01_00:00:00," /* !!! champagne time !!! */
        "2019-01-01_00:00:01");

    CHECK_NEXT("*/2 * * * *", CRON_CALC_OPT_DEFAULT,
        "2018-12-31_23:57:00",
        "2018-12-31_23:58:00,"
        "2019-01-01_00:00:00,"
        "2019-01-01_00:02:00");

    CHECK_NEXT("0 */2 * * *", CRON_CALC_OPT_DEFAULT,
        "2018-12-31_21:57:00",
        "2018-12-31_22:00:00,"
        "2019-01-01_00:00:00,"
        "2019-01-01_02:00:00");

    /* this means 'every odd date' or 'every 2nd day of month', but NOT 'every two days' */
    CHECK_NEXT("1 2 */2 * *", CRON_CALC_OPT_DEFAULT,
        "2018-12-29_21:57:00",
        "2018-12-31_02:01:00,"
        "2019-01-01_02:01:00,"
        "2019-01-03_02:01:00");

    CHECK_NEXT("1 2 1-31/2 * *", CRON_CALC_OPT_DEFAULT,
        "2018-12-29_21:57:00",
        "2018-12-31_02:01:00,"
        "2019-01-01_02:01:00,"
        "2019-01-03_02:01:00");

    /* Even days */
    CHECK_NEXT("1 2 2-31/2 * *", CRON_CALC_OPT_DEFAULT,
        "2018-12-29_21:57:00",
        "2018-12-30_02:01:00,"
        "2019-01-02_02:01:00,"
        "2019-01-04_02:01:00");

    /* Both day fields restricted -> match on either of them */
    CHECK_NEXT("1 2 28-31 * 5", CRON_CALC_OPT_DEFAULT,
        "2019-01-23_00:00:00",
        "2019-01-25_02:01:00,"  /* FRI */
        "2019-01-28_02:01:00,"
        "2019-01-29_02:01:00");

    /* Month day unrestricted -> match on both of them */
    CHECK_NEXT("1 2 * * 5", CRON_CALC_OPT_DEFAULT,
        "2019-01-23_00:00:00",
        "2019-01-25_02:01:00,"  /* FRI */
        "2019-02-01_02:01:00,"  /* FRI */
        "2019-02-08_02:01:00"); /* FRI */

    /* Week day unrestricted -> match on both of them */
    CHECK_NEXT("1 2 2-24 * *", CRON_CALC_OPT_DEFAULT,
        "2019-01-24_00:00:00",
        "2019-01-24_02:01:00,"
        "2019-02-02_02:01:00,"
        "2019-02-03_02:01:00");

    /* 31st of a month */
    CHECK_NEXT("1 0 31 * *", CRON_CALC_OPT_DEFAULT,
        "2020-01-01_00:00:00",
        "2020-01-31_00:01:00,"
        "2020-03-31_00:01:00,"
        "2020-05-31_00:01:00,"
        "2020-07-31_00:01:00,"
        "2020-08-31_00:01:00,"
        "2020-10-31_00:01:00,"
        "2020-12-31_00:01:00");

    /* 30th of a month */
    CHECK_NEXT("0 12 30 * *", CRON_CALC_OPT_DEFAULT,
        "2020-01-01_00:00:00",
        "2020-01-30_12:00:00,"
        "2020-03-30_12:00:00,2020-04-30_12:00:00,2020-05-30_12:00:00,"
        "2020-06-30_12:00:00,2020-07-30_12:00:00,2020-08-30_12:00:00,"
        "2020-09-30_12:00:00,2020-10-30_12:00:00,2020-11-30_12:00:00,"
        "2020-12-30_12:00:00");

    /* 29th of a month */
    CHECK_NEXT("0 0 29 * *", CRON_CALC_OPT_DEFAULT,
        "2019-01-01_00:00:00",
        "2019-01-29_00:00:00,"
        "2019-03-29_00:00:00,"
        "2019-04-29_00:00:00,2019-05-29_00:00:00,2019-06-29_00:00:00,"
        "2019-07-29_00:00:00,2019-08-29_00:00:00,2019-09-29_00:00:00,"
        "2019-10-29_00:00:00,2019-11-29_00:00:00,2019-12-29_00:00:00,"
        "2020-01-29_00:00:00,"
        "2020-02-29_00:00:00,"
        "2020-03-29_00:00:00");
    CHECK_NEXT("1 1 29 3 * *", CRON_CALC_OPT_WITH_YEARS,
        "2019-01-01_00:00:00",
        "2019-03-29_01:01:00");
    CHECK_NEXT("1 1 28 2 * *", CRON_CALC_OPT_WITH_YEARS,
        "2019-01-01_00:00:00",
        "2019-02-28_01:01:00");

    /* 2000 was also a leap year (div 400) */
    CHECK_NEXT("0 0 29 FEB *", CRON_CALC_OPT_DEFAULT,
        "1999-01-01_00:00:00",
        "2000-02-29_00:00:00,"
        "2004-02-29_00:00:00,"
        "2008-02-29_00:00:00,"
        "2012-02-29_00:00:00");

    /* 2100 is not a leap year (div 100) */
    CHECK_NEXT("0 0 29 FEB *", CRON_CALC_OPT_DEFAULT,
        "2096-01-01_00:00:00",
        "2096-02-29_00:00:00,"
        "2104-02-29_00:00:00,"
        "2108-02-29_00:00:00,"
        "2112-02-29_00:00:00");

    /* 29-Feb with restricted year range */
    CHECK_NEXT("0 0 29 FEB * 2015-2021", CRON_CALC_OPT_WITH_YEARS,
        "1999-01-01_00:00:00",
        "2016-02-29_00:00:00,"
        "2020-02-29_00:00:00,"
        "-");

    /* Every 15 minutes */
    CHECK_NEXT("*/15 * * * *", CRON_CALC_OPT_DEFAULT,
        "2018-12-31_23:29:00",
        "2018-12-31_23:30:00,"
        "2018-12-31_23:45:00,"
        "2019-01-01_00:00:00,"
        "2019-01-01_00:15:00");

    /* Twice a minute */
    CHECK_NEXT("*/30 * * * * *", CRON_CALC_OPT_WITH_SECONDS,
        "2018-12-31_23:59:00",
        "2018-12-31_23:59:30,"
        "2019-01-01_00:00:00,"
        "2019-01-01_00:00:30,"
        "2019-01-01_00:01:00");

    /* Ranges */
    CHECK_NEXT("5,10 5-10 * * *", CRON_CALC_OPT_DEFAULT,
        "2018-12-31_23:00:00",
        "2019-01-01_05:05:00,2019-01-01_05:10:00,"
        "2019-01-01_06:05:00,2019-01-01_06:10:00,"
        "2019-01-01_07:05:00,2019-01-01_07:10:00,"
        "2019-01-01_08:05:00,2019-01-01_08:10:00,"
        "2019-01-01_09:05:00,2019-01-01_09:10:00,"
        "2019-01-01_10:05:00,2019-01-01_10:10:00,"
        "2019-01-02_05:05:00,2019-01-02_05:10:00");

    CHECK_NEXT("5,10 5-10/2 * * * *", CRON_CALC_OPT_WITH_SECONDS,
        "2018-12-31_23:00:00",
        "2018-12-31_23:05:05,2018-12-31_23:05:10,"
        "2018-12-31_23:07:05,2018-12-31_23:07:10,"
        "2018-12-31_23:09:05,2018-12-31_23:09:10,"
        "2019-01-01_00:05:05,2019-01-01_00:05:10");

    CHECK_NEXT("0 0 5-10,21,30-31 * *", CRON_CALC_OPT_DEFAULT,
        "2018-12-30_23:00:00",
        "2018-12-31_00:00:00,"
        "2019-01-05_00:00:00,2019-01-06_00:00:00,2019-01-07_00:00:00,"
        "2019-01-08_00:00:00,2019-01-09_00:00:00,2019-01-10_00:00:00,"
        "2019-01-21_00:00:00,2019-01-30_00:00:00,2019-01-31_00:00:00,"
        "2019-02-05_00:00:00,2019-02-06_00:00:00,2019-02-07_00:00:00,"
        "2019-02-08_00:00:00,2019-02-09_00:00:00,2019-02-10_00:00:00,"
        "2019-02-21_00:00:00,"
        "2019-03-05_00:00:00,2019-03-06_00:00:00");

    CHECK_NEXT("0 0 1,31 FEB-AUG/2,OCT,11 *", CRON_CALC_OPT_DEFAULT,
        "2018-12-30_23:00:00",
        "2019-02-01_00:00:00,"
        "2019-04-01_00:00:00,"
        "2019-06-01_00:00:00,"
        "2019-08-01_00:00:00,2019-08-31_00:00:00,"
        "2019-10-01_00:00:00,2019-10-31_00:00:00,"
        "2019-11-01_00:00:00,"
        "2020-02-01_00:00:00");

    CHECK_NEXT("0 10 * * MON-FRI", CRON_CALC_OPT_DEFAULT,
        "2018-12-30_23:00:00",
        "2018-12-31_10:00:00,"
        "2019-01-01_10:00:00,2019-01-02_10:00:00,"
        "2019-01-03_10:00:00,2019-01-04_10:00:00,"
        "2019-01-07_10:00:00,2019-01-08_10:00:00")

    CHECK_NEXT("0 10 * * MON-WED,4,5", CRON_CALC_OPT_DEFAULT,
        "2018-12-30_23:00:00",
        "2018-12-31_10:00:00,"
        "2019-01-01_10:00:00,2019-01-02_10:00:00,"
        "2019-01-03_10:00:00,2019-01-04_10:00:00,"
        "2019-01-07_10:00:00,2019-01-08_10:00:00")

    CHECK_NEXT("0 12 * * SAT,SUN", CRON_CALC_OPT_DEFAULT,
        "2018-12-30_23:00:00",
        "2019-01-05_12:00:00,2019-01-06_12:00:00,"
        "2019-01-12_12:00:00")

    /* Exact points */
    CHECK_NEXT("59 23 31 12 *", CRON_CALC_OPT_DEFAULT,
        "2018-12-30_23:00:00",
        "2018-12-31_23:59:00,2019-12-31_23:59:00,2020-12-31_23:59:00,2021-12-31_23:59:00");

    CHECK_NEXT("58 59 23 31 12 *", CRON_CALC_OPT_WITH_SECONDS,
        "2018-12-30_23:00:00",
        "2018-12-31_23:59:58,2019-12-31_23:59:58,2020-12-31_23:59:58,2021-12-31_23:59:58");

    CHECK_NEXT("59 23 31 12 * 2020", CRON_CALC_OPT_WITH_YEARS,
        "2018-12-30_23:00:00",
        "2020-12-31_23:59:00,-,2020-12-31_23:59:00,-");

    CHECK_NEXT("* 12 * * *", CRON_CALC_OPT_DEFAULT,
        "2018-12-30_23:00:00",
        "2018-12-31_12:00:00,2018-12-31_12:01:00,2018-12-31_12:02:00");

    CHECK_NEXT("* * 12 * * *", CRON_CALC_OPT_WITH_SECONDS,
        "2018-12-30_23:00:00",
        "2018-12-31_12:00:00,2018-12-31_12:00:01,2018-12-31_12:00:02");

    /* Last */
    CHECK_NEXT("10 7 L * *", CRON_CALC_OPT_DEFAULT,
        "2018-12-30_23:00:00",
        "2018-12-31_07:10:00,2019-01-31_07:10:00,"
        "2019-02-28_07:10:00,2019-03-31_07:10:00,2019-04-30_07:10:00,"
        "2019-05-31_07:10:00,2019-06-30_07:10:00,2019-07-31_07:10:00,"
        "2019-08-31_07:10:00,2019-09-30_07:10:00,2019-10-31_07:10:00,"
        "2019-11-30_07:10:00,2019-12-31_07:10:00,2020-01-31_07:10:00,"
        "2020-02-29_07:10:00,2020-03-31_07:10:00");

    CHECK_NEXT("10 7 1,L * *", CRON_CALC_OPT_DEFAULT,
        "2019-12-30_23:00:00",
        "2019-12-31_07:10:00,"
        "2020-01-01_07:10:00,2020-01-31_07:10:00,"
        "2020-02-01_07:10:00,2020-02-29_07:10:00,"
        "2020-03-01_07:10:00,2020-03-31_07:10:00,"
        "2020-04-01_07:10:00,2020-04-30_07:10:00");

    CHECK_NEXT("10 7 1,18-20,L * *", CRON_CALC_OPT_DEFAULT,
        "2019-12-30_23:00:00",
        "2019-12-31_07:10:00,"
        "2020-01-01_07:10:00,2020-01-18_07:10:00,2020-01-19_07:10:00,2020-01-20_07:10:00,2020-01-31_07:10:00,"
        "2020-02-01_07:10:00,2020-02-18_07:10:00,2020-02-19_07:10:00,2020-02-20_07:10:00,2020-02-29_07:10:00,"
        "2020-03-01_07:10:00,2020-03-18_07:10:00,2020-03-19_07:10:00,2020-03-20_07:10:00,2020-03-31_07:10:00");

    /* Synonyms */
    CHECK_SAME(
        "* * * 1-12 *", CRON_CALC_OPT_DEFAULT,
        "* * * FEB,JAN,MAR,APR,MAY,JUN,JUL,AUG,SEP,OCT,NOV,DEC *", CRON_CALC_OPT_DEFAULT);
    CHECK_SAME(
        "* * * 1-12 *", CRON_CALC_OPT_DEFAULT,
        "* * * JAN-DEC *", CRON_CALC_OPT_DEFAULT);
    CHECK_SAME(
        "* * * 2-10/3 *", CRON_CALC_OPT_DEFAULT,
        "* * * FEB,MAY,AUG *", CRON_CALC_OPT_DEFAULT);
    CHECK_SAME(
        "* * * 2 *", CRON_CALC_OPT_DEFAULT,
        "* * * Feb *", CRON_CALC_OPT_DEFAULT);
    CHECK_SAME(
        "* * * 2 *", CRON_CALC_OPT_DEFAULT,
        "* * * fEB *", CRON_CALC_OPT_DEFAULT);
    CHECK_SAME(
        "1,3,5 * * * *", CRON_CALC_OPT_DEFAULT,
        "1-6/2 * * * *", CRON_CALC_OPT_DEFAULT);
    CHECK_SAME(
        "50,53,56,59 * * * *", CRON_CALC_OPT_DEFAULT,
        "50-59/3 * * * *", CRON_CALC_OPT_DEFAULT);
    CHECK_SAME(
        "* 4,8,12,16,20 * * *", CRON_CALC_OPT_DEFAULT,
        "* 4-23/4 * * *", CRON_CALC_OPT_DEFAULT);
    CHECK_SAME(
        "* * * * 0-6", CRON_CALC_OPT_DEFAULT,
        "* * * * TUE,WED,THU,FRI,SAT,SUN,MON", CRON_CALC_OPT_DEFAULT);
    CHECK_SAME(
        "* * * * 0-6", CRON_CALC_OPT_DEFAULT,
        "* * * * SUN-SAT", CRON_CALC_OPT_DEFAULT);
    CHECK_SAME(
        "* * * * 1-6", CRON_CALC_OPT_DEFAULT,
        "* * * * MON-SAT", CRON_CALC_OPT_DEFAULT);
    CHECK_SAME(
        "* * * * 0", CRON_CALC_OPT_DEFAULT,
        "* * * * SUN", CRON_CALC_OPT_DEFAULT);
    CHECK_SAME(
        "* * * * 0", CRON_CALC_OPT_DEFAULT,
        "* * * * 7", CRON_CALC_OPT_DEFAULT);
    CHECK_SAME(
        "* * * * 0-7", CRON_CALC_OPT_DEFAULT,
        "* * * * 0-6", CRON_CALC_OPT_DEFAULT);
    CHECK_SAME(
        "* * * * 1-7", CRON_CALC_OPT_DEFAULT,
        "* * * * 0-6", CRON_CALC_OPT_DEFAULT);

    /* Earliest */
    CronCalc cron;
    const time_t T1 = TS("2018-12-30_22:00:00");

    cron.addRule("0 10 * JAN MON");
    CHECK_EQ_INT(cron.next(T1), TS("2019-01-07_10:00:00"));
    cron.addRule("0 10 * JAN TUE");
    CHECK_EQ_INT(cron.next(T1), TS("2019-01-01_10:00:00")); // rule 2
    CHECK_EQ_INT(cron.next(TS("2019-01-01_10:00:00")), TS("2019-01-07_10:00:00")); // rule 1
    cron.addRule("0 10 * * MON");
    CHECK_EQ_INT(cron.next(T1), TS("2018-12-31_10:00:00")); // rule 3
    cron.addRule("0 11 * * *");
    CHECK_EQ_INT(cron.next(T1), TS("2018-12-31_10:00:00")); // rule 3
    cron.addRule("0 * * * *");
    CHECK_EQ_INT(cron.next(T1), TS("2018-12-30_23:00:00")); // rule 5

    printf("Failures: %d\n", gNumErrors);
    return gNumErrors;
}
