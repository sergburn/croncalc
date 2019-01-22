/**
 * Copyright (c) 2018 Sergey Burnevsky (sergey.burnevsky @ gmail.com)
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <time.h>
#include <stdio.h>
#include <string.h>

#include "cron_calc.hpp"

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

bool parseTimeString(const char* tm_str, struct tm* tm_val)
{
    memset(tm_val, 0, sizeof(struct tm));

    if (sscanf(tm_str, TM_SCAN_FMT,
        &tm_val->tm_year, &tm_val->tm_mon, &tm_val->tm_mday,
        &tm_val->tm_hour, &tm_val->tm_min, &tm_val->tm_sec) == 6)
    {
        time_t t;

        CHECK_TRUE(tm_val->tm_year >= 1900);
        tm_val->tm_year -= 1900;
        CHECK_TRUE(tm_val->tm_mon >= 1);
        tm_val->tm_mon -= 1;

        /* normalize to get other fields */
        t = mktime(tm_val);

        if (t >= (time_t)(0))
        {
            return true;
        }

        printf("ERROR: Can't decode time %d\n", (int) t);
        return false;
    }
    printf("ERROR: Can't parse time string '%s'\n", tm_str);
    return false;
}

/* ---------------------------------------------------------------------------- */

#if 0
void check_same(const char* expr1, const char* expr2)
{
    CronCalc cron1, cron2;

    CHECK_TRUE(CRON_CALC_OK == cron1.parse(expr1, NULL));
    CHECK_TRUE(CRON_CALC_OK == cron2.parse(expr2, NULL));

    CHECK_TRUE(cron1 == cron2);
}
#endif

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
    CHECK_EQ_INT_LN(err, cron.parse(expr, options, &err_location), lineno);
    CHECK_EQ_INT_LN(err_offset, err_location - expr, lineno);
}

#define CHECK_INVALID(expr_, opts_, err_, err_offset_) \
    check_invalid(expr_, opts_, err_, err_offset_, __LINE__)

/* ---------------------------------------------------------------------------- */

bool check_next(
    const char* expr,
    cron_calc_option_mask options,
    const char* initial,
    const char* next1,
    const char* next2,
    const char* next3,
    int lineno)
{
    int numErrors = gNumErrors;

    CronCalc cron;
    const char* err_location = NULL;
    cron_calc_error err = cron.parse(expr, options, &err_location);
    CHECK_EQ_INT(CRON_CALC_OK, err);
    CHECK_TRUE(NULL == err_location);

    if (err != CRON_CALC_OK)
    {
        return false;
    }

    struct tm calinit, calnext1, calnext2, calnext3;
    CHECK_TRUE(parseTimeString(initial, &calinit));
    CHECK_TRUE(parseTimeString(next1, &calnext1));
    CHECK_TRUE(parseTimeString(next2, &calnext2));
    CHECK_TRUE(parseTimeString(next3, &calnext3));

    time_t tinit = mktime(&calinit);
    time_t tnext1 = mktime(&calnext1);
    time_t tnext2 = mktime(&calnext2);
    time_t tnext3 = mktime(&calnext3);

    CHECK_EQ_TIME(tnext1, cron.next(tinit));
    CHECK_EQ_TIME(tnext2, cron.next(tnext1));
    CHECK_EQ_TIME(tnext3, cron.next(tnext2));

    return (numErrors == gNumErrors);
}

#define CHECK_NEXT(expr_, opts_, start_, n1_, n2_, n3_) \
    CHECK_TRUE_LN(check_next(expr_, opts_, start_, n1_, n2_, n3_, __LINE__), __LINE__)

/* ---------------------------------------------------------------------------- */

