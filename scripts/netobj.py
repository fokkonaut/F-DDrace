from sys import argv
from os import path

line_count = 0

class variable:
    name = "unknown"
    def __init__(self, args, name):
        global line_count
        self.name = name
        self.line = line_count
    def emit_declaration(self):
        return [f"\tint {self.name};"]
    def linedef(self):
        return f"#line {self.line}"
    def emit_secure(self, parent):
        return []
    def emit_unpack(self):
        return [f"msg.{self.name} = msg_unpack_int();"]
    def emit_unpack_check(self):
        return []
    def emit_pack(self):
        return [f"\t\tmsg_pack_int({self.name});"]

class var_any(variable):
    def __init__(self, args, name):
        super().__init__(args, name)

class var_range(variable):
    def __init__(self, args, name):
        self.min = args[0]
        self.max = args[1]
        super().__init__(args, name)
    def emit_unpack_check(self):
        return [f"if(msg.{self.name} < {self.min} || msg.{self.name} > {self.max}) {{ msg_failed_on = \"{self.name}\"; return 0; }}"]
    def emit_secure(self, parent):
        return [self.linedef(), f"obj->{self.name} = netobj_clamp_int(\"{parent.name}.{self.name}\", obj->{self.name}, {self.min}, {self.max});"]

class var_string(variable):
    def __init__(self, args, name):
        super().__init__(args, name)
    def emit_declaration(self):
        return [f"\tconst char *{self.name};"]
    def emit_unpack(self):
        return [f"msg.{self.name} = msg_unpack_string();"]
    def emit_pack(self):
        return [f"\t\tmsg_pack_string({self.name}, -1);"]

class object:
    def __init__(self, line):
        fields = line.split()
        self.name = fields[1]
        self.extends = None
        if len(fields) == 4 and fields[2] == "extends":
            self.extends = fields[3]
        self.enum_name = f"NETOBJTYPE_{self.name.upper()}"
        self.struct_name = f"NETOBJ_{self.name.upper()}"
        self.members = []

    def parse(self, lines):
        global line_count
        for index in range(0, len(lines)):
            line_count += 1
            line = lines[index]
            if not len(line):
                continue

            if line == "end":
                return lines[index+1:]
            else:
                # check for argument
                fields = line.split(")", 1)
                if len(fields) == 2:
                    names = [n.strip() for n in fields[1].split(",")]
                    line = fields[0].split("(", 1)
                    type = line[0]
                    args = [a.strip() for a in line[1].split(",")]
                else:
                    line = fields[0].split(None, 1)
                    type = line[0]
                    args = []
                    names = [n.strip() for n in line[1].split(",")]

                for name in names:
                    create_string = 'var_%s(%s, "%s")' % (type, args, name)
                    new_member = eval(create_string)
                    self.members += [new_member]

        raise Exception("Parse error")

    def emit_declaration(self):
        lines = []
        if self.extends:
            lines += [f"struct {self.struct_name} : public NETOBJ_{self.extends.upper()}\n {{" ]
        else:
            lines += [f"struct {self.struct_name}\n {{" ]
        for m in self.members:
            lines += m.emit_declaration()
        lines += ["};"]
        return lines

    def emit_secure(self):
        lines = []
        for m in self.members:
            lines += m.emit_secure(self)
        return lines

class message:
    def __init__(self, line):
        fields = line.split()
        self.name = fields[1]
        self.enum_name = f"NETMSGTYPE_{self.name.upper()}"
        self.struct_name = f"NETMSG_{self.name.upper()}"
        self.members = []

    def parse(self, lines):
        global line_count
        for index in range(0, len(lines)):
            line_count += 1
            line = lines[index]
            if not len(line):
                continue

            if line == "end":
                return lines[index+1:]
            else:
                # check for argument
                fields = line.split(")", 1)
                if len(fields) == 2:
                    names = [n.strip() for n in fields[1].split(",")]
                    line = fields[0].split("(", 1)
                    type = line[0]
                    args = [a.strip() for a in line[1].split(",")]
                else:
                    line = fields[0].split(None, 1)
                    type = line[0]
                    args = []
                    names = [n.strip() for n in line[1].split(",")]

                for name in names:
                    create_string = 'var_%s(%s, "%s")' % (type, args, name)
                    new_member = eval(create_string)
                    self.members += [new_member]

        raise Exception("Parse error")

    def emit_declaration(self):
        lines = []
        lines += [f"struct {self.struct_name}\n {{" ]
        for m in self.members:
            lines += m.emit_declaration()
        lines += ["\tvoid pack(int flags)"]
        lines += ["\t{" ]
        lines += [f"\t\tmsg_pack_start({self.enum_name}, flags);"]
        for m in self.members:
            lines += m.emit_pack()
        lines += ["\t\tmsg_pack_end();"]
        lines += ["\t}"]
        lines += ["};"]
        return lines

    def emit_unpack(self):
        lines = []
        for m in self.members:
            lines += m.emit_unpack()
        for m in self.members:
            lines += m.emit_unpack_check()
        return lines

    def emit_pack(self):
        lines = []
        for m in self.members:
            lines += m.emit_pack()
        return lines

