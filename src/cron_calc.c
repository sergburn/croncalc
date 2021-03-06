/*
 * Copyright (c) 2018 Sergey Burnevsky (sergey.burnevsky @ gmail.com)
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <string.h>
#include <ctype.h>
#include <stddef.h>

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
    CRON_CALC_YEAR_COUNT = 64,
    CRON_CALC_YEAR_END = CRON_CALC_YEAR_START + CRON_CALC_YEAR_COUNT - 1,
    CRON_CALC_YEAR_MAX = (sizeof(time_t) > 4) ? 3000 : 2038,
    /* years limited at 3000 to avoid too long operation if
     * cron_calc_next() is called with improperly initialized object */

    CRON_CALC_OPT_MDAY_STARRED = CRON_CALC_OPT_RESERVED_40,
    CRON_CALC_OPT_WDAY_STARRED = CRON_CALC_OPT_RESERVED_80,

    CRON_CALC_LAST_CODE = INT32_MAX,
};

static const char* const CRON_CALC_DAYS[] = {
    "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"
};

static const char* const CRON_CALC_MONTHS[] = {
    "", "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
};

static const int CRON_CALC_MONTH_LENGTHS[] = {
    0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31  /* Feb-29 is handled separately */
};

enum
{
    CRON_CALC_DAYS_NUM = sizeof(CRON_CALC_DAYS) / sizeof CRON_CALC_DAYS[0],
    CRON_CALC_MONTHS_NUM = sizeof(CRON_CALC_MONTHS) / sizeof CRON_CALC_MONTHS[0]
};

typedef struct cron_calc_field_def
{
    uint32_t min;
    uint32_t max;
    const char* const* names;
    uint32_t names_count;
} cron_calc_field_def;

