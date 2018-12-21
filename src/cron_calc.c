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

typedef enum cron_calc_tm_level {
    CRON_CALC_TM_YEAR,
    CRON_CALC_TM_MONTH,
    CRON_CALC_TM_DAY,
    CRON_CALC_TM_HOUR,
    CRON_CALC_TM_MINUTE,
    CRON_CALC_TM_SECOND,

    CRON_CALC_TM_WDAY
} cron_calc_tm_level;

typedef struct cron_calc_tm_field_def
{
    size_t tm_offset;
    cron_calc_field cron_field;
} cron_calc_tm_field_def;

static const cron_calc_tm_field_def K_CRON_CALC_TM_FIELDS[CRON_CALC_FIELD_LAST + 1] = {
    { offsetof(struct tm, tm_year), CRON_CALC_FIELD_YEARS },
    { offsetof(struct tm, tm_mon),  CRON_CALC_FIELD_MONTHS },
    { offsetof(struct tm, tm_mday), CRON_CALC_FIELD_DAYS },
    { offsetof(struct tm, tm_hour), CRON_CALC_FIELD_HOURS },
    { offsetof(struct tm, tm_min),  CRON_CALC_FIELD_MINUTES },
    { offsetof(struct tm, tm_sec),  CRON_CALC_FIELD_SECONDS },
    { offsetof(struct tm, tm_wday), CRON_CALC_FIELD_WDAYS }
};

/* ---------------------------------------------------------------------------- */

#define CRON_CALC_IS_DIGIT(a_) ((a_) >= '0' && (a_) <= '9')
#define CRON_CALC_IS_NAME_CHAR(a_) (((a_) >= 'A' && (a_) <= 'Z') || ((a_) >= 'a' && (a_) <= 'z'))

#define CRON_CALC_TM_CRON_FIELD(tm_field_) (K_CRON_CALC_TM_FIELDS[(tm_field_)].cron_field)
#define CRON_CALC_TM_CRON_FIELD_DEF(tm_field_) (K_CRON_CALC_FIELD_DEFS[CRON_CALC_TM_CRON_FIELD(tm_field_)])
#define CRON_CALC_TM_FIELD(tm_val_, tm_field_) (int*)((uint8_t*)(tm_val_) + K_CRON_CALC_TM_FIELDS[(int)(tm_field_)].tm_offset)
#define CRON_CALC_TM_FIELD_MIN(tm_field_) (CRON_CALC_TM_CRON_FIELD_DEF(tm_field_).min)
#define CRON_CALC_TM_FIELD_MAX(tm_field_) (CRON_CALC_TM_CRON_FIELD_DEF(tm_field_).max)

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

bool cron_calc_matches(const cron_calc* self, cron_calc_field field, int val)
{
    uint64_t field_value =
        field == CRON_CALC_FIELD_YEARS ? self->years :
        field == CRON_CALC_FIELD_MONTHS ? self->months :
        field == CRON_CALC_FIELD_DAYS ? self->days :
        field == CRON_CALC_FIELD_HOURS ? self->hours :
        field == CRON_CALC_FIELD_MINUTES ? self->minutes :
        field == CRON_CALC_FIELD_SECONDS ? self->seconds :
        field == CRON_CALC_FIELD_WDAYS ? self->weekDays : 0;

    return (field_value & (uint64_t) val);
}

/* ---------------------------------------------------------------------------- */

bool cron_calc_find_next(const cron_calc* self, struct tm* tm_val, cron_calc_tm_level level)
{
    int* fld = CRON_CALC_TM_FIELD(tm_val, level);
    const cron_calc_field_def* cron_fld = K_CRON_CALC_TM_FIELDS[level].cron_field;
    for (; *fld < CRON_CALC_TM_FIELD_MIN(level); *fld++)
    {
        if (cron_calc_matches(self, CRON_CALC_TM_CRON_FIELD(level), *fld))
        {
            cron_calc_rollover(level+1);
            match = true;
            break;
        }
    }
}

/* ---------------------------------------------------------------------------- */

time_t cron_calc_next(const cron_calc* self, time_t after)
{
    struct tm* tm_after;
    time_t start = after + 1;
    cron_calc next = { 0 };

    if (!self)
    {
    return (time_t) 0;
}

    tm_after = gmtime(&start);
    if (!tm_after)
    {
        return (time_t) 0;
    }

    next.years = tm_after->tm_year + 1900;
    next.months = tm_after->tm_mon + 1;
    next.days = tm_after->tm_mday;
    next.hours = tm_after->tm_hour;
    next.minutes = tm_after->tm_min;
    next.seconds = tm_after->tm_sec;
    next.weekDays = tm_after->tm_wday;

    tm_level = YEAR;
    while (tm_level <= SECOND)
    {
        bool match = false;

        if (cron_calc_matches(tm_level))
        {
            tm_level++;
            continue;
        }



        /* find match on current level */


        if (match)
        {
            tm_level++;
            continue;
        }

        /* current level should rollover,
         * but since parent needs to be incremented - do it and
         * all children will be rolled over, including this one */
        tm_level--;
    };

    memset(tm_after, 0, sizeof(struct tm));
    tm_after->tm_year = next.years - 1900;
    tm_after->tm_mon = next.months - 1;
    tm_after->tm_mday = next.days;
    tm_after->tm_hour = next.hours;
    tm_after->tm_min = next.minutes;
    tm_after->tm_sec = next.seconds;

    return mktime(tm_after);
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
