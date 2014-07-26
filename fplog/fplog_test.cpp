
#include <libjson/libjson.h>
#include "fplog.h"
#include "utils.h"

int main()
{
    JSONNode arr(JSON_ARRAY);
    JSONNode foo(JSON_NODE);
    JSONNode bar(JSON_NODE);

    foo.push_back(JSONNode("word", "foo"));
    bar.push_back(JSONNode("word", "bar"));
    bar.push_back(JSONNode("sushi", "bar"));

    arr.push_back(foo);
    arr.push_back(bar);
    arr.set_name("some_array");

    JSONNode base(JSON_NODE);
    base.push_back(arr);

    printf("%s\n", base.write_formatted().c_str());

    printf("Timezone: %d\n", generic_util::get_system_timezone());
    printf("iso8601 timezone: %s\n", generic_util::timezone_from_minutes_to_iso8601(generic_util::get_system_timezone()).c_str());
    printf("Full iso8601 date-time with timezone: %s\n", generic_util::get_iso8601_timestamp().c_str());

    fplog::write(fplog::Message(fplog::Prio::alert, "go fetch some numbers").add("int", 23).add("double", -1.23));

    return 0;
}
