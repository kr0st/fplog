import sys
import os
import imp
import inspect
import array

cur_path = os.path.dirname(__file__)
sys.path.append(cur_path)

lib = imp.load_dynamic('fpylog', os.path.join(cur_path, 'libfpylog.dylib'))


def write_log(log_msg, prio):
    msg = lib.Message(prio, lib.get_facility(), log_msg)
    frame, filename, line_number, function_name, lines, index = inspect.stack()[1]

    msg.set_module(filename)
    msg.set_line(line_number)

    if 'self' in frame.f_locals:
        msg.set_class(frame.f_locals['self'].__class__.__name__)

    msg.set_method(function_name)

    lib.write(msg)


def debug(log_msg):
    write_log(log_msg, lib.Prio.debug)


def info(log_msg):
    write_log(log_msg, lib.Prio.info)


def warn(log_msg):
    write_log(log_msg, lib.Prio.warning)


def err(log_msg):
    write_log(log_msg, lib.Prio.error)


def crit(log_msg):
    write_log(log_msg, lib.Prio.critical)

