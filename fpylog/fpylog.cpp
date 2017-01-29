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

namespace fpylog
{
    File::File(const char* prio, const char* name, boost::python::list& content)
    {
        std::vector<unsigned char> file;
        for (int i = 0; i < len(content); ++i)
            file.push_back(boost::python::extract<unsigned char>(content[i]));

        file_ = new fplog::File(prio, name, &(file[0]), file.size());
    }

    fplog::Priority_Filter* make_prio_filter(const char* filter_id)
    {
        return new fplog::Priority_Filter(filter_id);
    }

    void initlog(const char* appname, const char* facility, const char* uid, bool use_async, fplog::Filter_Base* filter)
    {
        fplog::initlog(appname, uid, 0, use_async);
        fplog::openlog(facility, filter);
    }
};

BOOST_PYTHON_MODULE(fpylog)
{
    class_<fpylog::World>("World")
    .def("greet", &fpylog::World::greet)
    .def("set", &fpylog::World::set);

    def("initlog", &fpylog::initlog);
    def("get_facility", &fplog::get_facility);
    def("shutdownlog", &fplog::shutdownlog);
    def("write", &fplog::write);

    class_<fplog::Facility>("Facility")
    .def_readonly("system", &fplog::Facility::system)
    .def_readonly("user", &fplog::Facility::user)
    .def_readonly("security", &fplog::Facility::security)
    .def_readonly("fglog", &fplog::Facility::fplog);

    class_<fplog::Prio>("Prio")
    .def_readonly("emergency", &fplog::Prio::emergency)
    .def_readonly("alert", &fplog::Prio::alert)
    .def_readonly("critical", &fplog::Prio::critical)
    .def_readonly("error", &fplog::Prio::error)
    .def_readonly("warning", &fplog::Prio::warning)
    .def_readonly("notice", &fplog::Prio::notice)
    .def_readonly("info", &fplog::Prio::info)
    .def_readonly("debug", &fplog::Prio::debug);

    class_<fplog::Filter_Base, boost::noncopyable>("Filter_Base", no_init);
    class_<fplog::Priority_Filter, bases<fplog::Filter_Base>, boost::noncopyable>("Priority_Filter", init<const char*>(args("filter_id")))
    .def("add", &fplog::Priority_Filter::add)
    .def("remove", &fplog::Priority_Filter::remove)
    .def("add_all_above", &fplog::Priority_Filter::add_all_above)
    .def("add_all_below", &fplog::Priority_Filter::add_all_below);
    
    def("make_prio_filter", &fpylog::make_prio_filter, return_value_policy<reference_existing_object>());

    class_<fpylog::File, boost::noncopyable>("File", init<const char*, const char*, boost::python::list&>(args("prio", "name", "content")))
    .def("as_message", &fpylog::File::as_message);

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
