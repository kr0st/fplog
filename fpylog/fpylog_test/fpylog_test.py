import sys

sys.path.append("~/Source/fplog/fpylog/fpylog_test/")

import fpylog


def main():
    print('inside main')
    world = fpylog.World()
    world.set("some greeting")
    print(world.greet())
    return 0

if __name__ == "__main__":
    sys.exit(main())
