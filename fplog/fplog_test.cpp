
#include <libjson/libjson.h>
#include <lua.hpp>
#include <boost/algorithm/string.hpp>
#include "fplog.h"
#include "utils.h"

namespace fplog { namespace testing {

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

    fplog::Message msg(fplog::Prio::alert, fplog::Facility::system, "go fetch some numbers");
    msg.add("blob", 66).add("int", 23).add_binary("int_bin", &var, sizeof(int)).add_binary("int_bin", &var2, sizeof(int)).add("    Double ", -1.23).add("encrypted", "sfewre");
    //fplog::write(msg);
    std::string log_msg_escaped(msg.as_string());
    boost::replace_all(log_msg_escaped, "\"", "\\\"");

    const char* format = "log_msg=\"%s\"; fplog_message = json.decode(log_msg); print(fplog_message.int)";
    char lua_script[2048] = {0};
    _snprintf(lua_script, sizeof(lua_script) - 1, format, log_msg_escaped.c_str());

    printf("%s\n", lua_script);

    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    luaL_dostring(L, "json = require(\"json\");");        
    luaL_dostring(L, lua_script);

    printf("%s\n", lua_tostring(L, -1));
    lua_pop(L, 1);

    lua_close(L);

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

    return true;
}

void run_all_tests()
{
    initlog("fplog_test");
    openlog(Facility::security, new Priority_Filter("prio_filter"));

    if (!filter_test())
        printf("filter_test failed!\n");

    //if (!class_logging_test())
        //printf("class_logging_test failed!\n");

    //if (!send_file_test())
        //printf("send_file_test failed!\n");

    //if (!trim_and_blob_test())
        //printf("trim_and_blob_test failed!\n");

    //if (!input_validators_test())
        //printf("input_validators_test failed!\n");

    closelog();
}

}};

int main()
{
    fplog::testing::run_all_tests();
    return 0;
}
