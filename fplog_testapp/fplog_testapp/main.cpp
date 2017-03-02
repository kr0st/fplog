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

std::vector<std::string> string_vector = {
    "hghggj",
    "poi5467",
    "DFBNMmrfufmk",
    "123098454",
    "hello",
    "A",
    "a",
    "asdHRSEDHBHMKJKLLKLA",
    "hdkflhajklsjkllfcjkawklnhdnjhymkujsduunkudnkuynuynwsyybmajybjsu6t5rbh54bqat4bh54vqagt4vgtwsy56bsuz7drjnytvbsg",
    "jkldfglkjasfkljdfhiuw4t687436nmdghjlasrljn;xsifdhrdnrtyhipd" };

fplog::Message get_random_message()
{
    std::string str = string_vector[rand() % 10];
    
    int x = rand() % 2000;
    if (x % 10 == 0)
    {
        return FPL_TRACE("%s", str.c_str());
    }
    else if (x % 6 == 0)
    {
        return FPL_ERROR("%s", str.c_str());
    }
    else if (x % 7 == 0)
    {
        return FPL_WARN("%s", str.c_str());
    }
    else
    {
        return FPL_INFO("%s", str.c_str());
    }
}

void lua_filter_perf_test_thread(int msg_count)
{
    Lua_Filter* lua_filter = new Lua_Filter("lua_filter", "if fplog_message.priority ~= nil then filter_result = true else filter_result = false end");
    
    for (int i = 0; i < msg_count; i++)
    {
        try
        {
            fplog::Message msg(get_random_message());
            bool passed = lua_filter->should_pass(msg);
        }
        catch (fplog::exceptions::Generic_Exception& e)
        {
            printf("EXCEPTION: %s\n", e.what().c_str());
        }
    }
    
    delete lua_filter;
}

int main(int argc, const char * argv[])
{
    initlog("fplog_testapp", "18849_18850", 0, false);
    openlog(Facility::security, new Priority_Filter("prio_filter"));
    
    lua_filter_perf_test_thread(10);
    
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
