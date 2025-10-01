from os import path, walk
from re import sub, compile

alphanum = "0123456789abcdefghijklmnopqrstuvwzyxABCDEFGHIJKLMNOPQRSTUVWXYZ_"
cpp_keywords = ["auto", "const", "double", "float", "int", "short", "struct", "unsigned", # C
"break", "continue", "else", "for", "long", "signed", "switch", "void",
"case", "default", "enum", "goto", "register", "sizeof", "typedef", "volatile",
"char", "do", "extern", "if", "return", "static", "union", "while",

"asm", "dynamic_cast", "namespace", "reinterpret_cast", "try", # C++
"bool", "explicit", "new", "static_cast", "typeid",
"catch", "false", "operator", "template", "typename",
"class", "friend", "private", "this", "using",
"const_cast", "inline", "public", "throw", "virtual",
"delete", "mutable", "protected", "true", "wchar_t"]

allowed_words = []

allowed_words += ["qsort"] # stdio / stdlib
allowed_words += ["size_t", "cosf", "sinf", "asinf", "acosf", "atanf", "powf", "fabs", "rand", "powf", "fmod", "sqrtf"] # math.h
allowed_words += ["time_t", "time", "strftime", "localtime"] # time.h
allowed_words += [ # system.h
    "int64",
    "dbg_assert", "dbg_msg", "dbg_break", "dbg_logger_stdout", "dbg_logger_debugger", "dbg_logger_file",
    "mem_alloc", "mem_zero", "mem_free", "mem_copy", "mem_move", "mem_comp", "mem_stats", "total_allocations", "allocated",
    "thread_create", "thread_sleep", "lock_wait", "lock_create", "lock_release", "lock_destroy", "swap_endian",
    "io_open", "io_read", "io_read", "io_write", "io_flush", "io_close", "io_seek", "io_skip", "io_tell", "io_length",
    "str_comp", "str_length", "str_quickhash", "str_format", "str_copy", "str_comp_nocase", "str_sanitize", "str_append",
    "str_comp_num", "str_find_nocase", "str_sanitize_strong", "str_uppercase", "str_toint", "str_tofloat",
    "str_utf8_encode", "str_utf8_rewind", "str_utf8_forward", "str_utf8_decode", "str_sanitize_cc", "str_skip_whitespaces",
    "fs_makedir", "fs_listdir", "fs_storage_path", "fs_is_dir",
    "net_init", "net_addr_comp", "net_host_lookup", "net_addr_str", "type", "port", "net_addr_from_str",
    "net_udp_create", "net_udp_send", "net_udp_recv", "net_udp_close", "net_socket_read_wait",
    "net_stats", "sent_bytes", "recv_bytes", "recv_packets", "sent_packets",
    "time_get", "time_freq", "time_timestamp"]

allowed_words += ["vec2", "vec3", "vec4", "round", "clamp", "length", "dot", "normalize", "frandom", "mix", "distance", "min",
        "closest_point_on_line", "max", "absolute"] # math.hpp
allowed_words += [  # tl
    "array", "sorted_array", "string",
    "all", "sort", "add", "remove_index", "remove", "delete_all", "set_size",
    "base_ptr", "size", "swap", "empty", "front", "pop_front", "find_binary", "find_linear", "clear", "range", "end", "cstr",
    "partition_linear", "partition_binary"]
allowed_words += ["fx2f", "f2fx"] # fixed point math

def CheckIdentifier(ident):
    return False

class Checker:
    def CheckStart(self, checker, filename):
        pass
    def CheckLine(self, checker, line):
        pass
    def CheckEnd(self, checker):
        pass

class FilenameExtentionChecker(Checker):
    def __init__(self):
        self.allowed = [".cpp", ".h"]
    def CheckStart(self, checker, filename):
        ext = path.splitext(filename)[1]
        if ext not in self.allowed:
            checker.Error(f"file extention '{ext}' is not allowed")

class IncludeChecker(Checker):
    def __init__(self):
        self.disallowed_headers = ["stdio.h", "stdlib.h", "string.h", "memory.h"]
    def CheckLine(self, checker, line):
        if "#include" in line:
            include_file = ""
            if '<' in line:
                include_file = line.split('<')[1].split(">")[0]
            elif '"' in line:
                include_file = line.split('"')[1]
            if include_file in self.disallowed_headers:
                checker.Error(f"{include_file} is not allowed")

class HeaderGuardChecker(Checker):
    def CheckStart(self, checker, filename):
        self.check = ".h" in filename
        self.guard = "#ifndef " + filename[4:].replace("/", "_").replace(".hpp", "").replace(".h", "").upper() + "_H"
    def CheckLine(self, checker, line):
        if self.check:
            self.check = False
            if line.strip() ==  self.guard:
                pass
            else:
                checker.Error(f"malformed or missing header guard. Should be '{self.guard}'")

