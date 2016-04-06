#include <iostream>
#include <libjson/libjson.h>
#include "fplog.h"
#include "utils.h"
#include <chrono>


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

/////////////////////////////////////////////////////////////////////////////////////////

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

void random_message()
{
    std::string str = string_vector[rand() % 10];

    int x = rand() % 2000;
    if (x % 10 == 0)
    {
        fplog::write(FPL_TRACE("%s", str.c_str()));
    }
    else if (x % 6 == 0)
    {
        fplog::write(FPL_ERROR("%s", str.c_str()));
    }
    else if (x % 7 == 0)
    {
        fplog::write(FPL_WARN("%s", str.c_str()));
    }
    else
    {
        fplog::write(FPL_INFO("%s", str.c_str()));
    }
}

void performance_test()
{
    int circle_count = 10;
    int message_count = 10000;

    using namespace std::chrono;
    initlog("fplog_test", "18749_18750");


    std::cout << "Priority filter " << std::endl;

    double duration_sum = 0;

    for (int j = 0; j < circle_count; j++){

        openlog(Facility::security, new Priority_Filter("prio_filter"));
        Priority_Filter* filter = dynamic_cast<Priority_Filter*>(find_filter("prio_filter"));

        if (filter)
            filter->add(fplog::Prio::debug);

        high_resolution_clock::time_point t1 = high_resolution_clock::now();
        for (int i = 0; i < message_count; i++) {
            try
            {
                random_message();
            }
            catch (fplog::exceptions::Generic_Exception& e)
            {
                printf("EXCEPTION: %s\n", e.what().c_str());
            }
        }
 
        high_resolution_clock::time_point t2 = high_resolution_clock::now();

        auto duration = duration_cast<microseconds>(t2 - t1).count();

        std::cout << "Duration of " << j << " iterations: " << duration << " microseconds" <<std::endl;

        closelog();
        duration_sum += duration;

    }

    auto final_duration = (double)((duration_sum + 500) / 1000000);
    double result = message_count / final_duration;

    std::cout << "Duration of 10 iteration with " << message_count << " messages, are done in: " << final_duration << " seconds" << std::endl;
    std::cout << result << " messages per second " << std::endl;
  
    
    std::cout << "Lua filter " << std::endl;

    double duration_sum1 = 0;

    for (int j = 0; j < circle_count; j++){
 
        openlog(Facility::security);
        Lua_Filter* lua_filter = new Lua_Filter("lua_filter", "if fplog_message.priority ~= nil then filter_result = true else filter_result = false end");         //local clock = os.clock function sleep() local i = 0 while i < 250000000 do i = i + 1 end end sleep() 
        fplog::add_filter(lua_filter);

        high_resolution_clock::time_point t3 = high_resolution_clock::now();

        for (int i = 0; i < message_count; i++) {

            try
            {
                random_message();
            }
            catch (fplog::exceptions::Generic_Exception& e)
            {
                printf("EXCEPTION: %s\n", e.what().c_str());
            }
        }
        high_resolution_clock::time_point t4 = high_resolution_clock::now();

        auto duration1 = duration_cast<microseconds>(t4 - t3).count();
        std::cout << "Duration of " << j << " iterations: " << duration1 << " microseconds" << std::endl;

        fplog::remove_filter(lua_filter);
        closelog();

        duration_sum1 += duration1;
    }

    auto final_duration1 = (double)((duration_sum1 + 500) / 1000000);
    
    double result1 = message_count / final_duration1;
   
    std::cout << result1 << " messages per second " << std::endl;


    std::cout << "Duration of 10 iteration with " << message_count << " messages, are done in: " << final_duration1 << " seconds" << std::endl;

    if (final_duration < final_duration1)
    {
        std::cout << "Priority filter is faster than Lua for " << (final_duration1 * 100) / final_duration << "%" << std::endl;
    }
    else {
        std::cout << "Lua filter is faster than Priority for " << (final_duration * 100) / final_duration1 << "%" << std::endl;

    }

    _getch();
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

}};

int main()
{
    //fplog::testing::run_all_tests();
  // fplog::testing::manual_test();
     fplog::testing::performance_test();
    //fplog::testing::spam_test();
    return 0;
}
