#!/usr/bin/env python3
from os import chdir, path
from copy_fix import copy_fix

from twlang import languages

chdir(path.dirname(__file__) + "/../..")

for lang in languages():
	content = copy_fix(lang, delete_unused=True, append_missing=True, delete_empty=False)
	with open(lang, "w", encoding="utf-8") as f:
		f.write(content)
