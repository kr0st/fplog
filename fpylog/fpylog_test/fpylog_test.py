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
        print(str(json_string))

    except Exception as e:
        print(e)
        return 1

    return 0

if __name__ == "__main__":
    sys.exit(main())