class event(object):
    def __init__(self, line):
        super().__init__(line)
        self.enum_name = "NETEVENTTYPE_%s" % self.name.upper()
        self.struct_name = "NETEVENT_%s" % self.name.upper()

class raw_reader:
    def __init__(self):
        self.raw_lines = []
    def parse(self, lines):
        global line_count
        for index in range(0, len(lines)):
            line_count += 1
            line = lines[index]
            if not len(line):
                continue

            if line == "end":
                return lines[index+1:]
            else:
                self.raw_lines += [line]

        raise Exception("Parse error")

class proto:
    def __init__(self):
        self.objects = []
        self.messages = []
        self.source_raw = []
        self.header_raw = []

def load(filename):
    # read the file
    global line_count
    line_count = 0
    lines = [line.split("//", 2)[0].strip() for line in open(filename).readlines()]

    p = proto()

    while len(lines):
        line_count += 1
        line = lines[0]

        if not len(line):
            del lines[0]
            continue

        fields = line.split(None, 1)

        del lines[0]

        if fields[0] == "object":
            new_obj = object(line)
            lines = new_obj.parse(lines)
            p.objects += [new_obj]
        elif fields[0] == "message":
            new_msg = message(line)
            lines = new_msg.parse(lines)
            p.messages += [new_msg]
        elif fields[0] == "event":
            new_obj = event(line)
            lines = new_obj.parse(lines)
            p.objects += [new_obj]
        elif fields[0] == "raw_source":
            raw = raw_reader()
            lines = raw.parse(lines)
            p.source_raw += raw.raw_lines
        elif fields[0] == "raw_header":
            raw = raw_reader()
            lines = raw.parse(lines)
            p.header_raw += raw.raw_lines
        else:
            print(f"error, strange line: {line}")

    return p

def emit_header_file(f, p):
    for line in p.header_raw:
        print(line, file=f)

    if 1: # emit the enum table for objects
        print("enum {", file=f)
        print("\tNETOBJTYPE_INVALID=0,", file=f)
        for obj in p.objects:
            print(f"\t{obj.enum_name},", file=f)
        print("\tNUM_NETOBJTYPES", file=f)
        print("};", file=f)
        print("", file=f)

    if 1: # emit the enum table for messages
        print("enum {", file=f)
        print("\tNETMSGTYPE_INVALID=0,", file=f)
        for msg in p.messages:
            print(f"\t{msg.enum_name},", file=f)
        print("\tNUM_NETMSGTYPES", file=f)
        print("};", file=f)
        print("", file=f)

    print("int netobj_secure(int type, void *data, int size);", file=f)
    print("const char *netobj_get_name(int type);", file=f)
    print("int netobj_num_corrections();", file=f)
    print("const char *netobj_corrected_on();", file=f)
    print("", file=f)
    print("void *netmsg_secure_unpack(int type);", file=f)
    print("const char *netmsg_get_name(int type);", file=f)
    print("const char *netmsg_failed_on();", file=f)
    print("", file=f)

    for obj in p.objects:
        for line in obj.emit_declaration():
            print(line, file=f)
        print("", file=f)

    for msg in p.messages:
        for line in msg.emit_declaration():
            print(line, file=f)
        print("", file=f)

