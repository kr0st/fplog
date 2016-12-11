//
//  fpylog.cpp
//  fpylog
//
//  Created by Rostislav Kuratch on 06/12/16.
//  Copyright © 2016 Rostislav Kuratch. All rights reserved.
//

#include "fpylog.h"

#include <iostream>
#include <boost/python.hpp>
using namespace boost::python;


BOOST_PYTHON_MODULE(fpylog)
{
    class_<fpylog::World>("World")
    .def("greet", &fpylog::World::greet)
    .def("set", &fpylog::World::set)
    ;
}
