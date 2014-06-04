#include "fplogd.h"

#include <tchar.h>
#include <conio.h>
#include <iostream>
#include <fstream>

#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

//#include <boost/thread/thread_time.hpp>
//flock.timed_lock(boost::get_system_time() + boost::posix_time::seconds(12));

using namespace boost::interprocess;

namespace fplogd { namespace testing {

}};

class Foo
{
    public:

        void Bar(){}
};

void FooBar() {}

int _tmain(int argc, _TCHAR* argv[])
{
    try 
    {
        const char* flock_name = "C:\\Users\\rosti_000\\.fplogd_lock";

        std::ofstream o(flock_name);
        file_lock flock(flock_name);
        printf("file_lock complete...\n");

        scoped_lock<file_lock> lock(flock);
        printf("scoped_lock complete...\n");

        printf("Locked! Press any key to exit..\n");
        _getch();
    }
    catch (interprocess_exception& e)
    {
        printf("Unable to lock: %s\n", e.what());
    }

    fplogd::notify_when_started<Foo>(&Foo::Bar, new Foo());
    fplogd::notify_when_started(FooBar);

	return 0;
}
