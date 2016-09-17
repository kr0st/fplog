#include "Queue_Controller.h"
#include <string.h>
#include <stack>
#include <vector>
#include <iostream>
#include <algorithm>

static const size_t buf_sz = -1;

Queue_Controller::Queue_Controller(size_t size_limit, size_t timeout):
max_size_(size_limit),
emergency_time_trigger_(timeout),
timer_start_(chrono::milliseconds(0))
{
    algo_ = make_shared<Remove_Newest>(*this);
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
            timer_start_ = time_point<system_clock, system_clock::duration>(chrono::milliseconds(0));
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
}

Queue_Controller::Algo::Result Queue_Controller::Remove_Newest::process_queue(size_t current_size)
{
    Result res;
    res.current_size = 0;
    res.removed_count = 0;
    
    int cs = current_size;
    
    while (cs >= max_size_)
    {
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

Queue_Controller::Algo::Result Queue_Controller::Remove_Oldest::process_queue(size_t current_size)
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

    std::reverse(v.begin(), v.end());
    
    while (cs >= max_size_)
    {
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
