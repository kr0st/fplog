#include "Queue_Controller.h"

bool Queue_Controller::empty()
{
   return mq_.empty();
    
};

void Queue_Controller::front(string *str)
{
    str = mq_.front();
}

void Queue_Controller::pop()
{
    return mq_.pop();
}

void Queue_Controller::push(string *str)
{   
    size_t buf_sz = 2048;
    int buf_length = strnlen_s(str->c_str(), buf_sz);
   
    mq_size += buf_length;

    if (mq_size < queue_limiter)      {                      

        mq_.push(str);
    }
    else
    {
        while ((mq_size < queue_limiter) && (mq_size <= (queue_limiter - buf_length))){

            mq_.pop();
        }

        mq_.push(str);

    }
    return mq_.push(str);
}