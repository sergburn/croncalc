/*
 * Copyright (c) 2018 Sergey Burnevsky (sergey.burnevsky @ gmail.com)
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <string.h>
#include <ctype.h>

#include "cron_calc.h"

typedef enum cron_calc_field
{
    CRON_CALC_FIELD_SECONDS,
    CRON_CALC_FIELD_MINUTES,
    CRON_CALC_FIELD_HOURS,
    CRON_CALC_FIELD_DAYS,
    CRON_CALC_FIELD_MONTHS,
    CRON_CALC_FIELD_WDAYS,
    CRON_CALC_FIELD_YEARS,

    CRON_CALC_FIELD_LAST = CRON_CALC_FIELD_YEARS
} cron_calc_field;

enum
{
    CRON_CALC_NAME_LEN = 3, /* All names in Cron have 3 chars */
    CRON_CALC_NAME_UPCASE = 'a' - 'A',
    CRON_CALC_YEAR_START = 2000,
    CRON_CALC_YEAR_END = CRON_CALC_YEAR_START + 63
};

static const char* const CRON_CALC_DAY_NAMES[] = {
    "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"
};

static const char* const CRON_CALC_MONTH_NAMES[] = {
    "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
};

enum
{
    CRON_CALC_DAY_NAMES_NUM = sizeof(CRON_CALC_DAY_NAMES) / sizeof(char*),
    CRON_CALC_MONTH_NAMES_NUM = sizeof(CRON_CALC_MONTH_NAMES) / sizeof(char*)
};

typedef struct cron_calc_field_def
{
    uint32_t min;
    uint32_t max;
    const char* const* names;
    uint32_t names_count;
    uint32_t names_first_index;
} cron_calc_field_def;

static const cron_calc_field_def K_CRON_CALC_FIELD_DEFS[CRON_CALC_FIELD_LAST + 1] = {
    { 0,        59,     NULL,                       0 },                            /* CRON_CALC_FIELD_SECONDS */
    { 0,        59,     NULL,                       0 },                            /* CRON_CALC_FIELD_MINUTES */
    { 0,        23,     NULL,                       0 },                            /* CRON_CALC_FIELD_HOURS */
    { 1,        31,     NULL,                       0 },                            /* CRON_CALC_FIELD_DAYS */
    { 1,        12,     CRON_CALC_MONTH_NAMES,      CRON_CALC_MONTH_NAMES_NUM },    /* CRON_CALC_FIELD_MONTHS */
    { 0,        6,      CRON_CALC_DAY_NAMES,        CRON_CALC_DAY_NAMES_NUM },      /* CRON_CALC_FIELD_WDAYS */
    { CRON_CALC_YEAR_START, CRON_CALC_YEAR_END, NULL, 0 },                          /* CRON_CALC_FIELD_YEARS */
};

/* ---------------------------------------------------------------------------- */

#define CRON_CALC_IS_DIGIT(a_) ((a_) >= '0' && (a_) <= '9')
#define CRON_CALC_IS_NAME_CHAR(a_) (((a_) >= 'A' && (a_) <= 'Z') || ((a_) >= 'a' && (a_) <= 'z'))

/* ---------------------------------------------------------------------------- */

static cron_calc_error cron_calc_set_field(
    cron_calc* self,
    uint32_t min,
    uint32_t max,
    uint32_t step,
    cron_calc_field field)
{
    int i;
    uint64_t value = 0;

    if (step < 1)
    {
        return CRON_CALC_ERROR_NUMBER_RANGE;
    }

    for (i = min; i <= max; i += step)
    {
        value |= (uint64_t)1 << i;
    }

    /* ranges checked at parsing */
    switch (field)
    {
        case CRON_CALC_FIELD_MINUTES:
            self->minutes = value;
            break;
        case CRON_CALC_FIELD_HOURS:
            self->hours = value;
            break;
        case CRON_CALC_FIELD_DAYS:
            self->days = value;
            break;
        case CRON_CALC_FIELD_MONTHS:
            self->months = value;
            break;
        case CRON_CALC_FIELD_WDAYS:
            self->weekDays = value;
            break;
        case CRON_CALC_FIELD_YEARS:
            self->years = value;
            break;
        case CRON_CALC_FIELD_SECONDS:
            self->seconds = value;
            break;
    }
    return CRON_CALC_OK;
}

/* ---------------------------------------------------------------------------- */

static void cron_calc_set_field_default(cron_calc* self, cron_calc_field field)
{
    const cron_calc_field_def* field_def = &K_CRON_CALC_FIELD_DEFS[field];
    cron_calc_set_field(self, field_def->min, field_def->max, 1, field);
}

/* ---------------------------------------------------------------------------- */

static cron_calc_error cron_calc_parse_limited_number(const char** pp, uint32_t* value, uint32_t minimum, uint32_t maximum)
{
    uint32_t val = 0;
    const char* p = *pp;

    /* this function returns pointer to the number start
     * as error location in all kinds of errors */

    for (; *p && CRON_CALC_IS_DIGIT(*p); p++)
    {
        val = val * 10 + *p - '0';
        if (val > maximum)
        {
            return CRON_CALC_ERROR_NUMBER_RANGE;
        }
    }
    if (p == *pp) /* nothing parsed */
    {
        return CRON_CALC_ERROR_NUMBER_EXPECTED;
    }
    if (val < minimum)
    {
        return CRON_CALC_ERROR_NUMBER_RANGE;
    }
    *pp = p;
    *value = val;
    return CRON_CALC_OK;
}