static const cron_calc_field_def K_CRON_CALC_FIELD_DEFS[CRON_CALC_FIELD_LAST + 1] = {
    { 0, 59 },                                                  /* CRON_CALC_FIELD_SECONDS */
    { 0, 59 },                                                  /* CRON_CALC_FIELD_MINUTES */
    { 0, 23 },                                                  /* CRON_CALC_FIELD_HOURS */
    { 1, 31 },                                                  /* CRON_CALC_FIELD_DAYS */
    { 1, 12,    CRON_CALC_MONTHS,   CRON_CALC_MONTHS_NUM },     /* CRON_CALC_FIELD_MONTHS */
    { 0,  7,    CRON_CALC_DAYS,     CRON_CALC_DAYS_NUM },       /* CRON_CALC_FIELD_WDAYS */
    { CRON_CALC_YEAR_START, CRON_CALC_YEAR_END },               /* CRON_CALC_FIELD_YEARS */
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

/* this trick with array of masks is only for "fun":
 * a) avoid get_mask(level) function, which cannot be fully test-covered
 * b) ok, it also reduces amount of conditions... but who would care...
 *
 * Only months, minutes and seconds need to be stored there,
 * so its length is shorter than one would guess.
 */
typedef uint64_t cron_calc_mask_array[CRON_CALC_TM_WDAY];

typedef struct cron_calc_tm_field_def
{
    size_t tm_offset;
    uint32_t min;
    uint32_t max;
} cron_calc_tm_field_def;

static const cron_calc_tm_field_def K_CRON_CALC_TM_FIELDS[CRON_CALC_FIELD_LAST + 1] = {
    { offsetof(struct tm, tm_year), CRON_CALC_YEAR_START, CRON_CALC_YEAR_END },
    { offsetof(struct tm, tm_mon),  1, 12 },
    { offsetof(struct tm, tm_mday), 1, 31 }, /* max is handled differently */
    { offsetof(struct tm, tm_hour), 0, 23 },
    { offsetof(struct tm, tm_min),  0, 59 },
    { offsetof(struct tm, tm_sec),  0, 59 },
    { offsetof(struct tm, tm_wday), 0,  6 } /* differs from parser range */
};

/* ---------------------------------------------------------------------------- */

#define CRON_CALC_IS_DIGIT(a_) ((a_) >= '0' && (a_) <= '9')
#define CRON_CALC_IS_NAME_CHAR(a_) (((a_) >= 'A' && (a_) <= 'Z') || ((a_) >= 'a' && (a_) <= 'z'))

#define CRON_CALC_MASK(a_) ((uint64_t)1 << (a_))
#define CRON_CALC_FIELD_MIN(field_) (K_CRON_CALC_FIELD_DEFS[field_].min)
#define CRON_CALC_FIELD_MAX(field_) (K_CRON_CALC_FIELD_DEFS[field_].max)

#define CRON_CALC_TM_FIELD(tm_val_, tm_field_) (int*)((uint8_t*)(tm_val_) + K_CRON_CALC_TM_FIELDS[(int)(tm_field_)].tm_offset)
#define CRON_CALC_TM_FIELD_MIN(tm_field_) (K_CRON_CALC_TM_FIELDS[tm_field_].min)
#define CRON_CALC_TM_FIELD_MAX(tm_field_) (K_CRON_CALC_TM_FIELDS[tm_field_].max)

#define CRON_CALC_MATCHES_MASK(val_, mask_) ((mask_) & ((uint64_t)1 << (val_)))

/* ---------------------------------------------------------------------------- */
/* ---------------------------------------------------------------------------- */

static bool cron_calc_is_leap_year(int year)
{
    return
        year % 400 == 0 ? true :
        year % 100 == 0 ? false :
        year % 4 == 0 ? true :
        false;
}

/* ---------------------------------------------------------------------------- */

static int cron_calc_month_days(int year, int month)
{
    if (month == 2)
    {
        return cron_calc_is_leap_year(year) ? 29 : 28;
    }
    else
    {
        return CRON_CALC_MONTH_LENGTHS[month];
    }
}

/* ---------------------------------------------------------------------------- */

static int cron_calc_get_week_day(int year, int month, int day)
{
    int corr = (14 - month) / 12;
    int y = year - corr;
    int m = month + 12 * corr - 2;
    return (day + y + y / 4 - y / 100 + y / 400 + 31 * m / 12) % 7;
}

/* ---------------------------------------------------------------------------- */
/* ---------------------------------------------------------------------------- */

static cron_calc_error cron_calc_set_field(
    cron_calc* self,
    uint32_t min,
    uint32_t max,
    uint32_t step,
    bool is_star,
    cron_calc_field field)
{
    int i;
    uint64_t value = 0;
    uint32_t shift = (field == CRON_CALC_FIELD_YEARS) ? CRON_CALC_YEAR_START : 0;

#ifndef CRON_CALC_WITH_COVERAGE /* never happens */
    if (step < 1)
    {
        return CRON_CALC_ERROR_NUMBER_RANGE;
    }
#endif
    if (max < min)
    {
        return CRON_CALC_ERROR_NUMBER_RANGE;
    }

    if (max != CRON_CALC_LAST_CODE)
    {
        for (i = min; i <= max; i += step)
        {
            value |= CRON_CALC_MASK(i - shift);
        }
    }

    /* ranges checked at parsing */
    switch (field)
    {
        case CRON_CALC_FIELD_SECONDS:
            self->seconds |= value;
            break;
        case CRON_CALC_FIELD_MINUTES:
            self->minutes |= value;
            break;
        case CRON_CALC_FIELD_HOURS:
            self->hours |= value;
            break;
        case CRON_CALC_FIELD_DAYS:
            if (max == CRON_CALC_LAST_CODE)
            {
                value = CRON_CALC_MASK(0);
            }
            self->days |= value;
            self->options |= is_star ? CRON_CALC_OPT_MDAY_STARRED : 0;
            break;
        case CRON_CALC_FIELD_MONTHS:
            self->months |= value;
            break;
        case CRON_CALC_FIELD_WDAYS:
            if (value & CRON_CALC_MASK(7)) /* SUN=7 is allowed, but it's the same as SUN=0 */
            {
                value |= CRON_CALC_MASK(0);
                value &= ~CRON_CALC_MASK(7);
            }
            self->weekDays |= value;
            self->options |= is_star ? CRON_CALC_OPT_WDAY_STARRED : 0;
            break;
        case CRON_CALC_FIELD_YEARS:
            self->years |= value;
            break;
    }
    return CRON_CALC_OK;
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
        if ((fname[0] == name[0] || fname[0] == name[0] - CRON_CALC_NAME_UPCASE) &&
            (fname[1] == name[1] || fname[1] == name[1] - CRON_CALC_NAME_UPCASE) &&
            (fname[2] == name[2] || fname[2] == name[2] - CRON_CALC_NAME_UPCASE))
        {
            *value = i;
            *pp = p;
            return CRON_CALC_OK;
        }
    }
    return CRON_CALC_ERROR_INVALID_NAME;
}

