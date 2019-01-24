/*
 * Copyright (c) 2018 Sergey Burnevsky (sergey.burnevsky @ gmail.com)
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef CRON_CALC_H_
#define CRON_CALC_H_

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum cron_calc_option
{
    CRON_CALC_OPT_DEFAULT       = 0x0,  /*!< Default options, only 5 standard required fields expected */
    CRON_CALC_OPT_WITH_SECONDS  = 0x1,  /*!< If set, first field is treated as seconds */
    CRON_CALC_OPT_WITH_YEARS    = 0x2,  /*!< If set, one more field for years is supported */
    CRON_CALC_OPT_ASSUME_STAR   = 0x4,  /*!< If set, allows to omit the remainder of cron expression,
                                             which contains only '*'s, full range will be used for such fields */

    CRON_CALC_OPT_FULL = CRON_CALC_OPT_WITH_SECONDS | CRON_CALC_OPT_WITH_YEARS,

    CRON_CALC_OPT_RESERVED_10   = 0x10,
    CRON_CALC_OPT_RESERVED_20   = 0x20,
    CRON_CALC_OPT_RESERVED_40   = 0x40,
    CRON_CALC_OPT_RESERVED_80   = 0x80
} cron_calc_option;

typedef uint8_t cron_calc_option_mask;

typedef struct cron_calc
{
    uint64_t years;
    uint64_t seconds;
    uint64_t minutes;
    uint32_t hours;
    uint32_t days;
    uint16_t months;
    uint8_t weekDays;
    cron_calc_option_mask options;
} cron_calc;

typedef enum cron_calc_error
{
    CRON_CALC_OK = 0,                       /*!< Success */
    CRON_CALC_ERROR_ARGUMENT = 1,           /*!< Function arguments are invalid */
    CRON_CALC_ERROR_FIELD_FORMAT = 2,       /*!< Invalid field format */
    CRON_CALC_ERROR_NUMBER_RANGE = 3,       /*!< Number range violated */
    CRON_CALC_ERROR_EXPR_SHORT = 4,         /*!< Expression too short, not all required fields found */
    CRON_CALC_ERROR_EXPR_LONG = 5,          /*!< Expression too long, did not end after last supported field */
    CRON_CALC_ERROR_INVALID_NAME = 6,       /*!< Unknown value name detected */
    CRON_CALC_ERROR_NUMBER_EXPECTED = 7,    /*!< Number could not be parsed */
    CRON_CALC_ERROR_IMPOSSIBLE_DATE = 8     /*!< Date specified in expression never matches, e.g. Nov-31 or 2001-Feb-29 */
} cron_calc_error;

#define CRON_CALC_INVALID_TIME ((time_t) -1) /* as defined in mktime() */

/**
 * Supported format:
 *  [SP <seconds> ] <minutes> SP <hours> SP <days> SP <months> SP <week days> [ SP <years> ]
 *
 * Each field is in format:
 *  *[/<step>] | <num>[-<num>[/<step>]][,...]
 *
 * Allowed nums are:
 *  <minutes>   : 0-59
 *  <hours>     : 0-23
 *  <days>      : 1-31
 *  <months>    : 1-12 or JAN through DEC
 *  <week days> : 0-7 or SUN through SAT; 0 and 7 == SUN
 *  <years>     : 2000-2063     CRON_CALC_OPT_WITH_YEARS must be set in options
 *  <seconds>   : 0-59          CRON_CALC_OPT_WITH_SECONDS must be set in options
 *
 * Step is allowed only after range (i.e. * or x-y). Step must be > 0.
 * Value names are also allowed in ranges (e.g. MON-WED), even mixed (0-TUE).
 *
 * With CRON_CALC_OPT_ASSUME_STAR option set short expressions are allowed,
 * e.g. shorthand '* 1-10' is equivalent to full expression '* 1-10 * * *'
 *
 * @param self The object to store parsed Cron rule
 * @param expr Cron expression, NULL-terminated string
 * @param options Parsing options
 * @param[out] err_location If not NULL and CRON_CALC_ERROR_EXPRESSION is returned,
 *                          points to the first character in the expression,
 *                          where parsing error occured.
 * @return CRON_CALC_OK on success
 * @return CRON_CALC_ERROR_EXPRESSION if expression was invalid
 * @return CRON_CALC_ERROR_ARGUMENT if one or more arguments invalid.
 */
cron_calc_error cron_calc_parse(cron_calc* self, const char* expr, cron_calc_option_mask options, const char** err_location);

/**
 * Calculates next time instant with regards to given reference time.
 * This function takes reference time from given `struct tm` object and updates it
 * with the next time instant according to the rule stored in `cron_calc` object.
 *
 * @param self The ready cron_calc object, initialized by successful cron_calc_parse() call.
 *             Must not be NULL or contain only zeroes.
 * @param after Time instant to start next search after.
 *              Note that even if given time matches specified rule,
 *              it will not be returned, only some moment `after` it.
 *              This allows to calol this function in a loop without changing argument:
 *              @code
 *              time_t start = 1515151515;
 *              time_t next = cron_calc_next(..., start);
 *              next = cron_calc_next(..., next);
 *              next = cron_calc_next(..., next);
 *              ...
 *              @endcode
 * @return Next time instant, or NULL if arguments are invalid
 */
time_t cron_calc_next(const cron_calc* self, time_t after);

/**
 * Utility function, compares two initialized `cron_calc` objects.
 * @return Whether given objects are same.
 * @return False if any of the pointers is NULL or any of the objects is invalid.
 */
bool cron_calc_is_same(const cron_calc* left, const cron_calc* right);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* CRON_CALC_H_ */