/* ---------------------------------------------------------------------------- */

static cron_calc_error cron_calc_parse_number(const char** pp, uint32_t* value, const cron_calc_field_def* field_def)
{
    return cron_calc_parse_limited_number(pp, value, field_def->min, field_def->max);
}

/* ---------------------------------------------------------------------------- */

static cron_calc_error cron_calc_parse_name(const char** pp, uint32_t* value, const cron_calc_field_def* field_def)
{
    int i, name_len = 0;
    char name[CRON_CALC_NAME_LEN + 1] = { 0 };
    const char* p = *pp;

    /* this function returns pointer to the name start
     * as error location in all kinds of errors */

    for (; *p && CRON_CALC_IS_NAME_CHAR(*p) && name_len < CRON_CALC_NAME_LEN; p++)
    {
        name[name_len++] = *p;
    }
    if (name_len != CRON_CALC_NAME_LEN)
    {
        return CRON_CALC_ERROR_INVALID_NAME;
    }
    for (i = 0; i < field_def->names_count; i++)
    {
        const char* fname = field_def->names[i];
        if ((fname[0] == name[0] || fname[0] == name[0] + CRON_CALC_NAME_UPCASE) &&
            (fname[1] == name[1] || fname[1] == name[1] + CRON_CALC_NAME_UPCASE) &&
            (fname[2] == name[2] || fname[2] == name[2] + CRON_CALC_NAME_UPCASE))
        {
            *value = i + field_def->names_first_index;
            *pp = p;
            return CRON_CALC_OK;
        }
    }
    return CRON_CALC_ERROR_INVALID_NAME;
}

/* ---------------------------------------------------------------------------- */

static cron_calc_error cron_calc_parse_value(const char** pp, uint32_t* value, const cron_calc_field_def* field_def)
{
    if (field_def->names_count && CRON_CALC_IS_NAME_CHAR(**pp))
    {
        return cron_calc_parse_name(pp, value, field_def);
    }
    return cron_calc_parse_number(pp, value, field_def);
}

/* ---------------------------------------------------------------------------- */

cron_calc_error cron_calc_parse(
    cron_calc* self,
    const char* expr,
    cron_calc_option_mask options,
    const char** err_location)
{
    cron_calc_error err = CRON_CALC_OK;
    uint32_t min, max, step;
    const char* p = expr;
    bool is_range = false;

    cron_calc_field field =
        options & CRON_CALC_OPT_WITH_SECONDS ?
        CRON_CALC_FIELD_SECONDS : CRON_CALC_FIELD_MINUTES;

    cron_calc_field last_field =
        options & CRON_CALC_OPT_WITH_YEARS ?
        CRON_CALC_FIELD_YEARS : CRON_CALC_FIELD_WDAYS;

    if (err_location)
    {
        *err_location = NULL;
    }

    if (!self || !expr)
    {
        return CRON_CALC_ERROR_ARGUMENT;
    }

    while (field <= last_field)
    {
        if (*p == '*')
        {
            min = K_CRON_CALC_FIELD_DEFS[field].min;
            max = K_CRON_CALC_FIELD_DEFS[field].max;
            step = 1;
            is_range = true;
            p++;
        }
        else
        {
            err = cron_calc_parse_value(&p, &min, &K_CRON_CALC_FIELD_DEFS[field]);
            if (err) break;

            is_range = false;
            if (*p == '-') /* max will follow */
            {
                p++;
                err = cron_calc_parse_value(&p, &max, &K_CRON_CALC_FIELD_DEFS[field]);
                if (err) break;
                is_range = true;
            }
        }

        if (is_range && (*p == '/')) /* step will follow */
        {
            p++;
            err = cron_calc_parse_limited_number(&p, &step, 1, K_CRON_CALC_FIELD_DEFS[field].max);
            if (err) break;
        }

        if (*p == 0 || isspace(*p) || *p == ',') /* end of field */
        {
            err = cron_calc_set_field(self, min, max, step, field);
            if (err) break;

            if (isspace(*p)) /* field is complete */
            {
                while (isspace(*p)) p++;
                field++;
            }
            else if (*p == ',') /* more data for this field */
            {
                p++;
            }
            else /* (*p == 0) -> expression complete */
            {
                break;
            }
        }
        else
        {
            err = CRON_CALC_ERROR_FIELD_FORMAT;
            break;
        }
    }

    if (!err && options & CRON_CALC_OPT_ASSUME_STAR)
    {
        for (; field < last_field; field++)
        {
            cron_calc_set_field_default(self, field);
        }
    }

    if (!err)
    {
        if (field < last_field)
        {
            err = CRON_CALC_ERROR_EXPR_SHORT;
        }
        else if (*p != 0) /* unexpected data in the expression */
        {
            err = CRON_CALC_ERROR_EXPR_LONG;
        }
    }

    if (err && err_location)
    {
        if (err_location)
        {
            *err_location = p;
        }
        return err;
    }

    return CRON_CALC_OK;
}

/* ---------------------------------------------------------------------------- */

time_t cron_calc_next(const cron_calc* self, time_t after)
{
    return (time_t) 0;
}

/* ---------------------------------------------------------------------------- */

bool cron_calc_is_same(const cron_calc* left, const cron_calc* right)
{
    return
        left->minutes == right->minutes &&
        left->hours == right->hours &&
        left->days == right->days &&
        left->months == right->months &&
        left->weekDays == right->weekDays &&
        left->years == right->years &&
        left->seconds == right->seconds;
}
