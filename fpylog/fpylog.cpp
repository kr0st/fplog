//
//  fpylog.cpp
//  fpylog
//
//  Created by Rostislav Kuratch on 06/12/16.
//  Copyright © 2016 Rostislav Kuratch. All rights reserved.
//

#include "fpylog.h"

#include <boost/python.hpp>
using namespace boost::python;

BOOST_PYTHON_MODULE(hello)
{
    class_<World>("World")
    .def("greet", &World::greet)
    .def("set", &World::set)
    ;
}