class CommentChecker(Checker):
    def CheckLine(self, checker, line):
        if line.strip()[-2:] == "*/" and "/*" in line:
            checker.Error("single line multiline comment")

class FileChecker:
    def __init__(self):
        self.checkers = []
        self.checkers += [FilenameExtentionChecker()]
        self.checkers += [HeaderGuardChecker()]
        self.checkers += [IncludeChecker()]
        self.checkers += [CommentChecker()]

    def Error(self, errormessage):
        self.current_errors += [(self.current_line, errormessage)]

    def CheckLine(self, line):
        for c in self.checkers:
            c.CheckLine(self, line)
        return True

    def CheckFile(self, filename):
        self.current_file = filename
        self.current_line = 0
        self.current_errors = []
        for checker in self.checkers:
            checker.CheckStart(self, filename)

        with open(filename, encoding="utf-8", errors="ignore") as fileobj:
            for line in fileobj.readlines():
                self.current_line += 1
                if "ignore_check" in line:
                    continue
                self.CheckLine(line)
        for checker in self.checkers:
            checker.CheckEnd(self)

    def GetErrors(self):
        return self.current_errors

def cstrip(lines):
    d = ""
    for line in lines:
        if "ignore_convention" in line:
            continue
        line = sub(r"^[\t ]*#.*", "", line)
        line = sub(r"//.*", "", line)
        line = sub(r'\".*?\"', '"String"', line) # remove strings
        d += line.strip() + " "
    d = sub(r'\/\*.*?\*\/', "", d) # remove /* */ comments
    d = d.replace("\t", " ") # tab to space
    d = sub(r"  *", " ", d) # remove double spaces
    d = d.strip()
    i = 1
    while i < len(d)-2:
        if d[i] == ' ':
            if not (d[i-1] in alphanum and d[i+1] in alphanum):
                d = d[:i] + d[i+1:]
        i += 1
    return d

def get_identifiers(data):
    idents = {}
    data = " "+data+" "
    regexp = compile(r"[^a-zA-Z0-9_][a-zA-Z_][a-zA-Z0-9_]+[^a-zA-Z0-9_]")
    start = 0
    while 1:
        m = regexp.search(data, start)
        if m is None:
            break
        start = m.end()-1
        name = data[m.start()+1:m.end()-1]
        if name in idents:
            idents[name] += 1
        else:
            idents[name] = 1
    return idents

grand_total = 0
grand_offenders = 0

gen_html = 1

if gen_html:
    print("<head>")
    print('<link href="/style.css" rel="stylesheet" type="text/css" />')
    print("</head>")
    print("<body>")

    print('<div id="outer">')

    print('<div id="top_left"><div id="top_right"><div id="top_mid">')
    print('<a href="/"><img src="/images/twlogo.png" alt="teeworlds logo" /></a>')
    print('</div></div></div>')

    print('<div id="menu_left"><div id="menu_right"><div id="menu_mid">')
    print('</div></div></div>')

    print('<div id="tlc"><div id="trc"><div id="tb">&nbsp;</div></div></div>')
    print('<div id="lb"><div id="rb"><div id="mid">')
    print('<div id="container">')

    print('<p class="topic_text">')
    print('<h1>Code Refactoring Progress</h1>')
    print('''This is generated by a script that find identifiers in the code
    that doesn't conform to the code standard. Right now it only shows headers
    because they need to be fixed before we can do the rest of the source.
    This is a ROUGH estimate of the progress''')
    print('</p>')

    print('<p class="topic_text">')
    print('<table>')

line_order = 1
total_files = 0
complete_files = 0
total_errors = 0

for (root,dirs,files) in walk("src"):
    for filename in files:
        filename = path.join(root, filename)
        if "/." in filename or "/external/" in filename or "/base/" in filename or "/generated/" in filename:
            continue
        if "src/osxlaunch/client.h" in filename: # ignore this file, ObjC file
            continue
        if "e_config_variables.h" in filename: # ignore config files
            continue
        if "src/game/variables.hpp" in filename: # ignore config files
            continue

        if not (".hpp" in filename or ".h" in filename or ".cpp" in filename):
            continue

        f = FileChecker()
        f.CheckFile(filename)
        num_errors = len(f.GetErrors())
        total_errors += num_errors

        if num_errors:
            print(f"<tr style=\"background: #e0e0e0\"><td colspan=\"2\">{filename}, {num_errors} errors</td></tr>")
            for line, msg in f.GetErrors():
                print(f'<tr"><td>{line}</td><td>{msg}</td></tr>')

if gen_html:
    print("</table>")
    print(f"<h1>{total_errors} errors</h1>")
    print("</p>")
    print("<div style='clear:both;'></div>")
    print('</div>')
    print('</div></div></div>')

    print('<div id="blc"><div id="brc"><div id="bb">&nbsp;</div></div></div>')
    print('</div>')

    print("</body>")
