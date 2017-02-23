//
//  main.cpp
//  fplog_testapp
//
//  Created by Rostislav Kuratch on 18/02/17.
//  Copyright © 2017 Rostislav Kuratch. All rights reserved.
//

#include <iostream>
#include <fplog/fplog.h>

using namespace std;
using namespace fplog;

int main(int argc, const char * argv[])
{
    initlog("fplog_testapp", "18849_18850", 0, false);
    openlog(Facility::security, new Priority_Filter("prio_filter"));
    
    Priority_Filter* filter = dynamic_cast<Priority_Filter*>(find_filter("prio_filter"));
    if (filter)
        filter->add(fplog::Prio::debug);
    
    std::string str;
    printf("Type log message to send. Input quit to exit.\n");

    std::cin >> str;
    while (str != "quit")
    {
        try
        {
            fplog::write(FPL_TRACE("%s", str.c_str()));
            std::cin >> str;
        }
        catch (fplog::exceptions::Generic_Exception& e)
        {
            printf("EXCEPTION: %s\n", e.what().c_str());
        }
    }

    closelog();
}