/* ---------------------------------------------------------------------------- */

static cron_calc_error cron_calc_parse_value(const char** pp, uint32_t* value, cron_calc_field field)
{
    const cron_calc_field_def* field_def = &K_CRON_CALC_FIELD_DEFS[field];
    if (field_def->names_count && CRON_CALC_IS_NAME_CHAR(**pp))
    {
        return cron_calc_parse_name(pp, value, field_def);
    }
    return cron_calc_parse_number(pp, value, field_def);
}

/* ---------------------------------------------------------------------------- */

static bool cron_calc_is_impossible(cron_calc* self)
{
    /* Impossible dates:
     * 1) 31st of a 30-day month (it's allowed by parser)
     * 2) 30th of February
     * 3) 29th of February in non-leap year
     */

    if (self->options & CRON_CALC_OPT_WITH_YEARS)
    {
        /* Feb-29, but non-leap years only */
        if (self->months == CRON_CALC_MASK(2) &&
            self->days == CRON_CALC_MASK(29))
        {
            /* check that no leap year is allowed */
            uint32_t y = 0;
            for (; y < CRON_CALC_YEAR_COUNT; y++)
            {
                if ((CRON_CALC_MASK(y) & self->years) &&
                    cron_calc_is_leap_year(y + CRON_CALC_YEAR_START))
                {
                    break;
                }
            }
            if (y == CRON_CALC_YEAR_COUNT)
            {
                return true;
            }
        }
    }

    /* Only 31st day is allowed, but ... */
    if (self->days == CRON_CALC_MASK(31))
    {
        /* ... no 31-day month allowed */
        const uint64_t long_months =
            CRON_CALC_MASK(1) | CRON_CALC_MASK(3) | CRON_CALC_MASK(5) |
            CRON_CALC_MASK(7) | CRON_CALC_MASK(8) | CRON_CALC_MASK(10) | CRON_CALC_MASK(12);
        if ((self->months & long_months) == 0)
        {
            return true;
        }
    }

    /* The only month is February, but ...  */
    if (self->months == CRON_CALC_MASK(2))
    {
        /* ... only days 30-31 allowed */
        const uint64_t feb_days = ~(CRON_CALC_MASK(30) | CRON_CALC_MASK(31));
        if ((self->days & feb_days) == 0)
        {
            return true;
        }
    }

    return false;
}

/* ---------------------------------------------------------------------------- */

