from json import load
from polib import POEntry, POFile # type: ignore
from os import chdir, walk, listdir, path, sep
from re import compile
from time import strftime
from sys import argv

from collections import defaultdict

chdir(path.dirname(path.realpath(argv[0])) + "/..")

format = "{0:40} {1:8} {2:8} {3:8}".format
SOURCE_EXTS = [".c", ".cpp", ".h"]

JSON_KEY_AUTHORS="authors"
JSON_KEY_TRANSL="translated strings"
JSON_KEY_UNTRANSL="needs translation"
JSON_KEY_OLDTRANSL="old translations"
JSON_KEY_CTXT="context"
JSON_KEY_OR="or"
JSON_KEY_TR="tr"

SOURCE_LOCALIZE_RE=compile(br'Localize\("(?P<str>([^"\\]|\\.)*)"(, ?"(?P<ctxt>([^"\\]|\\.)*)")?\)')

def parse_source():
	l10n = defaultdict(lambda: [])

	def process_line(line, filename, lineno):
		for match in SOURCE_LOCALIZE_RE.finditer(line):
			str_ = match.group('str').decode()
			ctxt = match.group('ctxt')
			if ctxt is not None:
				ctxt = ctxt.decode()
			l10n[(str_, ctxt)].append((filename, lineno))

	for root, dirs, files in walk("src"):
		for name in files:
			filename = path.join(root, name)
			
			if sep + "external" + sep in filename:
				continue

			if path.splitext(filename)[1] in SOURCE_EXTS:
				# HACK: Open source as binary file.
				# Necessary some of teeworlds source files
				# aren't utf-8 yet for some reason
				for lineno, line in enumerate(open(filename, 'rb')):
					# let lineno start with 1
					lineno += 1
					# process line
					process_line(line, filename, lineno)

	return l10n

def load_languagefile(filename):
	return load(open(filename), strict=False) # accept \t tabs

def write_languagefile(outputfilename, l10n_src, old_l10n_data):
	outputfilename += '.po'

	po = POFile()
	po.metadata = {
		'Project-Id-Version': 'teeworlds-0.7_dev',
		'Report-Msgid-Bugs-To': 'translation@teeworlds.com',
		'POT-Creation-Date': strftime("%Y-%m-%d %H:%M%z"),
		'PO-Revision-Date': strftime("%Y-%m-%d %H:%M%z"),
		'Language-Team': 'Teeworlds Translations <translation@teeworlds.com>',
		'MIME-Version': '1.0',
		'Content-Type': 'text/plain; charset=utf-8',
		'Content-Transfer-Encoding': '8bit',
	}

	translations = {}
	for type_ in (
		JSON_KEY_OLDTRANSL,
		JSON_KEY_UNTRANSL,
		JSON_KEY_TRANSL,
	):
		if type_ not in old_l10n_data:
			continue
		translations.update({
			(t[JSON_KEY_OR], t.get(JSON_KEY_CTXT)): t[JSON_KEY_TR]
			for t in old_l10n_data[type_]
			if t[JSON_KEY_TR]
		})

	all_items = set(translations) | set(l10n_src)
	old_items = set(translations) - set(l10n_src)

	for msg, ctxt in all_items:
		po.append(POEntry(
			msgid=msg,
			msgctxt=ctxt,
			msgstr=translations.get((msg, ctxt), ""),
			obsolete=(msg in old_items),
			occurrences=l10n_src[msg],
		))
	po.save(outputfilename)

if __name__ == '__main__':
	l10n_src = parse_source()

	po = POFile()
	po.metadata = {
		'Project-Id-Version': 'teeworlds-0.7_dev',
		'Report-Msgid-Bugs-To': 'translation@teeworlds.com',
		'POT-Creation-Date': strftime("%Y-%m-%d %H:%M%z"),
		'PO-Revision-Date': 'YEAR-MO-DA HO:MI+ZONE',
		'Language-Team': 'Teeworlds Translations <translation@teeworlds.com>',
		'MIME-Version': '1.0',
		'Content-Type': 'text/plain; charset=utf-8',
		'Content-Transfer-Encoding': '8bit',
	}
	for (msg, ctxt), occurrences in l10n_src.items():
		commenttxt = ctxt
		if(commenttxt):
			commenttxt = 'Context: '+commenttxt
		po.append(POEntry(msgid=msg, msgstr="", occurrences=occurrences, msgctxt=ctxt, comment=commenttxt))
	po.save('datasrc/languages/base.pot')

	for filename in listdir("datasrc/languages"):
		try:
			if (path.splitext(filename)[1] == ".json"
					and filename != "index.json"):
				filename = "datasrc/languages/" + filename
				write_languagefile(filename, l10n_src, load_languagefile(filename))
		except Exception as e:
			print(f"Failed on {filename}, re-raising for traceback: {e}")
			raise
