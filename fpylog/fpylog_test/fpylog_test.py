import sys
import os
import imp
import inspect
import array
import fplog_wrapper as fpylog
import random
import string
import time

cur_path = os.path.dirname(__file__)
sys.path.append(cur_path)


class Fpylog_Test:
    def __init__(self):
        fpylog.crit('message from class')


def spam_test():
    print('Spamming with random messages as fast as we can!!!')

    counter = 0

    while True:

        arr = ''.join(random.choice(string.ascii_uppercase + string.digits) for _ in range(135))

        counter += 1

        if counter % 13 == 0:
            fpylog.crit(arr)

        if counter % 11 == 0:
            fpylog.err(arr)

        if counter % 9 == 0:
            fpylog.warn(arr)

        if counter % 7 == 0:
            fpylog.info(arr)

        if counter % 5 == 0:
            fpylog.debug(arr)

        if counter > 1000000:
            counter = 0


def manual_test():

    in_str = ''
    print('Please write log messages to send, type "exit" to quit.')
    while in_str != 'exit':
        in_str = str(input())
        fpylog.info(in_str)


def pure_lib_test():

    fpylog_lib = None

    try:
        fpylog_lib = imp.load_dynamic('fpylog', os.path.join(cur_path, 'libfpylog3.dylib'))
    except Exception:
        fpylog_lib = imp.load_dynamic('fpylog', os.path.join(cur_path, 'libfpylog.so'))

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
        fpylog_lib.initlog("fpylog_pure_lib_test", "fpylog_lib", "18759_18760", False, filter)

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

    fpylog.lib.initlog("fpylog_test", "fpylog", "18759_18760", False, prio_filter)

    class_log_test = Fpylog_Test()

    raw_msg = fpylog.make_log_message('raw msg test', fpylog.lib.Prio.critical)
    raw_msg.add_float('pointless_number', -44.3)

    fpylog.write_raw(raw_msg)

    fpylog.write_log('writing generic log message', fpylog.lib.Prio.info)

    #manual_test()
    #spam_test()

    fpylog.lib.shutdownlog()

    return 0

if __name__ == "__main__":
    sys.exit(main())
