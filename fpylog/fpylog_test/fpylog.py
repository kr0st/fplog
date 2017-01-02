import sys
import os
import imp
import inspect
import array

cur_path = os.path.dirname(__file__)
sys.path.append(cur_path)

lib = imp.load_dynamic('fpylog', os.path.join(cur_path, 'libfpylog.dylib'))


def warn(log_msg):
    lib.write(lib.Message(lib.Prio.warning, lib.get_facility(), log_msg))
