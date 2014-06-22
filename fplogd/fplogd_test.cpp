#include "fplogd.h"

#include <tchar.h>
#include <conio.h>
#include <iostream>
#include <fstream>

#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>


using namespace boost::interprocess;

namespace fplogd { namespace testing {

}};

class Foo
{
    public:

        void Bar(){ printf("Foo::Bar called!\n"); }
};

void FooBar() { printf("FooBar called!\n"); }

int _tmain(int argc, _TCHAR* argv[])
{
    /*try 
    {
        std::string flock_name(fplogd::get_lock_file_name());
        std::ofstream o(flock_name);
        file_lock flock(flock_name.c_str());
        printf("file_lock complete...\n");

        scoped_lock<file_lock> lock(flock);
        printf("scoped_lock complete...\n");

        printf("Locked! Press any key to exit..\n");
        _getch();
    }
    catch (interprocess_exception& e)
    {
        printf("Unable to lock: %s\n", e.what());
    }*/

    /*fplogd::notify_when_started<Foo>(&Foo::Bar, new Foo());
    fplogd::notify_when_started(FooBar);

    std::this_thread::sleep_for(std::chrono::hours(1));*/

    /*spipc::global_init();

    std::vector<fplogd::Channel_Data> channels(fplogd::get_registered_channels());
    spipc::IPC ipc;
    ipc.connect(channels[0].uid);

    while (true)
    {
        char str[256] = {0};
        sprintf_s(str, sizeof(str), "%s: kill me kill me kill me...", channels[0].app_name.c_str());
        
        try
        {
            ipc.write(str, sizeof(str), 1000);
        }
        catch(sprot::exceptions::Exception)
        {
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }*/

    fplogd::start();
    _getch();
    fplogd::stop();

	return 0;
}
