from __future__ import print_function
from __future__ import unicode_literals
from __future__ import division
from __future__ import absolute_import
from builtins import int
from builtins import str
from future import standard_library
standard_library.install_aliases()
from builtins import object
from re import compile, search
from avatar.bintools.gdb.parse_stream import ParseStream, ParseError


ESCAPE_CHARS = {'n':'\n', '\\':'\\', '"':'"', 't':'\t'}
def parse_cstring(s):
    """
    const ==>
        c-string
    """
    s.expect_char('"')
    sb = []
    while s.check_limit():
        c = s.char()
        if c == '\\':
            sb.append(ESCAPE_CHARS[s.char()])
        elif c == '"':
            break
        else:
            sb.append(c)
    return ''.join(sb)


def parse_tuple(s):
    """
    tuple ==>
        "{}" | "{" result ( "," result )* "}"
    """
    s.expect_char('{')
    if s.peek() == '}':
        return {}
    tuple = parse_results(s)
    s.expect_char('}')
    return tuple


def parse_list(s):
    """
    list ==>
        "[]" | "[" value ( "," value )* "]" | "[" result ( "," result )* "]" 
    """
    s.expect_char('[')
    c = s.peek()
    if c in '"{[':
        list = []
        while s.check_limit():
            list.append(parse_value(s))
            if not s.next_char(','):
                break
    elif c == ']':
        s.skip()
        return []
    else:
        list = parse_results(s)
    s.expect_char(']')
    return list


def parse_value(s):
    """
    value ==>
        const | tuple | list
    """
    c = s.peek()
    if c == '"':
        value = parse_cstring(s)
    elif c == '{':
        value = parse_tuple(s)
    elif c == '[':
        value = parse_list(s)
    
    return value


def parse_variable(s):
    """
    variable ==>
        string
    """
    sb = []
    while s.check_limit():
        c = s.char()
        if c == '=':
            break
        sb.append(c)
    return ''.join(sb)


def parse_results(s):
    """
    results ==>
        result ( "," result )*
    
    result ==>
        variable "=" value
    """
    s.next_char(',')
    results = {}
    while s.check_limit():
        var, value = (parse_variable(s), parse_value(s))
        results[var] = value
        if not s.next_char(','):
            break
    return results


class Async(object):
    EXEC, STATUS, NOTIFY = 0, 1, 2
    MAP = {'*':0, '+':1, '=':2}
    PATTERN = compile('(?P<type>[\*\+=])(?P<class>[^,]+)(?P<results>,.+)?')
   
    def __init__(self, line):
        m = search(Async.PATTERN, line)
        if m is None:
            raise ParseError('Error parsing async msg: %s' % line)
        self.type = Async.MAP[m.group('type')]
        self.klass = m.group('class')
        self.results = None
        if m.group('results') is not None:
            self.results = parse_results(ParseStream(m.group('results')))
            
    def __str__(self):
        result = []
        result.append({0:"*",1:'+', 2:'='}[self.type])
        result.append(self.klass)
        if hasattr(self, "results"):
            result.append(",")
            result.append(",".join(["%s=%s" % (key, val) for (key, val) in self.results.items()]))
            
        return "".join(result)
        


class Result(object):
    PATTERN = compile('(?P<token>\d+)\^(?P<class>done|running|connected|error|exit)(?P<results>,.+)?')
    
    def __init__(self, line):
        m = search(Result.PATTERN, line)
        if m is None:
            raise ParseError('Error parsing result: %s' % line)
        self.token = int(m.group('token'))
        self.klass = m.group('class')
        self.results = None
        if m.group('results') is not None:
            self.results = parse_results(ParseStream(m.group('results')))


class Stream(object):
    CONSOLE, TARGET, ERROR_LOG = 0, 1, 2
    MAP = {'~':0, '@':1, '&':2}
    
    def __init__(self, line):
        self.type = Stream.MAP[line[0]]
        self.string = parse_cstring(ParseStream(line[1:]))


