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
#include <string>

using namespace boost::python;

fplog::Message& (fplog::Message::*add_short)(const char*, int) = &fplog::Message::add;
fplog::Message& (fplog::Message::*add_long)(const char*, long long int) = &fplog::Message::add;
fplog::Message& (fplog::Message::*add_float)(const char*, double) = &fplog::Message::add;
fplog::Message& (fplog::Message::*add_string)(const char*, const char*) = &fplog::Message::add;

fplog::Message& (fplog::Message::*set_text)(const char*) = &fplog::Message::set_text;
fplog::Message& (fplog::Message::*set_class)(const char*) = &fplog::Message::set_class;
fplog::Message& (fplog::Message::*set_module)(const char*) = &fplog::Message::set_module;
fplog::Message& (fplog::Message::*set_method)(const char*) = &fplog::Message::set_method;

BOOST_PYTHON_MODULE(fpylog)
{
    class_<fpylog::World>("World")
    .def("greet", &fpylog::World::greet)
    .def("set", &fpylog::World::set);
    
    scope Message(
    class_<fplog::Message>("Message", init<const char*, const char*, const char*>(args("prio", "facility", "text")))
    .def("as_json_string", &fplog::Message::as_string)
    .def(init<const std::string&>(args("json_string")))
    .def("add_short", add_short, return_value_policy<reference_existing_object>())
    .def("add_long", add_long, return_value_policy<reference_existing_object>())
    .def("add_float", add_float, return_value_policy<reference_existing_object>())
    .def("add_string", add_string, return_value_policy<reference_existing_object>())
    .def("set_text", set_text, return_value_policy<reference_existing_object>())
    .def("set_module", set_module, return_value_policy<reference_existing_object>())
    .def("set_class", set_class, return_value_policy<reference_existing_object>())
    .def("set_method", set_method, return_value_policy<reference_existing_object>())
    .def("set_line", &fplog::Message::set_line, return_value_policy<reference_existing_object>()));

    class_<fplog::Message::Mandatory_Fields>("Mandatory_Fields")
    .def_readonly("facility", &fplog::Message::Mandatory_Fields::facility)
    .def_readonly("priority", &fplog::Message::Mandatory_Fields::priority)
    .def_readonly("timestamp", &fplog::Message::Mandatory_Fields::timestamp)
    .def_readonly("hostname", &fplog::Message::Mandatory_Fields::hostname)
    .def_readonly("appname", &fplog::Message::Mandatory_Fields::appname);
    
    class_<fplog::Message::Optional_Fields>("Optional_Fields")
    .def_readonly("text", &fplog::Message::Optional_Fields::text)
    .def_readonly("component", &fplog::Message::Optional_Fields::component)
    .def_readonly("class_name", &fplog::Message::Optional_Fields::class_name)
    .def_readonly("method", &fplog::Message::Optional_Fields::method)
    .def_readonly("module", &fplog::Message::Optional_Fields::module)
    .def_readonly("line", &fplog::Message::Optional_Fields::line)
    .def_readonly("options", &fplog::Message::Optional_Fields::options)
    .def_readonly("encrypted", &fplog::Message::Optional_Fields::encrypted)
    .def_readonly("file", &fplog::Message::Optional_Fields::file)
    .def_readonly("blob", &fplog::Message::Optional_Fields::blob)
    .def_readonly("warning", &fplog::Message::Optional_Fields::warning)
    .def_readonly("sequence", &fplog::Message::Optional_Fields::sequence)
    .def_readonly("batch", &fplog::Message::Optional_Fields::batch);
}
