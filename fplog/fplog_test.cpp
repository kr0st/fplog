
#include <libjson/libjson.h>
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
    fplog::write(fplog::Message(fplog::Prio::alert, fplog::Facility::system, "go fetch some numbers").add("int", 23).add("    Double ", -1.23).add_binary("int_bin", &var, sizeof(int)));
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

    printf("%s\n", json3.write_formatted().c_str());

    return true;
}

void run_all_tests()
{
    initlog("fplog_test");
    openlog(Facility::security);

    if (!class_logging_test())
        printf("class_logging_test failed!\n");

    if (!send_file_test())
        printf("send_file_test failed!\n");

    if (!trim_and_blob_test())
        printf("trim_and_blob_test failed!\n");

    if (!input_validators_test())
        printf("input_validators_test failed!\n");

    closelog();
}

}};

int main()
{
    fplog::testing::run_all_tests();
    return 0;
}
