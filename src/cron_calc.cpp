// Copyright (c) 2018-2019 Sergey Burnevsky (sergey.burnevsky @ gmail.com)
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#ifndef CRON_CALC_NO_EXCEPT
#include <list>
#define CRON_CALC_USE_STDLIST
#else
#error "TODO: support for simple list implementation"
#endif

#include "cron_calc.hpp"

// ----------------------------------------------------------------------------

#ifdef CRON_CALC_USE_STDLIST

typedef std::list<cron_calc> CronCalcList;

class CronCalcImpl
{
public:
    CronCalcList mList;
};

#endif

// ----------------------------------------------------------------------------

CronCalc::CronCalc() :
    mPimpl(new CronCalcImpl)
{
}

// ----------------------------------------------------------------------------

CronCalc::~CronCalc()
{
    delete mPimpl;
}

// ----------------------------------------------------------------------------

cron_calc_error CronCalc::addRule(const char* expr, cron_calc_option_mask options, const char** err_location)
{
    cron_calc cc;
    cron_calc_error err = cron_calc_parse(&cc, expr, options, err_location);
    if (err == CRON_CALC_OK)
    {
        mPimpl->mList.push_back(cc);
    }
    return err;
}

// ----------------------------------------------------------------------------

cron_calc_error CronCalc::addRule(const char* expr)
{
    const char* err_location = NULL;
    return addRule(expr, CRON_CALC_OPT_DEFAULT, &err_location);
}

// ----------------------------------------------------------------------------

time_t CronCalc::next(time_t after) const
{
    time_t earliest = CRON_CALC_INVALID_TIME;
    for (CronCalcList::const_iterator iter = mPimpl->mList.begin();
         iter != mPimpl->mList.end(); ++iter)
    {
        time_t next = cron_calc_next(&(*iter), after);
        if (next != CRON_CALC_INVALID_TIME &&
           (earliest == CRON_CALC_INVALID_TIME || next < earliest))
        {
            earliest = next;
        }
    }

    return earliest;
}
