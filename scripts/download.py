from twlib import fetch_file
from shutil import rmtree
from os import chdir, path, remove
from sys import argv
from zipfile import ZipFile
from distutils.dir_util import copy_tree # type: ignore

chdir(path.dirname(path.realpath(argv[0])) + "/..")

def unzip(filename, where):
    try:
        z = ZipFile(filename, "r")
    except Exception:
        return False

    # extract files
    for name in z.namelist():
        z.extract(name, where)
    z.close()
    return z.namelist()[0]

def downloadAll(targets):
    version = "6c4af62b8c9853bfca1166d672a16abdbf9f0d26"
    url = "https://github.com/teeworlds/teeworlds-libs/archive/{}.zip".format(version)

    # download and unzip
    src_package_libs = fetch_file(url)
    if not src_package_libs:
        raise RuntimeError("couldn't download libs")
    libs_dir = unzip(src_package_libs, ".")
    if not libs_dir:
        raise RuntimeError("couldn't unzip libs")
    libs_dir = "teeworlds-libs-{}".format(version)

    if "sdl" in targets:
        copy_tree(libs_dir + "/sdl/", "other/sdl/")
    if "freetype" in targets:
        copy_tree(libs_dir + "/freetype/", "other/freetype/")

    # cleanup
    try:
        rmtree(libs_dir)
        remove(src_package_libs)
    except Exception: 
        pass

def main():
    import argparse
    p = argparse.ArgumentParser(description="Download freetype and SDL library and header files for Windows.")
    p.add_argument("targets", metavar="TARGET", nargs='+', choices=["sdl", "freetype"], help='Target to download. Valid choices are "sdl" and "freetype"')
    args = p.parse_args()

    downloadAll(args.targets)

if __name__ == '__main__':
    main()
