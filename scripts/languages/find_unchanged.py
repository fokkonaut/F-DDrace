#!/usr/bin/env python3
from os import chdir, path
from sys import argv

from twlang import translations

chdir(path.dirname(__file__) + "/../..")

if len(argv) < 2:
	raise ValueError("usage: python find_unchanged.py <file>")
infile = argv[1]

trans = translations(infile)
for tran, (_, expr, _) in trans.items():
	if tran == expr:
		print(tran)