def parse(string):
    if string and string[0] in ['*', '+', '=']:
        msg = Async(string)
    
    elif string and string[0] in ['~', '@', '&']:
        msg = Stream(string)
    
    elif string and string[0].isdigit():
        msg = Result(string)
    
    else:
        raise ParseError('Unhandled Output: %s' % string)
    
    return msg

if __name__ == '__main__':
    TEST_LINES = (
        ('~"done.\n"',
            [('type', Stream.CONSOLE), ('string', 'done.\n')]),
        
        ('&".gdbinit: No such file or directory.\n"',
            [('type', Stream.ERROR_LOG), ('string', '.gdbinit: No such file or directory.\n')]),
        
        ('1457^done',
            [('token', '1457'), ('klass', 'done')]),
        
        ('1461^done,value="(gdb) "',
            [('token', '1461'), ('klass', 'done'), ('results', {'value':"(gdb) "})]),
        
        ('=thread-group-created,id="42000"',
            [('type', Async.NOTIFY), ('klass', 'thread-group-created'), ('results', {'id':"42000"})]),
        
        ('=thread-created,id="1",group-id="42000""',
            [('type', Async.NOTIFY), ('klass', 'thread-created'), ('results', {'id':"1", 'group-id':"42000"})]),
        
        ('*stopped,frame={addr="0x00341850",func="_start",args=[],from="/lib/ld-linux.so.2"},thread-id="1",stopped-threads="all",core="0"',
            [('type', Async.EXEC), ('klass', 'stopped'), ('results', {'frame':{'addr':"0x00341850",'func':"_start",'args':[],'from':"/lib/ld-linux.so.2"}, 'thread-id':"1", 'stopped-threads':"all", 'core':"0"})]),
        ('1465^connected', 
            [('token', '1465'), ('klass', 'connected')]),
        
        ('1466^error,msg="No /proc directory: \'/proc/42000\'"', 
            [('token', '1466'), ('klass', 'error'), ('results', {'msg': "No /proc directory: '/proc/42000'"})]),
        
        ('1472^done,stack=[frame={level="0",addr="0x00341850",func="_start",from="/lib/ld-linux.so.2"}]', 
            [('token', '1472'), ('klass', 'done'), ('results', {'stack': {'frame':{'level':'0','addr':"0x00341850", 'func':"_start",'from':"/lib/ld-linux.so.2"}}})]),
        
        ('1473^done,changed-registers=["0","1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16","17","18","19","20","21","22","23","24","25","26","27","28","29","30","31","32","33","34","35","36","37","38","39","40","41"]',
            [('token', '1473'), ('klass', 'done'), ('results', {'changed-registers': ["0","1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16","17","18","19","20","21","22","23","24","25","26","27","28","29","30","31","32","33","34","35","36","37","38","39","40","41"]})]),
        
        ('1476^done,bkpt={number="1",type="breakpoint",disp="del",enabled="y",addr="0x080483ed",func="main",file="../src/TestGDB.c",fullname="/home/emilmont/Workspaces/idea/TestGDB/src/TestGDB.c",line="5",times="0",original-location="main"}',
            [('token', '1476'), ('klass', 'done'), ('results', {'bkpt':{'number':"1",'type':"breakpoint",'disp':"del",'enabled':"y",'addr':"0x080483ed",'func':"main",'file':"../src/TestGDB.c",'fullname':"/home/emilmont/Workspaces/idea/TestGDB/src/TestGDB.c",'line':"5",'times':"0",'original-location':"main"}})]),
        
        ('1477^running',
            [('token', '1477'), ('klass', 'running'), ('results', None)]),
        
        ('*running,thread-id="all"',
            [('type', Async.EXEC), ('klass', 'running'), ('results', {'thread-id':'all'})]),
        
        ('*stopped,thread-id="1",stopped-threads="all",core="0"',
            [('type', Async.EXEC), ('klass', 'stopped'), ('results', {'thread-id':'1', 'stopped-threads':'all', 'core':'0'})]),
        
        ('*stopped,reason="breakpoint-hit",disp="del",bkptno="1",frame={addr="0x080483ed",func="main",args=[],file="../src/TestGDB.c",fullname="/home/emilmont/Workspaces/idea/TestGDB/src/TestGDB.c",line="5"},thread-id="1",stopped-threads="all",core="0"',
            [('type', Async.EXEC), ('klass', 'stopped'), ('results', {'reason':'breakpoint-hit', 'disp':'del', 'bkptno':'1', 'frame':{'addr':"0x080483ed",'func':"main",'args':[],'file':"../src/TestGDB.c",'fullname':"/home/emilmont/Workspaces/idea/TestGDB/src/TestGDB.c",'line':"5"}, 'thread-id':"1", 'stopped-threads':"all",'core':"0"})]),
    
        ('1487^done,asm_insns=[src_and_asm_line={line="4",file="../src/TestGDB.c",line_asm_insn=[{address="0x080483e4",func-name="main",offset="0",inst="push   %ebp"},{address="0x080483e5",func-name="main",offset="1",inst="mov    %esp,%ebp"},{address="0x080483e7",func-name="main",offset="3",inst="and    $0xfffffff0,%esp"},{address="0x080483ea",func-name="main",offset="6",inst="sub    $0x20,%esp"}]},src_and_asm_line={line="5",file="../src/TestGDB.c",line_asm_insn=[{address="0x080483ed",func-name="main",offset="9",inst="movl   $0x0,0x1c(%esp)"}]},src_and_asm_line={line="6",file="../src/TestGDB.c",line_asm_insn=[{address="0x080483f5",func-name="main",offset="17",inst="jmp    0x8048411 <main+45>"},{address="0x0804840c",func-name="main",offset="40",inst="addl   $0x1,0x1c(%esp)"},{address="0x08048411",func-name="main",offset="45",inst="cmpl   $0x9,0x1c(%esp)"},{address="0x08048416",func-name="main",offset="50",inst="jle    0x80483f7 <main+19>"}]},src_and_asm_line={line="7",file="../src/TestGDB.c",line_asm_insn=[{address="0x080483f7",func-name="main",offset="19",inst="mov    $0x80484e0,%eax"},{address="0x080483fc",func-name="main",offset="24",inst="mov    0x1c(%esp),%edx"},{address="0x08048400",func-name="main",offset="28",inst="mov    %edx,0x4(%esp)"},{address="0x08048404",func-name="main",offset="32",inst="mov    %eax,(%esp)"},{address="0x08048407",func-name="main",offset="35",inst="call   0x804831c <printf@plt>"}]},src_and_asm_line={line="8",file="../src/TestGDB.c",line_asm_insn=[]},src_and_asm_line={line="9",file="../src/TestGDB.c",line_asm_insn=[]},src_and_asm_line={line="10",file="../src/TestGDB.c",line_asm_insn=[{address="0x08048418",func-name="main",offset="52",inst="mov    $0x0,%eax"}]},src_and_asm_line={line="11",file="../src/TestGDB.c",line_asm_insn=[{address="0x0804841d",func-name="main",offset="57",inst="leave  "},{address="0x0804841e",func-name="main",offset="58",inst="ret    "}]}]',
            [('token', '1487'), ('klass', 'done'), ('results', {'asm_insns':{
                'src_and_asm_line':{'line':'4', 'file':'../src/TestGDB.c', 'line_asm_insn':[
                    {'address':'0x080483e4', 'func-name':'main', 'offset':'0', 'inst':"push   %ebp"},
                    {'address':'0x080483e5', 'func-name':'main', 'offset':'1', 'inst':"mov    %esp,%ebp"},
                    {'address':'0x080483e7', 'func-name':'main', 'offset':'3', 'inst':"mov    $0xfffffff0,%esp"},
                    {'address':"0x080483ea", 'func-name':"main", 'offset':"6", 'inst':"sub    $0x20,%esp"},
                ]},
                'src_and_asm_line':{'line':'5', 'file':'../src/TestGDB.c', 'line_asm_insn':[
                    {'address':'0x080483ed', 'func-name':'main', 'offset':'9', 'inst':"movl   $0x0,0x1c(%esp)"},
                ]},
                'src_and_asm_line':{'line':'6', 'file':'../src/TestGDB.c', 'line_asm_insn':[
                    {'address':'0x080483f5', 'func-name':'main', 'offset':'17', 'inst':"jmp    0x8048411 <main+45>"},
                    {'address':'0x0804840c', 'func-name':'main', 'offset':'40', 'inst':"addl   $0x1,0x1c(%esp)"},
                    {'address':'0x08048411', 'func-name':'main', 'offset':'45', 'inst':"cmpl   $0x9,0x1c(%esp)"},
                    {'address':"0x08048416", 'func-name':"main", 'offset':"50", 'inst':"jle    0x80483f7 <main+19>"},
                ]},
                'src_and_asm_line':{'line':'7', 'file':'../src/TestGDB.c', 'line_asm_insn':[
                    {'address':'0x080483f7', 'func-name':'main', 'offset':'19', 'inst':"jmp    0x8048411 <main+45>"},
                    {'address':'0x080483fc', 'func-name':'main', 'offset':'24', 'inst':"addl   $0x1,0x1c(%esp)"},
                    {'address':'0x08048400', 'func-name':'main', 'offset':'28', 'inst':"cmpl   $0x9,0x1c(%esp)"},
                    {'address':"0x08048404", 'func-name':"main", 'offset':"32", 'inst':"mov    %eax,(%esp)"},
                    {'address':"0x08048407", 'func-name':"main", 'offset':"35", 'inst':"call   0x804831c <printf@plt>"},
                ]},
                'src_and_asm_line':{'line':'8', 'file':'../src/TestGDB.c', 'line_asm_insn':[]},
                'src_and_asm_line':{'line':'9', 'file':'../src/TestGDB.c', 'line_asm_insn':[]},
                'src_and_asm_line':{'line':'10', 'file':'../src/TestGDB.c', 'line_asm_insn':[
                    {'address':'0x08048418', 'func-name':'main', 'offset':'52', 'inst':"mov    $0x0,%eax"},
                ]},
                'src_and_asm_line':{'line':'11', 'file':'../src/TestGDB.c', 'line_asm_insn':[
                    {'address':'0x0804841d', 'func-name':'main', 'offset':'57', 'inst':"leave  "},
                    {'address':'0x0804841e', 'func-name':'main', 'offset':'58', 'inst':"ret    "},
                ]},
                
            }})]),
        ('1488^done,stack-args=[frame={level="0",args=[]}]',
            [('token', '1488'), ('klass', 'done'), ('results', {'stack-args':{'frame':{'level':'0', 'args':[]}}})]),
        
        ('1489^done,locals=[name="i"]',
            [('token', '1489'), ('klass', 'done'), ('results', {'locals':{'name':'i'}})]),
        
        ('1498^done,changelist=[{name="var1",in_scope="true",type_changed="false",has_more="0"}]',
            [('token', '1498'), ('klass', 'done'), ('results', {'changelist':[{'name':'var1', 'in_scope':'true', 'type_changed':'false', 'has_more':'0'}]})]),
        
        ('1509^done,changelist=[]',
            [('token', '1509'), ('klass', 'done'), ('results', {'changelist':[]})]),
    )
    
    for line, expected in TEST_LINES:
        msg = parse(line)
        for key, value in expected:
            attr = getattr(msg, key)
            assert attr == value, '"%s" != "%s"' % (str(attr), str(value))
    
    print('OK')
