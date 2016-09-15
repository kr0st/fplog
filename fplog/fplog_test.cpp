#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <wspiapi.h>
#endif

#ifndef _LINUX
#include <conio.h>
#else

#include <stdio.h>
#include <termios.h>
#include <unistd.h>

int _getch() {
  struct termios oldt,
                 newt;
  int            ch;
  tcgetattr( STDIN_FILENO, &oldt );
  newt = oldt;
  newt.c_lflag &= ~( ICANON | ECHO );
  tcsetattr( STDIN_FILENO, TCSANOW, &newt );
  ch = getchar();
  tcsetattr( STDIN_FILENO, TCSANOW, &oldt );
  return ch;
}

#endif

#include <iostream>
#include <libjson/libjson.h>
#include "fplog.h"
#include "utils.h"
#include <chrono>
#include <thread>
//#include <udt.h>
//#include <cc.h>
//#include <test_util.h>
#include <spipc/UDT_Transport.h>
#include <spipc/socket_transport.h>
#include <fplogd/Queue_Controller.h>

using namespace std;

namespace fplog { 

class FPLOG_API Fplog_Impl
{
    public:

        void set_test_mode(bool mode);
        void wait_until_queues_are_empty();
};

FPLOG_API extern Fplog_Impl* g_fplog_impl;
FPLOG_API extern std::vector<std::string> g_test_results_vector;

namespace testing {

#ifndef WIN32
void* recvdata(void*);
#else
DWORD WINAPI recvdata(LPVOID);
#endif

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
    
    filter = new Priority_Filter("prio_filter");

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

void performance_test()
{
    int circle_count = 10;
    int message_count = 100000;

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
 
        g_fplog_impl->wait_until_queues_are_empty();
        high_resolution_clock::time_point t2 = high_resolution_clock::now();

        auto duration = duration_cast<microseconds>(t2 - t1).count();
        duration_sum += duration;

        std::cout << "Duration of " << j << " iterations: " << duration << " microseconds" <<std::endl;

        fplog::remove_filter(filter);
        filter = 0;
        
        closelog();
    }

    auto final_duration = (double)((duration_sum + 500) / 1000000);
    double result = message_count / final_duration;

    std::cout << "Duration of 10 iteration with " << message_count << " messages, are done in: " << final_duration << " seconds" << std::endl;
    std::cout << result << " messages per second " << std::endl;
  
    
    std::cout << "Lua filter " << std::endl;

    double duration_sum1 = 0;

    for (int j = 0; j < circle_count; j++){
 
        openlog(Facility::security);
        //Lua_Filter* lua_filter = new Lua_Filter("lua_filter", "if fplog_message.priority ~= nil then filter_result = true else filter_result = false end");         //local clock = os.clock function sleep() local i = 0 while i < 250000000 do i = i + 1 end end sleep() 
        fplog::Chai_Filter* lua_filter = new fplog::Chai_Filter("chai_filter", "if (fplog_message[\"priority\"].is_var_undef()) { filter_result = false; } else { filter_result = true;}");
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

        g_fplog_impl->wait_until_queues_are_empty();
        high_resolution_clock::time_point t4 = high_resolution_clock::now();

        auto duration1 = duration_cast<microseconds>(t4 - t3).count();
        duration_sum1 += duration1;

        std::cout << "Duration of " << j << " iterations: " << duration1 << " microseconds" << std::endl;

        fplog::remove_filter(lua_filter);
        lua_filter = 0;

        closelog();
    }

    auto final_duration1 = (double)((duration_sum1 + 500) / 1000000);
    
    double result1 = message_count / final_duration1;
   
    std::cout << result1 << " messages per second " << std::endl;


    std::cout << "Duration of 10 iteration with " << message_count << " messages, are done in: " << final_duration1 << " seconds" << std::endl;

