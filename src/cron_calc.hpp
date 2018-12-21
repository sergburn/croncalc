// Copyright (c) 2018 Sergey Burnevsky (sergey.burnevsky @ gmail.com)
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#ifndef CRON_CALC_HPP_
#define CRON_CALC_HPP_

#include "cron_calc.h"

/**
 * C++ wrapper interface for cron_calc C API
 */
class CronCalc
{
public:
    CronCalc();

    /**
     * Wrapper for cron_calc_parse().
     * Stores result of parsing internally.
     * @see cron_calc_parse().
     */
    cron_calc_error parse(const char* expr, cron_calc_option_mask options, const char** err_location);

    /**
     * Wrapper for cron_calc_next().
     * Does not modify this object.
     * @see cron_calc_next().
     */
    time_t next(time_t after) const;

    friend bool operator==(const CronCalc& cron1, const CronCalc& cron2);

private:
    cron_calc mSelf;
};

#endif // CRON_CALC_HPP_
