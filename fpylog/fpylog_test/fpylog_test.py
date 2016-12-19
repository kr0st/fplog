import sys
import os
import imp

cur_path = os.path.dirname(__file__)
sys.path.append(cur_path)


def main():
    print('inside main')

    fpylog = imp.load_dynamic('fpylog', os.path.join(cur_path, 'libfpylog.dylib'))

    print(dir(fpylog))

    try:
        world = fpylog.World()
        world.set("some greeting")
        print(world.greet())

        message = fpylog.Message("warning", "fpylog", "success!")
        json_string = message.as_json_string()

        message2 = fpylog.Message(str(json_string))

        message2.add_short('short', 22)
        message2.add_long('long', 42545)
        message2.add_float('float', 2.42)
        message2.add_string('str', 'just a string')

        print(str(message2.as_json_string()))

        print(str(fpylog.Message.Mandatory_Fields().facility))
        print(str(fpylog.Message.Mandatory_Fields().priority))
        print(str(fpylog.Message.Mandatory_Fields().timestamp))
        print(str(fpylog.Message.Mandatory_Fields().hostname))
        print(str(fpylog.Message.Mandatory_Fields().appname))

        print(str(fpylog.Message.Optional_Fields().text))
        print(str(fpylog.Message.Optional_Fields().component))
        print(str(fpylog.Message.Optional_Fields().class_name))
        print(str(fpylog.Message.Optional_Fields().method))
        print(str(fpylog.Message.Optional_Fields().module))
        print(str(fpylog.Message.Optional_Fields().line))
        print(str(fpylog.Message.Optional_Fields().options))
        print(str(fpylog.Message.Optional_Fields().encrypted))
        print(str(fpylog.Message.Optional_Fields().file))
        print(str(fpylog.Message.Optional_Fields().blob))
        print(str(fpylog.Message.Optional_Fields().warning))
        print(str(fpylog.Message.Optional_Fields().sequence))
        print(str(fpylog.Message.Optional_Fields().batch))

    except Exception as e:
        print(e)
        return 1

    return 0

if __name__ == "__main__":
    sys.exit(main())
