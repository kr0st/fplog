#include <iostream>
#include <libjson/libjson.h>
#include "fplog.h"
#include "utils.h"
#include <thread>
#include <conio.h>

namespace fplog { 
    
class FPLOG_API Fplog_Impl
{
    public:

        void set_test_mode(bool mode);
};

FPLOG_API extern fplog::Fplog_Impl g_fplog_impl;
FPLOG_API extern std::vector<std::string> g_test_results_vector;

namespace testing {

class Bar
{
    public:
        virtual bool FooBar() = 0;
};

class Foo: public Bar
{
    public:

        bool FooBar()
        {
            fplog::write(FPL_CINFO("blah-blah %d!", 38).add("real", -9.54));
            return true;
        }
};

bool class_logging_test()
{
    Foo Bar;
    return Bar.FooBar();
}

bool send_file_test()
{
    const char* str = "asafdkfj *** Hello, world! -=-=-=-=-=-+++   ";
    fplog::write(fplog::File(fplog::Prio::alert, "dump.bin", str, strlen(str)).as_message());
    
    return true;
}

bool trim_and_blob_test()
{
    int var = -533;
    int var2 = 54674;
    fplog::write(fplog::Message(fplog::Prio::alert, fplog::Facility::system, "go fetch some numbers").add("blob", 66).add("int", 23).add_binary("int_bin", &var, sizeof(int)).add_binary("int_bin", &var2, sizeof(int)).add("    Double ", -1.23).add("encrypted", "sfewre"));
    return true;
}

bool input_validators_test()
{
    JSONNode json1(JSON_NODE);
    json1.push_back(JSONNode(Message::Optional_Fields::encrypted, "should_not_appear"));
    json1.set_name(Message::Optional_Fields::blob);

    JSONNode json2(JSON_ARRAY);
    json2.set_name("  tExt ");
    json2.push_back(json1);

    JSONNode json3(JSON_NODE);
    json3.push_back(json2);

    fplog::write(fplog::Message(fplog::Prio::info, fplog::Facility::user).add(json3));

    return true;
}

bool filter_test()
{
    Priority_Filter* filter = dynamic_cast<Priority_Filter*>(find_filter("prio_filter"));
    if (!filter)
        return false;

    {
        fplog::Message msg(fplog::Prio::alert, fplog::Facility::system, "this message should not appear");
        fplog::write(msg);
    }

    filter->add(fplog::Prio::emergency);
    filter->add(fplog::Prio::debug);

    {
        fplog::Message msg(fplog::Prio::alert, fplog::Facility::system, "this message still should not appear");
        fplog::write(msg);
    }
    {
        fplog::Message msg(fplog::Prio::emergency, fplog::Facility::system, "this emergency message is visible");
        fplog::write(msg);
    }
    {
        fplog::Message msg(fplog::Prio::debug, fplog::Facility::system, "along with this debug message");
        fplog::write(msg);
    }

    filter->remove(fplog::Prio::emergency);

    {
        fplog::Message msg(fplog::Prio::emergency, fplog::Facility::system, "this is invisible emergency");
        fplog::write(msg);
    }

    remove_filter(filter);

    {
        fplog::Message msg(fplog::Prio::debug, fplog::Facility::system, "this debug message should be invisible");
        fplog::write(msg);
    }

    Lua_Filter* lua_filter = new Lua_Filter("lua_filter", "if fplog_message.text ~= nil and string.find(fplog_message.text, \"hello from Lua!\") ~= nil then filter_result = true else filter_result = false end");

    fplog::add_filter(lua_filter);
    fplog::add_filter(filter);

    filter->add(fplog::Prio::alert);
    filter->add(fplog::Prio::critical);
    filter->add(fplog::Prio::debug);
    filter->add(fplog::Prio::emergency);
    filter->add(fplog::Prio::error);
    filter->add(fplog::Prio::info);
    filter->add(fplog::Prio::notice);
    filter->add(fplog::Prio::warning);

    {
        fplog::Message msg(fplog::Prio::info, fplog::Facility::user, "this message from Lua should not be displayed");
        fplog::write(msg);
    }

    {
        fplog::Message msg(fplog::Prio::notice, fplog::Facility::security, "this notice is to say hello from Lua!");
        fplog::write(msg);
    }

    fplog::remove_filter(lua_filter);

    return true;
}

void print_test_vector()
{
    for (auto str : fplog::g_test_results_vector)
    {
        std::cout << str << std::endl;
    }
}

void manual_test()
{
    initlog("fplog_test", "18749_18750");
    openlog(Facility::security, new Priority_Filter("prio_filter"));
    Priority_Filter* filter = dynamic_cast<Priority_Filter*>(find_filter("prio_filter"));
    if (filter)
        filter->add(fplog::Prio::debug);

    std::string str;
    printf("Type log message to send. Input quit to exit.\n");
    while (str != "quit")
    {
        try
        {
            std::cin >> str;
            fplog::write(FPL_TRACE("%s", str.c_str()));
        }
        catch (fplog::exceptions::Generic_Exception& e)
        {
            printf("EXCEPTION: %s\n", e.what().c_str());
        }
    }
    closelog();
}

void spam_test()
{
    initlog("fplog_test", "18749_18750");
    openlog(Facility::security, new Priority_Filter("prio_filter"));
    Priority_Filter* filter = dynamic_cast<Priority_Filter*>(find_filter("prio_filter"));
    if (filter)
        filter->add(fplog::Prio::debug);

    std::string str;
    printf("Flooding fplogd with messages - each with incremental number.\n");
    unsigned long num = 0;

    while (true)
    {
        try
        {
            num++;
            str = std::to_string(num);
            fplog::write(FPL_TRACE("Next number is %s", str.c_str()));
        }
        catch (fplog::exceptions::Generic_Exception& e)
        {
            printf("EXCEPTION: %s\n", e.what().c_str());
        }
    }
    closelog();
}

void run_all_tests()
{
    initlog("fplog_test", "18749_18750");
    openlog(Facility::security, new Priority_Filter("prio_filter"));
    fplog::g_fplog_impl.set_test_mode(true);

    if (!filter_test())
        printf("filter_test failed!\n");

    if (!class_logging_test())
        printf("class_logging_test failed!\n");

    if (!send_file_test())
        printf("send_file_test failed!\n");

    if (!trim_and_blob_test())
        printf("trim_and_blob_test failed!\n");

    if (!input_validators_test())
        printf("input_validators_test failed!\n");

    print_test_vector();
    closelog();
}

void start_thread(const char *facility)
{
    openlog(facility);
    Priority_Filter *priority_filter = new Priority_Filter("prio_filter");
    priority_filter->add(fplog::Prio::debug);
    fplog::add_filter(priority_filter);

    fplog::write(FPL_TRACE(facility));

    closelog();
}

void multithreading_test()
{
    initlog("fplog_test", "18749_18750");
    std::vector<std::thread> threads;
   
    threads.push_back(std::thread(start_thread, Facility::user));

    threads.push_back(std::thread(start_thread, Facility::system));

    _getch();

    for (std::vector<std::thread>::iterator it = threads.begin(); it != threads.end(); ++it)
    {
        it->join();
    }
}

}};

int main()
{
    //fplog::testing::run_all_tests();
    fplog::testing::manual_test();
    //fplog::testing::spam_test();
    //fplog::testing::multithreading_test();
    return 0;
}
