#include "Queue_Controller.h"

#include <string.h>
#include <stack>
#include <vector>
#include <iostream>
#include <algorithm>

#include <fplog.h>


static const size_t buf_sz = -1;

Queue_Controller::Queue_Controller(size_t size_limit, size_t timeout):
max_size_(size_limit),
emergency_time_trigger_(timeout),
timer_start_(chrono::milliseconds(0))
{
    algo_ = make_shared<Remove_Oldest_Below_Priority>(*this, fplog::Prio::warning);

    algo_fallback_ = make_shared<Remove_Oldest>(*this);
    //Remove_Newest and Remove_Oldest could not fail to remove items in normal conditions,
    //however other algos could fail to remove items if conditions of removal are not fully met.
}

bool Queue_Controller::empty()
{
   return mq_.empty();
}

string *Queue_Controller::front()
{
    return mq_.front();
}

void Queue_Controller::pop()
{
    string* str = mq_.front();
    
    mq_.pop();

    if (!str)
        return;

    #ifdef _LINUX
    int buf_length = strnlen(str->c_str(), buf_sz);
    #else
    int buf_length = strnlen_s(str->c_str(), buf_sz);
    #endif

    mq_size_ -= buf_length;
}

void Queue_Controller::push(string *str)
{   
    if (!str)
        return;

    #ifdef _LINUX
    int buf_length = strnlen(str->c_str(), buf_sz);
    #else
    int buf_length = strnlen_s(str->c_str(), buf_sz);
    #endif

    if (state_of_emergency())
        handle_emergency();

    mq_.push(str);
    mq_size_ += buf_length;
}

bool Queue_Controller::state_of_emergency()
{
    size_t timeout = emergency_time_trigger_;

    auto check_time_out = [&timeout, this]()
    {
        unsigned long long int mult = 1;
        
        #ifdef _LINUX
        mult = 100;
        #endif
        
        time_point<system_clock, system_clock::duration> timer_stop(system_clock::now());
        system_clock::duration converted_timeout(static_cast<unsigned long long>(timeout) * 10000 * mult);
        if (timer_stop - this->timer_start_ >= converted_timeout)
            throw std::out_of_range("emergency timout!");
    };

    if (mq_size_ > max_size_)
    {
        if (timer_start_ == time_point<system_clock, system_clock::duration>(chrono::milliseconds(0)))
            timer_start_ = system_clock::now();

        try
        {
            check_time_out();
        }
        catch (std::out_of_range&)
        {
            return true;
        }
    }
    else
        timer_start_ = time_point<system_clock, system_clock::duration>(chrono::milliseconds(0));

    return false;
}

void Queue_Controller::handle_emergency()
{
    Algo::Result res = algo_->process_queue(mq_size_);
    mq_size_ = res.current_size;

    if (state_of_emergency())
        res = algo_fallback_->process_queue(mq_size_);

    mq_size_ = res.current_size;
}

Queue_Controller::Algo::Result Queue_Controller::Remove_Oldest::process_queue(size_t current_size)
{
    Result res;
    res.current_size = 0;
    res.removed_count = 0;
    
    int cs = current_size;
    
    while (cs >= max_size_)
    {
        if (mq_.empty())
            break;
            
        string* str = mq_.front();
        mq_.pop();

        if (str)
        {
            #ifdef _LINUX
            int buf_length = strnlen(str->c_str(), buf_sz);
            #else
            int buf_length = strnlen_s(str->c_str(), buf_sz);
            #endif
        
            cs -= buf_length;
        }

        res.removed_count++;

        delete str;
    }
    
    if (cs < 0)
        cs = 0;

    res.current_size = cs;
    return res;
}

