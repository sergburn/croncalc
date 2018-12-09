/**
 * Copyright (c) 2018 Sergey Burnevsky (sergey.burnevsky @ gmail.com)
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <time.h>
#include <stdio.h>

#include "cron_calc.hpp"

int gNumErrors = 0;

#define CHECK_TRUE(arg_) { bool tmp = (arg_); if (!tmp) { \
    gNumErrors++; \
    printf("Line %3d: CHECK_TRUE: " # arg_ "\n", __LINE__); \
    } }

#define CHECK_TRUE_LN(arg_, line_) { bool tmp = (arg_); if (!tmp) { \
    gNumErrors++; \
    printf("Line %3d: CHECK_TRUE: " # arg_ "\n", line_); \
    } }

#define CHECK_EQ_INT_LN(arg1_, arg2_, line_) { \
    uint32_t a1 = (arg1_); \
    uint32_t a2 = (arg2_); \
    if (a1 != a2) { \
        gNumErrors++; \
        printf("Line %3d: CHECK_EQ_INT: %lu != %lu <= (" # arg1_ " != " # arg2_ ")\n", line_, a1, a2); \
    } }

static const char* TM_FORMAT = "%Y-%m-%d_%H:%M:%s";
static const char* TM_SCAN_FMT = "%u-%u-%u_%u:%u:%u";

bool parseTimeString(const char* tmString, struct tm* tmValue)
{
    if (sscanf(tmString, TM_SCAN_FMT,
        &tmValue->tm_year, &tmValue->tm_mon, &tmValue->tm_mday,
        &tmValue->tm_hour, &tmValue->tm_min, &tmValue->tm_sec) == 6)
    {
        CHECK_TRUE(tmValue->tm_year >= 1900);
        tmValue->tm_year -= 1900;
        CHECK_TRUE(tmValue->tm_mon >= 1);
        tmValue->tm_mon -= 1;
        return true;
    }
    printf("Can't parse time string '%s'\n", tmString);
    return false;
}

#if 0
void check_next(const char* pattern, const char* str_initial, const char* str_expected)
{
    CronCalc cron;
    const char* err_location = NULL;
    CHECK_TRUE(CRON_CALC_OK == cron.parse(pattern, &err_location));
    CHECK_TRUE(NULL == err_location);

    struct tm calnext, expected;
    CHECK_TRUE(parseTimeString(str_initial, &calnext));
    CHECK_TRUE(parseTimeString(str_expected, &expected));

    CHECK_TRUE(CRON_CALC_OK == cron.next(&calnext));

    CHECK_TRUE(calnext.tm_year == expected.tm_year);
    CHECK_TRUE(calnext.tm_mon == expected.tm_mon);
    CHECK_TRUE(calnext.tm_mday == expected.tm_mday);
    CHECK_TRUE(calnext.tm_hour == expected.tm_hour);
    CHECK_TRUE(calnext.tm_min == expected.tm_min);
    CHECK_TRUE(calnext.tm_sec == expected.tm_sec);
    CHECK_TRUE(calnext.tm_wday == expected.tm_wday);
}

void check_same(const char* expr1, const char* expr2)
{
    CronCalc cron1, cron2;

    CHECK_TRUE(CRON_CALC_OK == cron1.parse(expr1, NULL));
    CHECK_TRUE(CRON_CALC_OK == cron2.parse(expr2, NULL));

    CHECK_TRUE(cron1 == cron2);
}
#endif

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

#if 0


    CHECK_INVALID("-5 * * * * *", CRON_CALC_ERROR_NUMBER_RANGE, 6);
    CHECK_INVALID("3-2 */5 * * * *", CRON_CALC_ERROR_NUMBER_RANGE, 6);
    CHECK_INVALID("/5 * * * * *", CRON_CALC_ERROR_NUMBER_RANGE, 6);
    CHECK_INVALID("*/0 * * * * *", CRON_CALC_ERROR_NUMBER_RANGE, 6);
    CHECK_INVALID("*/-0 * * * * *", CRON_CALC_ERROR_NUMBER_RANGE, 6);
    CHECK_INVALID("* 1 1 0 * *", CRON_CALC_ERROR_NUMBER_RANGE, 6);

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
    printf("Test failures: %d\n", gNumErrors);
    return gNumErrors;
}
