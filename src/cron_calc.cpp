// Copyright (c) 2018 Sergey Burnevsky (sergey.burnevsky @ gmail.com)
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include <string.h>
#include "cron_calc.hpp"

// ----------------------------------------------------------------------------

CronCalc::CronCalc()
{
    memset(&mSelf, 0, sizeof mSelf);
}

// ----------------------------------------------------------------------------

bool operator==(const CronCalc& cron1, const CronCalc& cron2)
{
    return cron_calc_is_same(&cron1.mSelf, &cron2.mSelf);
}

// ----------------------------------------------------------------------------

cron_calc_error CronCalc::parse(const char* expr, cron_calc_option_mask options, const char** err_location)
{
    return cron_calc_parse(&mSelf, expr, options, err_location);
}

// ----------------------------------------------------------------------------

time_t CronCalc::next(time_t after) const
{
    return cron_calc_next(&mSelf, after);
}
