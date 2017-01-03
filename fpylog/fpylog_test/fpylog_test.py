import sys
import os
import imp
import inspect
import array
import fpylog

cur_path = os.path.dirname(__file__)
sys.path.append(cur_path)


class Fpylog_Test:
    def __init__(self):
        fpylog.crit('message from class')


def manual_test():

    in_str = ''
    print('Please write log messages to send, type "exit" to quit.')
    while in_str != 'exit':
        in_str = str(raw_input())
        fpylog.info(in_str)


def pure_lib_test():

    fpylog_lib = imp.load_dynamic('fpylog', os.path.join(cur_path, 'libfpylog.dylib'))

    print(dir(fpylog_lib))

    try:
        world = fpylog_lib.World()
        world.set("some greeting")
        print(world.greet())

        message = fpylog_lib.Message("warning", "user", "success!")
        json_string = message.as_json_string()

        message2 = fpylog_lib.Message(str(json_string))

        message2.add_short('short', 22)
        message2.add_long('long', 42545)
        message2.add_float('float', 2.42)
        message2.add_string('str', 'just a string')

        print(str(fpylog_lib.Message.Mandatory_Fields.facility))
        print(str(fpylog_lib.Message.Mandatory_Fields.priority))
        print(str(fpylog_lib.Message.Mandatory_Fields.timestamp))
        print(str(fpylog_lib.Message.Mandatory_Fields.hostname))
        print(str(fpylog_lib.Message.Mandatory_Fields.appname))

        print(str(fpylog_lib.Message.Optional_Fields.text))
        print(str(fpylog_lib.Message.Optional_Fields.component))
        print(str(fpylog_lib.Message.Optional_Fields.class_name))
        print(str(fpylog_lib.Message.Optional_Fields.method))
        print(str(fpylog_lib.Message.Optional_Fields.module))
        print(str(fpylog_lib.Message.Optional_Fields.line))
        print(str(fpylog_lib.Message.Optional_Fields.options))
        print(str(fpylog_lib.Message.Optional_Fields.encrypted))
        print(str(fpylog_lib.Message.Optional_Fields.file))
        print(str(fpylog_lib.Message.Optional_Fields.blob))
        print(str(fpylog_lib.Message.Optional_Fields.warning))
        print(str(fpylog_lib.Message.Optional_Fields.sequence))
        print(str(fpylog_lib.Message.Optional_Fields.batch))

        message2.set_class('superclass')
        message2.set_module(__file__)
        message2.set_line(inspect.currentframe().f_back.f_lineno)
        message2.set_text('new text')

        print(str(message2.as_json_string()))

        arr = bytearray(os.urandom(5))
        arr2 = array.array('i', [2]) * 5

        print(list(arr))
        print(list(arr2))

        file = fpylog_lib.File('warning', 'some_file', list(arr))
        file2 = fpylog_lib.File('warning', 'another_file', list(arr2))

        print(str(file.as_message().as_json_string()))
        print(str(file2.as_message().as_json_string()))

        print('Should print out word "debug": ' + fpylog_lib.Prio.debug)

        filter = fpylog_lib.make_prio_filter("prio_filter")
        filter.add_all_above(fpylog_lib.Prio.debug, False)
        fpylog_lib.initlog("fpylog_pure_lib_test", "fpylog_lib", "18749_18750", False, filter)

        fpylog_lib.write(message2)
        fpylog_lib.write(file.as_message())
        fpylog_lib.write(file2.as_message())

        fpylog_lib.shutdownlog()

    except Exception as e:
        print(e)
        return 1


def main():

    print('inside main')

    pure_lib_test()

    prio_filter = fpylog.lib.make_prio_filter("prio_filter")
    prio_filter.add_all_above(fpylog.lib.Prio.debug, False)

    fpylog.lib.initlog("fpylog_test", "fpylog", "18749_18750", False, prio_filter)

    class_log_test = Fpylog_Test()

    manual_test()

    fpylog.lib.shutdownlog()

    return 0

if __name__ == "__main__":
    sys.exit(main())
