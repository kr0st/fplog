#include "Queue_Controller.h"

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
    int buf_length = strnlen_s(str->c_str(), buf_sz);

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