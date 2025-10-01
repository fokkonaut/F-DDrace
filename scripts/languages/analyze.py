#!/usr/bin/env python3
from os import chdir, path
from sys import argv

from twlang import languages, localizes, translations

chdir(path.dirname(__file__) + "/../..")

if len(argv) > 1:
	langs = argv[1:]
else:
	langs = languages()
local = localizes()
table = []
for lang in langs:
	trans = translations(lang)
	empty = 0
	supported = 0
	unused = 0
	for tran, (_, expr, _) in trans.items():
		if not expr:
			empty += 1
		else:
			if tran in local:
				supported += 1
			else:
				unused += 1
	table.append([lang, len(trans), empty, len(local)-supported, unused])

table.sort(key=lambda row: row[3])
table = [["filename", "total", "empty", "missing", "unused"]] + table
s = [[str(e) for e in row] for row in table]
lens = [max(map(len, col)) for col in zip(*s)]
fmt = "    ".join(f"{{:{x}}}" for x in lens)
t = [fmt.format(*row) for row in s]
print("\n".join(t))