int main()
{
    /* bad format */
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

    /* number ranges */
    CHECK_INVALID("60 * * * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_RANGE, 0);
    CHECK_INVALID("0-60 * * * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_RANGE, 2);
    CHECK_INVALID("59 24 * * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_RANGE, 3);
    CHECK_INVALID("59 0-24 * * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_RANGE, 5);
    CHECK_INVALID("59 23 0 * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_RANGE, 6);
    CHECK_INVALID("59 23 32 * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_RANGE, 6);
    CHECK_INVALID("59 23 0-31 * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_RANGE, 6);
    CHECK_INVALID("59 23 1-32 * *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_RANGE, 8);
    CHECK_INVALID("59 23 31 0 *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_RANGE, 9);
    CHECK_INVALID("59 23 31 13 *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_RANGE, 9);
    CHECK_INVALID("59 23 31 0-12 *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_RANGE, 9);
    CHECK_INVALID("59 23 31 1-13 *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_RANGE, 11);
    CHECK_INVALID("59 23 31 12 8", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_RANGE, 12);
    CHECK_INVALID("59 23 31 12 0-8", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_NUMBER_RANGE, 14);
    CHECK_INVALID("60 59 23 31 12 6", CRON_CALC_OPT_WITH_SECONDS, CRON_CALC_ERROR_NUMBER_RANGE, 0);
    CHECK_INVALID("0-60 59 23 31 12 6", CRON_CALC_OPT_WITH_SECONDS, CRON_CALC_ERROR_NUMBER_RANGE, 2);
    CHECK_INVALID("59 23 31 12 6 1999", CRON_CALC_OPT_WITH_YEARS, CRON_CALC_ERROR_NUMBER_RANGE, 14);
    CHECK_INVALID("59 23 31 12 6 1999-2063", CRON_CALC_OPT_WITH_YEARS, CRON_CALC_ERROR_NUMBER_RANGE, 14);
    CHECK_INVALID("59 23 31 12 6 2064", CRON_CALC_OPT_WITH_YEARS, CRON_CALC_ERROR_NUMBER_RANGE, 14);
    CHECK_INVALID("59 23 31 12 6 2000-2064", CRON_CALC_OPT_WITH_YEARS, CRON_CALC_ERROR_NUMBER_RANGE, 19);
    CHECK_INVALID("59 59 23 31 12 6 1999", CRON_CALC_OPT_FULL, CRON_CALC_ERROR_NUMBER_RANGE, 17);
    CHECK_INVALID("59 59 23 31 12 6 1999-2063", CRON_CALC_OPT_FULL, CRON_CALC_ERROR_NUMBER_RANGE, 17);
    CHECK_INVALID("59 59 23 31 12 6 2064", CRON_CALC_OPT_FULL, CRON_CALC_ERROR_NUMBER_RANGE, 17);
    CHECK_INVALID("59 59 23 31 12 6 2000-2064", CRON_CALC_OPT_FULL, CRON_CALC_ERROR_NUMBER_RANGE, 22);

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
    CHECK_INVALID("* * * M *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_INVALID_NAME, 6);
    CHECK_INVALID("* * * MA *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_INVALID_NAME, 6);
    CHECK_INVALID("* * * MAN *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_INVALID_NAME, 6);
    CHECK_INVALID("* * * MAY M", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_INVALID_NAME, 10);
    CHECK_INVALID("* * * MAY MO", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_INVALID_NAME, 10);
    CHECK_INVALID("* * * MAY MOY", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_INVALID_NAME, 10);
    CHECK_INVALID("* * * MAY-AUF MOY", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_INVALID_NAME, 10);
    CHECK_INVALID("* * * MAY-AUG MON-TUF", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_INVALID_NAME, 18);

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
    CHECK_INVALID("* * 31 FEB *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_IMPOSSIBLE_DATE, 12);
    CHECK_INVALID("* * 31 APR *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_IMPOSSIBLE_DATE, 12);
    CHECK_INVALID("* * 31 JUN *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_IMPOSSIBLE_DATE, 12);
    CHECK_INVALID("* * 31 SEP *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_IMPOSSIBLE_DATE, 12);
    CHECK_INVALID("* * 31 11 *", CRON_CALC_OPT_DEFAULT, CRON_CALC_ERROR_IMPOSSIBLE_DATE, 11);
    CHECK_INVALID("* * 29 FEB * 2001", CRON_CALC_OPT_WITH_YEARS, CRON_CALC_ERROR_IMPOSSIBLE_DATE, 17);
    CHECK_INVALID("* * 29 FEB * 2001-2003", CRON_CALC_OPT_WITH_YEARS, CRON_CALC_ERROR_IMPOSSIBLE_DATE, 22);

    /* Next */

    CHECK_NEXT("* * * * *", CRON_CALC_OPT_DEFAULT,
        "2018-12-31_23:58:00",
        "2018-12-31_23:59:00",
        "2019-01-01_00:00:00", /* !!! champagne time !!! */
        "2019-01-01_00:01:00");

    CHECK_NEXT("* * * * * *", CRON_CALC_OPT_WITH_SECONDS,
        "2018-12-31_23:59:58",
        "2018-12-31_23:59:59",
        "2019-01-01_00:00:00", /* !!! champagne time !!! */
        "2019-01-01_00:00:01");

    CHECK_NEXT("*/2 * * * *", CRON_CALC_OPT_DEFAULT,
        "2018-12-31_23:57:00",
        "2018-12-31_23:58:00",
        "2019-01-01_00:00:00",
        "2019-01-01_00:02:00");

    CHECK_NEXT("0 */2 * * *", CRON_CALC_OPT_DEFAULT,
        "2018-12-31_21:57:00",
        "2018-12-31_22:00:00",
        "2019-01-01_00:00:00",
        "2019-01-01_02:00:00");

    /* this means 'every odd date' or 'every 2nd day of month', but NOT 'every two days' */
    CHECK_NEXT("1 2 */2 * *", CRON_CALC_OPT_DEFAULT,
        "2018-12-29_21:57:00",
        "2018-12-31_02:01:00",
        "2019-01-01_02:01:00",
        "2019-01-03_02:01:00");

    /* same for even days */
    CHECK_NEXT("1 2 2-31/2 * *", CRON_CALC_OPT_DEFAULT,
        "2018-12-29_21:57:00",
        "2018-12-30_02:01:00",
        "2019-01-02_02:01:00",
        "2019-01-04_02:01:00");

#if 0

    check_next("* 1-4 * * * * */15", "2012-07-01_09:53:50", "2012-07-02_01:00:00");
    check_next("* 1-4 * * * * */15", "2012-07-01_09:53:00", "2012-07-02_01:00:00");
    check_next("0 */2 1-4 * * *", "2012-07-01_09:00:00", "2012-07-02_01:00:00");
    check_next("0 */2 * * * *", "2012-07-01_09:00:00", "2012-07-01_09:02:00");
    check_next("0 */2 * * * *", "2013-07-01_09:00:00", "2013-07-01_09:02:00");
    check_next("0 */2 * * * *", "2018-09-14_14:24:00", "2018-09-14_14:26:00");
    check_next("0 */2 * * * *", "2018-09-14_14:25:00", "2018-09-14_14:26:00");
    check_next("0 */20 * * * *", "2018-09-14_14:24:00", "2018-09-14_14:40:00");
    check_next("* * * * * *", "2012-07-01_09:00:00", "2012-07-01_09:00:01");
    check_next("* * * * * *", "2012-12-01_09:00:58", "2012-12-01_09:00:59");
    check_next("10 * * * * *", "2012-12-01_09:42:09", "2012-12-01_09:42:10");
    check_next("11 * * * * *", "2012-12-01_09:42:10", "2012-12-01_09:42:11");
    check_next("10 * * * * *", "2012-12-01_09:42:10", "2012-12-01_09:43:10");
    check_next("10-15 * * * * *", "2012-12-01_09:42:09", "2012-12-01_09:42:10");
    check_next("10-15 * * * * *", "2012-12-01_21:42:14", "2012-12-01_21:42:15");
    check_next("0 * * * * *", "2012-12-01_21:10:42", "2012-12-01_21:11:00");
    check_next("0 * * * * *", "2012-12-01_21:11:00", "2012-12-01_21:12:00");
    check_next("0 11 * * * *", "2012-12-01_21:10:42", "2012-12-01_21:11:00");
    check_next("0 10 * * * *", "2012-12-01_21:11:00", "2012-12-01_22:10:00");
    check_next("0 0 * * * *", "2012-09-30_11:01:00", "2012-09-30_12:00:00");
    check_next("0 0 * * * *", "2012-09-30_12:00:00", "2012-09-30_13:00:00");
    check_next("0 0 * * * *", "2012-09-10_23:01:00", "2012-09-11_00:00:00");
    check_next("0 0 * * * *", "2012-09-11_00:00:00", "2012-09-11_01:00:00");
    check_next("0 0 0 * * *", "2012-09-01_14:42:43", "2012-09-02_00:00:00");
    check_next("0 0 0 * * *", "2012-09-02_00:00:00", "2012-09-03_00:00:00");
    check_next("* * * 10 * *", "2012-10-09_15:12:42", "2012-10-10_00:00:00");
    check_next("* * * 10 * *", "2012-10-11_15:12:42", "2012-11-10_00:00:00");
    check_next("0 0 0 * * *", "2012-09-30_15:12:42", "2012-10-01_00:00:00");
    check_next("0 0 0 * * *", "2012-10-01_00:00:00", "2012-10-02_00:00:00");
    check_next("0 0 0 * * *", "2012-08-30_15:12:42", "2012-08-31_00:00:00");
    check_next("0 0 0 * * *", "2012-08-31_00:00:00", "2012-09-01_00:00:00");
    check_next("0 0 0 * * *", "2012-10-30_15:12:42", "2012-10-31_00:00:00");
    check_next("0 0 0 * * *", "2012-10-31_00:00:00", "2012-11-01_00:00:00");
    check_next("0 0 0 1 * *", "2012-10-30_15:12:42", "2012-11-01_00:00:00");
    check_next("0 0 0 1 * *", "2012-11-01_00:00:00", "2012-12-01_00:00:00");
    check_next("0 0 0 1 * *", "2010-12-31_15:12:42", "2011-01-01_00:00:00");
    check_next("0 0 0 1 * *", "2011-01-01_00:00:00", "2011-02-01_00:00:00");
    check_next("0 0 0 31 * *", "2011-10-30_15:12:42", "2011-10-31_00:00:00");
    check_next("0 0 0 1 * *", "2011-10-30_15:12:42", "2011-11-01_00:00:00");
    check_next("* * * * * 2", "2010-10-25_15:12:42", "2010-10-26_00:00:00");
    check_next("* * * * * 2", "2010-10-20_15:12:42", "2010-10-26_00:00:00");
    check_next("* * * * * 2", "2010-10-27_15:12:42", "2010-11-02_00:00:00");
    check_next("55 5 * * * *", "2010-10-27_15:04:54", "2010-10-27_15:05:55");
    check_next("55 5 * * * *", "2010-10-27_15:05:55", "2010-10-27_16:05:55");
    check_next("55 * 10 * * *", "2010-10-27_09:04:54", "2010-10-27_10:00:55");
    check_next("55 * 10 * * *", "2010-10-27_10:00:55", "2010-10-27_10:01:55");
    check_next("* 5 10 * * *", "2010-10-27_09:04:55", "2010-10-27_10:05:00");
    check_next("* 5 10 * * *", "2010-10-27_10:05:00", "2010-10-27_10:05:01");
    check_next("55 * * 3 * *", "2010-10-02_10:05:54", "2010-10-03_00:00:55");
    check_next("55 * * 3 * *", "2010-10-03_00:00:55", "2010-10-03_00:01:55");
    check_next("* * * 3 11 *", "2010-10-02_14:42:55", "2010-11-03_00:00:00");
    check_next("* * * 3 11 *", "2010-11-03_00:00:00", "2010-11-03_00:00:01");
    check_next("0 0 0 29 2 *", "2007-02-10_14:42:55", "2008-02-29_00:00:00");
    check_next("0 0 0 29 2 *", "2008-02-29_00:00:00", "2012-02-29_00:00:00");
    check_next("0 0 7 ? * MON-FRI", "2009-09-26_00:42:55", "2009-09-28_07:00:00");
    check_next("0 0 7 ? * MON-FRI", "2009-09-28_07:00:00", "2009-09-29_07:00:00");
    check_next("0 30 23 30 1/3 ?", "2010-12-30_00:00:00", "2011-01-30_23:30:00");
    check_next("0 30 23 30 1/3 ?", "2011-01-30_23:30:00", "2011-04-30_23:30:00");
    check_next("0 30 23 30 1/3 ?", "2011-04-30_23:30:00", "2011-07-30_23:30:00");

    check_same("* * 2 * *", "* * 2 * ?");
    check_same("57,59 * * * * *", "57/2 * * * * *");
    check_same("1,3,5 * * * * *", "1-6/2 * * * * *");
    check_same("* 4,8,12,16,20 * * *", "* 4/4 * * *");
    check_same("* * * * 0-6", "* * * * TUE,WED,THU,FRI,SAT,SUN,MON");
    check_same("* * * * 0-6", "* * * * MON-SAT");
    check_same("* * * * 0", "* * * * SUN");
    check_same("* * * * 0", "* * * * 7");
    check_same("* * * 1-12 *", "* * * FEB,JAN,MAR,APR,MAY,JUN,JUL,AUG,SEP,OCT,NOV,DEC *");
    check_same("* * * 1-12 *", "* * * JAN-DEC *");
    check_same("* * * 2 *", "* * * Feb *");
    check_same("*  *  * *  1 *", "* * * * 1 *");
#endif
    printf("Failures: %d\n", gNumErrors);
    return gNumErrors;
}
