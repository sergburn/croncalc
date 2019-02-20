// Copyright (c) 2018-2019 Sergey Burnevsky (sergey.burnevsky @ gmail.com)
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#ifndef CRON_CALC_HPP_
#define CRON_CALC_HPP_

#include "cron_calc.h"

class CronCalcImpl;

/**
 * C++ wrapper interface for cron_calc C API
 */
class CronCalc
{
public:
    CronCalc();
    ~CronCalc();

    /**
     * Adds another Cron expression to this container.
     * Following calls to next() will take it into account, if this call succeeds.
     *
     * @see cron_calc_parse() for details on arguments and return values.
     */
    cron_calc_error addRule(const char* expr, cron_calc_option_mask options, const char** err_location);

    /**
     * Adds another Cron expression with default options to this container.
     * This short-cutting overload calls main method with CRON_CALC_OPT_DEFAULT option.
     */
    cron_calc_error addRule(const char* expr);

    /**
     * Calculates next time instant with regards to given reference time.
     * Checks all rules added with addRule() and picks the earliest matching time.
     * If no rules have been added so far, CRON_CALC_INVALID_TIME.
     * @see cron_calc_next() for details on arguments and return values.
     * @return CRON_CALC_INVALID_TIME Also if no rules have been added yet.
     */
    time_t next(time_t after) const;

private:
    CronCalc(const CronCalc&);
    const CronCalc& operator=(const CronCalc&);

private:
    CronCalcImpl* mPimpl;
};

#endif // CRON_CALC_HPP_
