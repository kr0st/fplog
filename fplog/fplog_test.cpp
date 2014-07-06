
#include <libjson/libjson.h>
#include "fplog.h"

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

    return 0;
}
