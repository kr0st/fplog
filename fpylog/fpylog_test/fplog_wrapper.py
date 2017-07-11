import sys
import os
import imp
import inspect
import array

cur_path = os.path.dirname(__file__)
sys.path.append(cur_path)

lib = None

try:
    lib = imp.load_dynamic('fpylog', os.path.join(cur_path, 'libfpylog3.dylib'))
except Exception:
    lib = imp.load_dynamic('fpylog', os.path.join(cur_path, 'libfpylog.so'))


def make_log_message(log_msg, prio, stack_frame=1):
    msg = lib.Message(prio, lib.get_facility(), log_msg)
    frame, filename, line_number, function_name, lines, index = inspect.stack()[stack_frame]

    msg.set_module(os.path.basename(filename))
    msg.set_line(line_number)

    if 'self' in frame.f_locals:
        msg.set_class(frame.f_locals['self'].__class__.__name__)

    msg.set_method(function_name)
    return msg


def write_raw(raw_msg):
    lib.write(raw_msg)


def write_log(log_msg, prio, json_str=None, stack_frame=2):
    fplog_msg = make_log_message(log_msg, prio, stack_frame)
    if json_str is not None:
        fplog_msg.add_json(json_str)
    lib.write(fplog_msg)


def debug(log_msg, json_str=None):
    write_log(log_msg, lib.Prio.debug, json_str, stack_frame=3)


def info(log_msg, json_str=None):
    write_log(log_msg, lib.Prio.info, json_str, stack_frame=3)


def warn(log_msg, json_str=None):
    write_log(log_msg, lib.Prio.warning, json_str, stack_frame=3)


def err(log_msg, json_str=None):
    write_log(log_msg, lib.Prio.error, json_str, stack_frame=3)


def crit(log_msg, json_str=None):
    write_log(log_msg, lib.Prio.critical, json_str, stack_frame=3)
