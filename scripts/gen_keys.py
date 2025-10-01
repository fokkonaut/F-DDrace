# generate keys.h file
with open("src/engine/keys.h", "w", encoding="utf-8") as f:

    keynames = []
    for i in range(0, 512):
        keynames += ["&%d" % i]

    print("#ifndef ENGINE_KEYS_H", file=f)
    print("#define ENGINE_KEYS_H", file=f)

    # KEY_EXECUTE already exists on windows platforms
    print("#if defined(CONF_FAMILY_WINDOWS)", file=f)
    print("   #undef KEY_EXECUTE", file=f)
    print("#endif", file=f)

    print('/* AUTO GENERATED! DO NOT EDIT MANUALLY! */', file=f)
    print("enum", file=f)
    print("{", file=f)

    print("\tKEY_FIRST = 0,", file=f)

    highestid = 0
    with open("scripts/SDL_scancode.h", encoding="utf-8") as sdl_file:
        for line_f in sdl_file:
            line = line_f.strip().split("=")
            if len(line) == 2 and "SDL_SCANCODE_" in line[0]:
                key = line[0].strip().replace("SDL_SCANCODE_", "KEY_")
                value = int(line[1].split(",")[0].strip())
                if key[0:2] == "/*":
                    continue
                print("\t%s = %d," % (key, value), file=f)

                keynames[value] = key.replace("KEY_", "").lower()

                if value > highestid:
                    highestid = value

    print("\tKEY_MOUSE_1 = %d," % (highestid + 1), file=f)
    keynames[highestid + 1] = "mouse1"
    print("\tKEY_MOUSE_2 = %d," % (highestid + 2), file=f)
    keynames[highestid + 2] = "mouse2"
    print("\tKEY_MOUSE_3 = %d," % (highestid + 3), file=f)
    keynames[highestid + 3] = "mouse3"
    print("\tKEY_MOUSE_4 = %d," % (highestid + 4), file=f)
    keynames[highestid + 4] = "mouse4"
    print("\tKEY_MOUSE_5 = %d," % (highestid + 5), file=f)
    keynames[highestid + 5] = "mouse5"
    print("\tKEY_MOUSE_6 = %d," % (highestid + 6), file=f)
    keynames[highestid + 6] = "mouse6"
    print("\tKEY_MOUSE_7 = %d," % (highestid + 7), file=f)
    keynames[highestid + 7] = "mouse7"
    print("\tKEY_MOUSE_8 = %d," % (highestid + 8), file=f)
    keynames[highestid + 8] = "mouse8"
    print("\tKEY_MOUSE_WHEEL_UP = %d," % (highestid + 9), file=f)
    keynames[highestid + 9] = "mousewheelup"
    print("\tKEY_MOUSE_WHEEL_DOWN = %d," % (highestid + 10), file=f)
    keynames[highestid + 10] = "mousewheeldown"
    print("\tKEY_MOUSE_9 = %d," % (highestid + 11), file=f)
    keynames[highestid + 11] = "mouse9"
    print("\tKEY_LAST,", file=f)

    print("};", file=f)
    print("", file=f)
    print("#endif", file=f)

# generate keynames.h file
with open("src/engine/client/keynames.h", "w", encoding="utf-8") as f:
    print('/* AUTO GENERATED! DO NOT EDIT MANUALLY! */', file=f)
    print('', file=f)
    print('#ifndef KEYS_INCLUDE', file=f)
    print('#error do not include this header!', file=f)
    print('#endif', file=f)
    print('', file=f)
    print("#include <string.h>", file=f)
    print("", file=f)
    print("const char g_aaKeyStrings[512][20] =", file=f)
    print("{", file=f)
    for n in keynames:
        print('\t"%s",' % n, file=f)
    print("};", file=f)
    print("", file=f)