    if (final_duration < final_duration1)
    {
        std::cout << "Priority filter is faster than Lua by " << (final_duration1 * 100) / final_duration - 100 << "%" << std::endl;
    }
    else {
        std::cout << "Lua filter is faster than Priority by " << (final_duration * 100) / final_duration1 - 100 << "%" << std::endl;

    }

    _getch();
}

void prio_filter_perf_test_thread(int msg_count)
{
    Priority_Filter* prio_filter = new Priority_Filter("prio_filter");

    prio_filter->add(fplog::Prio::warning);
    prio_filter->add(fplog::Prio::error);
    prio_filter->add(fplog::Prio::critical);
    prio_filter->add(fplog::Prio::info);

    for (int i = 0; i < msg_count; i++)
    {
        try
        {
            fplog::Message msg(get_random_message());
            bool passed = prio_filter->should_pass(msg);
        }
        catch (fplog::exceptions::Generic_Exception& e)
        {
            printf("EXCEPTION: %s\n", e.what().c_str());
        }
    }

    delete prio_filter;
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

typedef void (*perf_test_thread_func)(int msg_count);

unsigned multi_threaded_filter_performance_test(perf_test_thread_func thread_func, int num_threads, int msg_per_thread)
{
    using namespace std::chrono;

    if (!thread_func || (num_threads == 0))
        return 0;

    high_resolution_clock::time_point t1 = high_resolution_clock::now();

    std::thread** workers = new std::thread* [num_threads];
    for (int i = 0; i < num_threads; ++i)
        workers[i] = new std::thread(thread_func, msg_per_thread);

    for (int i = 0; i < num_threads; ++i)
    {
        workers[i]->join();
        delete workers[i];
    }

    delete [] workers;

    high_resolution_clock::time_point t2 = high_resolution_clock::now();
    
    auto duration = duration_cast<seconds>(t2 - t1).count();

    return duration;
}

void filter_perft_test_summary()
{
    int msg_count = 2000000;
    int thread_count = 2;

    unsigned duration = multi_threaded_filter_performance_test(&prio_filter_perf_test_thread, thread_count, msg_count);
    unsigned msg_per_second = msg_count * thread_count / duration;
    std::cout << "Priority filter performance = " << msg_per_second << " mps" << "(" << thread_count << " threads)" << std::endl;

    msg_count = 200000;
    duration = multi_threaded_filter_performance_test(&lua_filter_perf_test_thread, thread_count, msg_count);
    msg_per_second = msg_count * thread_count / duration;
    std::cout << "Lua filter performance = " << msg_per_second << " mps" << "(" << thread_count << " threads)" << std::endl;
}

void manual_test()
{
    initlog("fplog_test", "18749_18750", 0, false);
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

bool batching_test()
{
    Priority_Filter* filter = dynamic_cast<Priority_Filter*>(find_filter("prio_filter"));
    if (!filter)
        return false;

    filter->add(fplog::Prio::debug);

    JSONNode batch(JSON_ARRAY);

    batch.push_back(fplog::Message(fplog::Prio::debug, fplog::Facility::user, "batching test msg #1").as_json());
    batch.push_back(fplog::Message(fplog::Prio::debug, fplog::Facility::user, "batching test msg #2").as_json());

    fplog::Message batch_msg(fplog::Prio::debug, fplog::Facility::user);
    batch_msg.add_batch(batch);

    JSONNode json_msg(batch_msg.as_json());
    JSONNode::iterator it(json_msg.begin());

    std::cout << json_msg.write() << std::endl << std::endl;
    int counter = 0;

    while (it != json_msg.end())
    {
        std::cout << counter << ": name=" << it->name().c_str() << ", value=" << it->as_string().c_str() << std::endl;
        ++it;
        counter++;
    }

    std::cout << "msg has batch? = " << batch_msg.has_batch() << std::endl;

    fplog::Message batch_clone(batch_msg.as_string());

    std::cout << "cloned msg: " << batch_clone.as_string() << std::endl << std::endl;
    std::cout << "clone msg has batch? = " << batch_clone.has_batch() << std::endl;

    return true;
}

bool queue_controller_test()
{
    {
        Queue_Controller qc(200, 3);
        
        std::string msg("Ten bytes.");
        
        for (int i = 0; i < 30; ++i)
            qc.push(new std::string(msg));
        
        std::vector<std::string*> v;
        
        while (!qc.empty())
        {
            v.push_back(qc.front());
            qc.pop();
        }
        
        if (v.size() != 30)
        {
            cout << "Incorrect size of queue detected! (" << v.size() << ")." << std::endl;
            return false;
        }
        
        for (int i = 0; i < 30; ++i)
            qc.push(new std::string(msg));
            
        std::this_thread::sleep_for(chrono::seconds(4));
        
        qc.push(new std::string(msg));
        v.clear();
        
        while (!qc.empty())
        {
            v.push_back(qc.front());
            qc.pop();
        }

        if (v.size() != 20)
        {
            cout << "Incorrect size of queue detected! (" << v.size() << ")." << std::endl;
            return false;
        }
    }
    return true;
}

void run_all_tests()
{
    initlog("fplog_test", "18749_18750");
    openlog(Facility::security, new Priority_Filter("prio_filter"));
    g_fplog_impl->set_test_mode(true);

    if (!queue_controller_test())
        printf("queue_controller_test failed!\n");
        
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

    if (!batching_test())
        printf("batching_test failed!\n");

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

bool g_udt_stop;
const char* g_udt_msg = "very secret one paragraph";
bool g_udt_transport_test = false;

fplog::Transport_Interface::Params g_udt_params;

void transport_server()
{
    fplog::Transport_Interface* trans;

    if (g_udt_transport_test)
        trans = new spipc::UDT_Transport();
    else
        trans = new spipc::Socket_Transport();
        
    std::auto_ptr<fplog::Transport_Interface> ptr(trans);

    trans->connect(g_udt_params);

    while (!g_udt_stop)
    {
        char buf[1024] = {0};

        try
        {
            memset(buf, 0, sizeof(buf));
            if (trans->read(buf, sizeof(buf), 1000) > 0)
                std::cout << "READ " << buf << std::endl;
        }
        catch (fplog::exceptions::Generic_Exception& e)
        {
            std::cout << "RECV error: " << e.what() << std::endl;
        }
    }
}

void transport_client()
{
    fplog::Transport_Interface* trans;
    
    if (g_udt_transport_test)
        trans = new spipc::UDT_Transport();
    else
        trans = new spipc::Socket_Transport();
        
    std::auto_ptr<fplog::Transport_Interface> ptr(trans);

    trans->connect(g_udt_params);

    while (!g_udt_stop)
    {
        try
        {
            if (trans->write(g_udt_msg, strlen(g_udt_msg) + 1, 1000) > 0)
                std::cout << "WROTE " << g_udt_msg << std::endl;
        }
        catch (fplog::exceptions::Generic_Exception& e)
        {
            std::cout << "SEND error: " << e.what() << std::endl;
        }
    }
}

bool transport_test()
{
    g_udt_params["type"] = "ip";
    g_udt_params["ip"] = "127.0.0.1";
    g_udt_params["uid"] = "18751_18752";

    g_udt_stop = false;

    std::thread srv(transport_server);
    std::thread client(transport_client);

    _getch();
    g_udt_stop = true;

    srv.join();
    client.join();

    return true;
}

bool udt_test()
{
    g_udt_transport_test = true;
    return transport_test();
}

bool socket_test()
{
    g_udt_transport_test = false;
    return transport_test();
}

}};


int main()
{
    fplog::testing::queue_controller_test();
    
    //fplog::testing::run_all_tests();

    //fplog::testing::manual_test();
    
    //fplog::testing::performance_test();
    
    //fplog::testing::filter_perft_test_summary();
    //fplog::testing::spam_test();
    //fplog::testing::multithreading_test();

    //fplog::testing::udt_test();
    //fplog::testing::socket_test();

    fplog::shutdownlog();
    return 0;
}
