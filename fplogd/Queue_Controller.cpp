#include "Queue_Controller.h"
#include <string.h>

Queue_Controller::Queue_Controller(Algo algo = Remove_Newest, size_t timeout = 30000)
{
}

bool Queue_Controller::empty()
{
   return mq_.empty();
    
};

string *Queue_Controller::front()
{
    return mq_.front();
}

void Queue_Controller::pop()
{
    return mq_.pop();
}

void Queue_Controller::push(string *str)
{   
    size_t buf_sz = 2048;
    
    #ifdef _LINUX
    int buf_length = strnlen(str->c_str(), buf_sz);
    #else
    int buf_length = strnlen_s(str->c_str(), buf_sz);
    #endif

    if (mq_size < queue_limiter)      {                      

        mq_.push(str);
        mq_size += buf_length;

    }
    else
    {
        while (mq_size > queue_limiter){

            mq_.pop();
            mq_size -= buf_length;
        }

        mq_.push(str);

    }
}