cron_calc_error cron_calc_parse(
    cron_calc* self,
    const char* expr,
    cron_calc_option_mask options,
    const char** err_location)
{
    cron_calc_error err = CRON_CALC_OK;
    const char* p = expr;
    cron_calc_field field = CRON_CALC_FIELD_SECONDS;
    cron_calc_field last_field = CRON_CALC_FIELD_YEARS;

    if (err_location)
    {
        *err_location = NULL;
    }

    if (!self || !expr)
    {
        return CRON_CALC_ERROR_ARGUMENT;
    }

    memset(self, 0, sizeof *self);
    self->options = options;

    if (!(options & CRON_CALC_OPT_WITH_SECONDS))
    {
        cron_calc_set_field(self, /* match only at seconds 00 */
            CRON_CALC_FIELD_MIN(CRON_CALC_FIELD_SECONDS),
            CRON_CALC_FIELD_MIN(CRON_CALC_FIELD_SECONDS),
            1, true,
            CRON_CALC_FIELD_SECONDS);
        field = CRON_CALC_FIELD_MINUTES;
    }

    if (!(options & CRON_CALC_OPT_WITH_YEARS))
    {
        /* any year will match, even outside of range */
        last_field = CRON_CALC_FIELD_WDAYS;
    }

    while (field <= last_field)
    {
        uint32_t min = 0, max = 0, step = 1;
        bool is_range = false, is_star = false;

        if (*p == '*')
        {
            min = CRON_CALC_FIELD_MIN(field);
            max = CRON_CALC_FIELD_MAX(field);
            is_range = is_star = true;
            p++;
        }
        else if (*p == 'L')
        {
            if (field == CRON_CALC_FIELD_DAYS)
            {
                min = max = CRON_CALC_LAST_CODE;
                p++;
            }
            else
            {
                err = CRON_CALC_ERROR_NUMBER_EXPECTED;
                break;
            }
        }
        else
        {
            err = cron_calc_parse_value(&p, &min, field);
            if (err) break;

            if (*p == '-') /* max will follow */
            {
                p++;
                err = cron_calc_parse_value(&p, &max, field);
                if (err) break;
                is_range = true;
            }
            else
            {
                max = min;
            }
        }

        if (is_range && (*p == '/')) /* step will follow */
        {
            p++;
            err = cron_calc_parse_limited_number(&p, &step, 1, CRON_CALC_FIELD_MAX(field));
            if (err) break;
        }

        if (*p == 0 || isspace(*p) || *p == ',') /* end of field */
        {
            err = cron_calc_set_field(self, min, max, step, is_star, field);
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
                field++;
                break;
            }
        }
        else
        {
            err = CRON_CALC_ERROR_FIELD_FORMAT;
            break;
        }
    }

    if (!err)
    {
        if (field <= last_field)
        {
            err = CRON_CALC_ERROR_EXPR_SHORT;
        }
        else if (*p != 0) /* unexpected data in the expression */
        {
            err = CRON_CALC_ERROR_EXPR_LONG;
        }
    }

    if (!err && cron_calc_is_impossible(self))
    {
        err = CRON_CALC_ERROR_IMPOSSIBLE_DATE;
    }

    if (err && err_location)
    {
        *err_location = p;
    }

    return err;
}

/* ---------------------------------------------------------------------------- */
/* ---------------------------------------------------------------------------- */

static bool cron_calc_find_next(
    const cron_calc* self,
    struct tm* tm_val,
    const cron_calc_mask_array mask_array,
    cron_calc_tm_level level,
    bool rollover);

/* ---------------------------------------------------------------------------- */

static bool cron_calc_find_next_day(
    const cron_calc* self,
    struct tm* tm_val,
    const cron_calc_mask_array masks,
    bool rollover)
{
    const int month_len = cron_calc_month_days(tm_val->tm_year, tm_val->tm_mon);
    /* crontab(5): If both fields are restricted (i.e., do not contain the "*" character),
     * the command will be run when _either_ field matches the current time. */
    const bool either = !(self->options & (CRON_CALC_OPT_MDAY_STARRED | CRON_CALC_OPT_WDAY_STARRED));
    const uint64_t week_days = self->weekDays;
    uint64_t days = self->days;
    /* Bit 0 is set if last day of month should match also */
    if (CRON_CALC_MATCHES_MASK(0, days))
    {
        days |= CRON_CALC_MASK(month_len);
    }

    if (rollover)
    {
        tm_val->tm_mday = CRON_CALC_TM_FIELD_MIN(CRON_CALC_TM_DAY);
        tm_val->tm_wday = cron_calc_get_week_day(tm_val->tm_year, tm_val->tm_mon, tm_val->tm_mday);
    }

    for (; tm_val->tm_mday <= month_len; tm_val->tm_mday++, rollover = true)
    {
        const bool mday_matches = CRON_CALC_MATCHES_MASK(tm_val->tm_mday, days);
        const bool wday_matches = CRON_CALC_MATCHES_MASK(tm_val->tm_wday, week_days);

        const bool matches = either ? mday_matches || wday_matches : mday_matches && wday_matches;

        if (matches && cron_calc_find_next(self, tm_val, masks, CRON_CALC_TM_HOUR, rollover))
        {
            return true;
        }
        tm_val->tm_wday = (tm_val->tm_wday + 1) % 7;
    }
    return false;
}

/* ---------------------------------------------------------------------------- */

