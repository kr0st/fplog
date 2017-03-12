// fplog_testapp.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <fplog.h>


int main()
{
    fplog::initlog("fplog_testapp", "18849_18850", 0, false);
    fplog::openlog(fplog::Facility::user, new fplog::Priority_Filter("prio_filter"));

    fplog::Priority_Filter* filter = dynamic_cast<fplog::Priority_Filter*>(fplog::find_filter("prio_filter"));
    filter->add_all_above(fplog::Prio::debug, true);

    fplog::write(FPL_WARN("Testing! Tesing!"));

    fplog::closelog();
    return 0;
}