Queue_Controller::Algo::Result Queue_Controller::Remove_Newest::process_queue(size_t current_size)
{
    Result res;
    res.current_size = 0;
    res.removed_count = 0;
    
    int cs = current_size;

    vector<string*> v;
    while (!mq_.empty())
    {
        v.push_back(mq_.front());
        mq_.pop();
    }

    while (cs >= max_size_)
    {
        if (v.empty())
            break;

        string* str = v.back();
        v.pop_back();

        if (str)
        {
            #ifdef _LINUX
            int buf_length = strnlen(str->c_str(), buf_sz);
            #else
            int buf_length = strnlen_s(str->c_str(), buf_sz);
            #endif
        
            cs -= buf_length;
        }

        res.removed_count++;

        delete str;
    }

    for (vector<string*>::iterator it(v.begin()); it != v.end(); ++it)
    {
        mq_.push(*it);
    }

    if (cs < 0)
        cs = 0;

    res.current_size = cs;
    return res;
}

void Queue_Controller::Remove_Oldest_Below_Priority::make_filter()
{
    filter_ = std::make_shared<fplog::Priority_Filter>("Remove_Oldest_Below_Priority");
    filter_->add_all_below(prio_.c_str(), inclusive_);
}

Queue_Controller::Algo::Result Queue_Controller::Remove_Oldest_Below_Priority::process_queue(size_t current_size)
{
    Result res;
    res.current_size = 0;
    res.removed_count = 0;
    
    std::queue<std::string*> mq;
    
    int cs = current_size;
    
    while (!mq_.empty())
    {
        string* str = mq_.front();

        if (str)
        {
            if (cs >= max_size_)
            {
                #ifdef _LINUX
                int buf_length = strnlen(str->c_str(), buf_sz);
                #else
                int buf_length = strnlen_s(str->c_str(), buf_sz);
                #endif

                fplog::Message msg(*str);
                if (filter_->should_pass(msg))
                {
                    cs -= buf_length;
                    res.removed_count++;
                    delete str;
                }
                else
                    mq.push(str);
            }
            else
                mq.push(str);
        }
        
        mq_.pop();
    }

    mq_ = mq;
    
    if (cs < 0)
        cs = 0;

    res.current_size = cs;
    return res;
}

void Queue_Controller::Remove_Newest_Below_Priority::make_filter()
{
    filter_ = std::make_shared<fplog::Priority_Filter>("Remove_Newest_Below_Priority");
    filter_->add_all_below(prio_.c_str(), inclusive_);
}

Queue_Controller::Algo::Result Queue_Controller::Remove_Newest_Below_Priority::process_queue(size_t current_size)
{
    Result res;
    res.current_size = 0;
    res.removed_count = 0;
    
    std::vector<std::string*> v;
    
    for (std::string* str = 0; !mq_.empty(); mq_.pop())
    {
        str = mq_.front();
        if (!str)
            continue;
        v.push_back(str);
    }

    for (std::vector<std::string*>::reverse_iterator it(v.rbegin()); it != v.rend(); ++it)
        mq_.push(*it);

    {
        std::vector<std::string*> empty;
        v.swap(empty);
    }

    int cs = current_size;
    
    while (!mq_.empty())
    {
        string* str = mq_.front();

        if (str)
        {

            if (cs >= max_size_)
            {
                #ifdef _LINUX
                int buf_length = strnlen(str->c_str(), buf_sz);
                #else
                int buf_length = strnlen_s(str->c_str(), buf_sz);
                #endif

                fplog::Message msg(*str);
                if (filter_->should_pass(msg))
                {
                    cs -= buf_length;
                    res.removed_count++;
                    delete str;
                }
                else
                    v.push_back(str);
            }
            else
                v.push_back(str);
        }

        mq_.pop();
    }

    for (std::vector<std::string*>::reverse_iterator it(v.rbegin()); it != v.rend(); ++it)
        mq_.push(*it);

    {
        std::vector<std::string*> empty;
        v.swap(empty);
    }

    if (cs < 0)
        cs = 0;

    res.current_size = cs;
    return res;
}

void Queue_Controller::change_algo(shared_ptr<Algo> algo, Algo::Fallback_Options::Type fallback_algo)
{
    algo_ = algo;
    
    if (fallback_algo == Algo::Fallback_Options::Remove_Newest)
        algo_fallback_ = make_shared<Remove_Newest>(*this);

    if (fallback_algo == Algo::Fallback_Options::Remove_Oldest)
        algo_fallback_ = make_shared<Remove_Oldest>(*this);
}