def emit_source_file(f, p, protofilename):
    print(f"#line 1 \"{path.abspath(protofilename).replace('\\', '\\\\')}\"", file=f)

    for line in p.source_raw:
        print(line, file=f)

    print("const char *msg_failed_on = \"\";", file=f)
    print("const char *obj_corrected_on = \"\";", file=f)
    print("static int num_corrections = 0;", file=f)
    print("int netobj_num_corrections() { return num_corrections; }", file=f)
    print("const char *netobj_corrected_on() { return obj_corrected_on; }", file=f)
    print("const char *netmsg_failed_on() { return msg_failed_on; }", file=f)
    print("", file=f)
    print("static int netobj_clamp_int(const char *error_msg, int v, int min, int max)", file=f)
    print("{", file=f)
    print("\tif(v<min) { obj_corrected_on = error_msg; num_corrections++; return min; }", file=f)
    print("\tif(v>max) { obj_corrected_on = error_msg; num_corrections++; return max; }", file=f)
    print("\treturn v;", file=f)
    print("}", file=f)
    print("", file=f)

    if 1: # names
        print("static const char *object_names[] = {", file=f)
        print('\t"invalid",', file=f)
        for obj in p.objects:
            print(f'\t"{obj.name}",', file=f)
        print('\t""', file=f)
        print("};", file=f)
        print("", file=f)

    if 1: # secure functions
        print("static int secure_object_invalid(void *data, int size) { return 0; }", file=f)
        for obj in p.objects:
            print(f"static int secure_{obj.name}(void *data, int size)", file=f)
            print("{", file=f)
            print(f"\t{obj.struct_name} *obj = ({obj.struct_name} *)data;", file=f)
            print("\t(void)obj;", file=f) # to get rid of "unused variable" warning
            print(f"\tif(size != sizeof({obj.struct_name})) return -1;", file=f)
            if obj.extends:
                print(f"\tif(secure_{obj.extends}(data, sizeof(NETOBJ_{obj.extends.upper()})) != 0) return -1;", file=f)
            for line in obj.emit_secure():
                print("\t" + line, file=f)
            print("\treturn 0;", file=f)
            print("}", file=f)
            print("", file=f)

    if 1: # secure function table
        print("typedef int(*SECUREFUNC)(void *data, int size);", file=f)
        print("static SECUREFUNC secure_funcs[] = {", file=f)
        print("\tsecure_object_invalid,", file=f)
        for obj in p.objects:
            print(f"\tsecure_{obj.name},", file=f)
        print("\t0x0", file=f)
        print("};", file=f)
        print("", file=f)

    if 1:
        print("int netobj_secure(int type, void *data, int size)", file=f)
        print("{", file=f)
        print("\tif(type < 0 || type >= NUM_NETOBJTYPES) return -1;", file=f)
        print("\treturn secure_funcs[type](data, size);", file=f)
        print("};", file=f)
        print("", file=f)

    if 1:
        print("const char *netobj_get_name(int type)", file=f)
        print("{", file=f)
        print("\tif(type < 0 || type >= NUM_NETOBJTYPES) return \"(invalid)\";", file=f)
        print("\treturn object_names[type];", file=f)
        print("};", file=f)
        print("", file=f)

    if 1: # names
        print("static const char *message_names[] = {", file=f)
        print('\t"invalid",', file=f)
        for msg in p.messages:
            print(f'\t"{msg.name}",', file=f)
        print('\t""', file=f)
        print("};", file=f)
        print("", file=f)

    if 1: # secure functions
        print("static void *secure_unpack_invalid() { return 0; }", file=f)
        for msg in p.messages:
            print(f"static void *secure_unpack_{msg.name}()", file=f)
            print("{", file=f)
            print(f"\tstatic {msg.struct_name} msg;", file=f)
            for line in msg.emit_unpack():
                print("\t" + line, file=f)
            print("\treturn &msg;", file=f)
            print("}", file=f)
            print("", file=f)

    if 1: # secure function table
        print("typedef void *(*SECUREUNPACKFUNC)();", file=f)
        print("static SECUREUNPACKFUNC secure_unpack_funcs[] = {", file=f)
        print("\tsecure_unpack_invalid,", file=f)
        for msg in p.messages:
            print(f"\tsecure_unpack_{msg.name},", file=f)
        print("\t0x0", file=f)
        print("};", file=f)
        print("", file=f)

    if 1:
        print("void *netmsg_secure_unpack(int type)", file=f)
        print("{", file=f)
        print("\tvoid *msg;", file=f)
        print("\tmsg_failed_on = \"\";", file=f)
        print("\tif(type < 0 || type >= NUM_NETMSGTYPES) return 0;", file=f)
        print("\tmsg = secure_unpack_funcs[type]();", file=f)
        print("\tif(msg_unpack_error()) return 0;", file=f)
        print("\treturn msg;", file=f)
        print("};", file=f)
        print("", file=f)

    if 1:
        print("const char *netmsg_get_name(int type)", file=f)
        print("{", file=f)
        print("\tif(type < 0 || type >= NUM_NETMSGTYPES) return \"(invalid)\";", file=f)
        print("\treturn message_names[type];", file=f)
        print("};", file=f)
        print("", file=f)

if __name__ == "__main__":
    if argv[1] == "header":
        p = load(argv[2])
        with open(argv[3], "w") as f:
            emit_header_file(f, p)
    elif argv[1] == "source":
        p = load(argv[2])
        with open(argv[3], "w") as f:
            emit_source_file(f, p, argv[2])
    else:
        raise ValueError("invalid command")
