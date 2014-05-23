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

int _tmain(int argc, _TCHAR* argv[])
{
    try 
    {
        const char* flock_name = "D:\\Profiles\\RostislavK1\\.fplogd_lock";

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

	return 0;
}
