//
//  fpylog.cpp
//  fpylog
//
//  Created by Rostislav Kuratch on 06/12/16.
//  Copyright © 2016 Rostislav Kuratch. All rights reserved.
//

#include "fpylog.h"
#include <fplog.h>

#include <iostream>
#include <boost/python.hpp>
using namespace boost::python;


BOOST_PYTHON_MODULE(fpylog)
{
    class_<fpylog::World>("World")
    .def("greet", &fpylog::World::greet)
    .def("set", &fpylog::World::set);
    
    class_<fplog::Message>("Message", init<const char*, const char*, const char*>(args("prio", "facility", "text")))
    .def("as_json_string", &fplog::Message::as_string);
}
