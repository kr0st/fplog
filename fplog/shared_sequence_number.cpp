#include "shared_sequence_number.h"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/shared_memory_object.hpp>

#include <chrono>

using namespace std::chrono;

namespace fplog {

static const char* g_shared_mem_name = "fplog_sequence_number";
static const char* g_condition_mutex_name = "fplog_sequence_protector";
static const size_t g_shared_mem_size = sizeof(unsigned long long int);

class Shared_Sequence_Number::Impl
{
    public:
        
        Impl();
        virtual ~Impl();
        
        unsigned long long int read();
    
    
    private:
      
        boost::interprocess::shared_memory_object* shared_mem_;
        boost::interprocess::mapped_region* mapped_mem_region_;
        
        unsigned char* buf_;
        
        boost::interprocess::named_mutex condition_mutex_;
};
  
Shared_Sequence_Number::Shared_Sequence_Number()
{
    impl_ = new Shared_Sequence_Number::Impl();
}

Shared_Sequence_Number::~Shared_Sequence_Number()
{
    delete impl_;
}

unsigned long long int Shared_Sequence_Number::read()
{
    return impl_->read();
}
    
Shared_Sequence_Number::Impl::~Impl()
{
    boost::interprocess::scoped_lock<boost::interprocess::named_mutex> lock(condition_mutex_);
    delete mapped_mem_region_;

    buf_ = 0;
    mapped_mem_region_ = 0;

    delete shared_mem_;
}

Shared_Sequence_Number::Impl::Impl():
condition_mutex_(boost::interprocess::open_or_create, g_condition_mutex_name),
mapped_mem_region_(0),
buf_(0),
shared_mem_(0)
{
relock:

    boost::posix_time::ptime pt = boost::posix_time::microsec_clock::universal_time() 
        +  boost::posix_time::milliseconds(5000);

    boost::interprocess::scoped_lock<boost::interprocess::named_mutex> lock(condition_mutex_, pt);

    if (!lock.owns())
    {
        lock.release();
        condition_mutex_.unlock();
        goto relock;
    }

    bool virgin = false;
    try
    {
        shared_mem_ = new boost::interprocess::shared_memory_object(boost::interprocess::open_only, g_shared_mem_name, boost::interprocess::read_write);
    }
    catch (boost::interprocess::interprocess_exception&)
    {
        virgin = true;
        delete shared_mem_;
        shared_mem_ = new boost::interprocess::shared_memory_object(boost::interprocess::open_or_create, g_shared_mem_name, boost::interprocess::read_write);
    }

    //without this mapped_region will throw exception all the time becaues shared mem file is 0 size initially and this is the way to set shared mem size
    shared_mem_->truncate(g_shared_mem_size);

    mapped_mem_region_ = new boost::interprocess::mapped_region(*shared_mem_, boost::interprocess::read_write);
    buf_ = static_cast<unsigned char*>(mapped_mem_region_->get_address());

    if (virgin)
        memset(buf_, 0, g_shared_mem_size);
}

unsigned long long int Shared_Sequence_Number::Impl::read()
{
relock:

    boost::posix_time::ptime pt = boost::posix_time::microsec_clock::universal_time() 
        +  boost::posix_time::milliseconds(5000);

    boost::interprocess::scoped_lock<boost::interprocess::named_mutex> lock(condition_mutex_, pt);

    if (!lock.owns())
    {
        lock.release();
        condition_mutex_.unlock();
        goto relock;
    }

    unsigned long long int number;

    memcpy(&number, buf_, sizeof(unsigned long long int));
    number++;
    memcpy(buf_, &number, sizeof(unsigned long long int));

    return number - 1;
}

};
