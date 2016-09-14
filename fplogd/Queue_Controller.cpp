#include "Queue_Controller.h"
#include <string.h>

static const size_t buf_sz = -1;

Queue_Controller::Queue_Controller(size_t size_limit, size_t timeout):
max_size_(size_limit),
emergency_time_trigger_(timeout)
{
    algo_ = make_shared<Remove_Newest>(mq_, max_size_);
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

    #ifdef _LINUX
    int buf_length = strnlen(str->c_str(), buf_sz);
    #else
    int buf_length = strnlen_s(str->c_str(), buf_sz);
    #endif

    mq_size_ -= buf_length;
}

void Queue_Controller::push(string *str)
{   
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
    if (mq_size_ > max_size_)
        return true;
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

        #ifdef _LINUX
        int buf_length = strnlen(str->c_str(), buf_sz);
        #else
        int buf_length = strnlen_s(str->c_str(), buf_sz);
        #endif        

        res.removed_count++;
        cs -= buf_length;
    }
    
    if (cs < 0)
        cs = 0;

    res.current_size = cs;
    return res;
}
