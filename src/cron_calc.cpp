// Copyright (c) 2018-2019 Sergey Burnevsky (sergey.burnevsky @ gmail.com)
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#ifndef CRON_CALC_NO_EXCEPT
#include <list>
#else
#include <memory>
#endif

#include "cron_calc.hpp"

// ----------------------------------------------------------------------------

#ifndef CRON_CALC_NO_EXCEPT

typedef std::list<cron_calc> CronCalcList;

class CronCalcImpl
{
public:
    cron_calc_error push_back(const cron_calc& cc)
    {
        mList.push_back(cc);
        return CRON_CALC_OK;
    }

    typedef CronCalcList::iterator Iter;
    typedef CronCalcList::const_iterator ConstIter;

    ConstIter begin() const { return mList.begin(); }
    ConstIter end() const { return mList.end(); }

    CronCalcList mList;
};

#else // CRON_CALC_NO_EXCEPT

class CronCalcImpl
{
    struct ListItem
    {
        ListItem() : mNext(NULL)
        {
            memset(&mCc, 0, sizeof(cron_calc));
        }

        cron_calc mCc;
        ListItem* mNext;
    };

public:
    CronCalcImpl() : mHead(NULL)
    {
    }

    ~CronCalcImpl()
    {
        for (ListItem* item = mHead; item != NULL; )
        {
            ListItem* next = item->mNext;
            delete item;
            item = next;
        }
    }

    cron_calc_error push_back(const cron_calc& cc)
    {
        ListItem* item = new (std::nothrow) ListItem;
        if (!item) return CRON_CALC_ERROR_OOM;

        item->mCc = cc;

        if (!mHead)
        {
            mHead = item;
        }
        else
        {
            ListItem* parent = mHead;
            for (; parent->mNext != NULL; parent = parent->mNext);
            parent->mNext = item;
        }
        return CRON_CALC_OK;
    }

    struct ConstIter
    {
        ConstIter(ListItem* it) : item(it) {}
        const cron_calc& operator*() const { return item->mCc; }
        ConstIter& operator++() { item = item->mNext; return *this; }
        bool operator!=(const ConstIter& rhs) const { return item != rhs.item; }
    private:
        ListItem* item;
    };

    ConstIter begin() const { return ConstIter(mHead); }
    ConstIter end() const { return ConstIter(NULL); }

private:
    ListItem* mHead;
};

#endif

// ----------------------------------------------------------------------------

CronCalc::CronCalc() :
#ifndef CRON_CALC_NO_EXCEPT
    mPimpl(new CronCalcImpl)
#else
    mPimpl(new (std::nothrow) CronCalcImpl)
#endif
{
}

// ----------------------------------------------------------------------------

CronCalc::~CronCalc()
{
    delete mPimpl;
}

// ----------------------------------------------------------------------------

#ifdef CRON_CALC_NO_EXCEPT
    #define RET_UNLESS_INIT(ret_) if (!mPimpl) return (ret_);
#else
    #define RET_UNLESS_INIT(ret_)
#endif

// ----------------------------------------------------------------------------

cron_calc_error CronCalc::addRule(const char* expr, cron_calc_option_mask options, const char** err_location)
{
    RET_UNLESS_INIT(CRON_CALC_ERROR_OOM);

    cron_calc cc;
    cron_calc_error err = cron_calc_parse(&cc, expr, options, err_location);

    return err ? err : mPimpl->push_back(cc);
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
    RET_UNLESS_INIT(CRON_CALC_INVALID_TIME);

    time_t earliest = CRON_CALC_INVALID_TIME;
    for (CronCalcImpl::ConstIter iter = mPimpl->begin(); iter != mPimpl->end(); ++iter)
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
