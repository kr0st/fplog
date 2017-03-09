// fplog_testapp.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <fplog.h>


int main()
{
    fplog::initlog("fplog_testapp", "18849_18850");
    fplog::openlog(fplog::Facility::user);

    FPL_WARN("Testing! Tesing!");

    fplog::closelog();
    return 0;
}
