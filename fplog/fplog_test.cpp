
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

    arr.push_back(foo);
    arr.push_back(bar);

    printf("%s\n", arr.write_formatted().c_str());

    printf("Timezone: %d\n", generic_util::get_system_timezone());
    printf("iso8601 timezone: %s\n", generic_util::timezone_from_minutes_to_iso8601(generic_util::get_system_timezone()).c_str());

    return 0;
}