static bool cron_calc_find_next_year(
    const cron_calc* self,
    struct tm* tm_val,
    const cron_calc_mask_array masks)
{
    bool rollover = false;
    const bool any_year = !(self->options & CRON_CALC_OPT_WITH_YEARS);
    const int val_max = any_year ? CRON_CALC_YEAR_MAX : CRON_CALC_YEAR_END;

    for (; tm_val->tm_year <= val_max; tm_val->tm_year++, rollover = true)
    {
        if ((any_year || CRON_CALC_MATCHES_MASK(tm_val->tm_year - CRON_CALC_YEAR_START, self->years)) &&
            cron_calc_find_next(self, tm_val, masks, CRON_CALC_TM_MONTH, rollover))
        {
            return true;
        }
    }
    return false;
}

/* ---------------------------------------------------------------------------- */

static bool cron_calc_find_next(
    const cron_calc* self,
    struct tm* tm_val,
    const cron_calc_mask_array masks,
    cron_calc_tm_level level,
    bool rollover)
{
    const uint64_t mask = masks[level];
    const int val_min = CRON_CALC_TM_FIELD_MIN(level);
    const int val_max = CRON_CALC_TM_FIELD_MAX(level);

    int* fld = CRON_CALC_TM_FIELD(tm_val, level);
    int val = rollover ? val_min : *fld;
    bool found = false;

    /* if no match on this level, it has to be incremented
     * and therefore all levels downwards have to roll over and start from minimum.
     * if seconds not specified in the expression, its mask is set to 1,
     * which yeilds match on the first iteration of this loop (only after rollover though)
     */
    for (; !found && (val <= val_max); val++, rollover = true)
    {
        if (CRON_CALC_MATCHES_MASK(val, mask))
        {
            *fld = val;

            if (level == CRON_CALC_TM_MONTH)
            {
                found = cron_calc_find_next_day(self, tm_val, masks, rollover);
            }
            else if (level == CRON_CALC_TM_SECOND)
            {
                found = true;
            }
            else
            {
                found = cron_calc_find_next(self, tm_val, masks, level + 1, rollover);
            }
        }
    }
    return found;
}

/* ---------------------------------------------------------------------------- */

time_t cron_calc_next(const cron_calc* self, time_t after)
{
    struct tm tm_buf = { 0 };
    struct tm* tm_after = NULL;
    time_t start = after + 1;
    cron_calc_mask_array masks = { 0 };

    if (!self)
    {
        return CRON_CALC_INVALID_TIME;
    }

    /* try to check that this object was initialized correctly before this call.
     * All fields (except years) must be non-0, although this is not a 100%-proof method. */
    if (!self->options ||
        !self->months || !self->days || !self->weekDays ||
        !self->hours || !self->minutes || !self->seconds)
    {
        return CRON_CALC_INVALID_TIME;
    }
    if ((self->options & CRON_CALC_OPT_WITH_YEARS) && !self->years)
    {
        return CRON_CALC_INVALID_TIME;
    }

    masks[CRON_CALC_TM_MONTH] = self->months;
    masks[CRON_CALC_TM_HOUR] = self->hours;
    masks[CRON_CALC_TM_MINUTE] = self->minutes;
    masks[CRON_CALC_TM_SECOND] = self->seconds;
    /* other masks are taken from self */

#if defined(_POSIX_C_SOURCE)
    tm_after = localtime_r(&start, &tm_buf);
#elif defined (_MSC_VER)
    tm_after = localtime_s(&start, &tm_buf);
#else
    tm_after = localtime(&start);
    if (tm_after)
    {
        tm_buf = *tm_after;
    }
#endif
    if (!tm_after)
    {
        return CRON_CALC_INVALID_TIME;
    }

    /* adjust tm values */
    tm_buf.tm_year += 1900;
    tm_buf.tm_mon += 1;

    if (!cron_calc_find_next_year(self, tm_after, masks))
    {
        return CRON_CALC_INVALID_TIME;
    }

    /* restore to tm definitions */
    tm_buf.tm_year -= 1900;
    tm_buf.tm_mon -= 1;
    tm_buf.tm_isdst = -1;

    return mktime(tm_after); /* CRON_CALC_INVALID_TIME in case of error */
}

/* ---------------------------------------------------------------------------- */

bool cron_calc_is_same(const cron_calc* left, const cron_calc* right)
{
    return
        left->seconds == right->seconds &&
        left->minutes == right->minutes &&
        left->hours == right->hours &&
        left->days == right->days &&
        left->weekDays == right->weekDays &&
        left->months == right->months &&
        left->years == right->years &&
        left->options == right->options;
}
