#!/usr/bin/python

import re
import os
import sys
import random

class MyDict(dict):
    def __init__(self, init=lambda:[], desc=['tag']):
        self.init = self._genInit(init, desc)
        self._order = []
        super(type(self),self).__init__()
    
    def _genInit(self, init, desc):
        self.desc = desc
        if len(desc) > 1:
            return lambda:MyDict(init=init, desc=desc[:-1])
        else:
            assert(init == None or callable(init))
            return init
    
    def __setitem__(self, key, value):
        if key not in self:
            self._order.append(key)
        super(type(self),self).__setitem__(key, value)
    
    def __getitem__(self, key):
        if key not in self and self.init is not None:
            self[key] = self.init()
        return super(type(self),self).__getitem__(key)
    
    def __iter__(self):
        return iter(self._order)
    
    def __str__(self):
        return '[%s] %s' % (
            ':'.join(self.desc), super(type(self),self).__str__())
    
    def keys(self):
        # Return new list to avoid changing self._order
        return [i for i in self._order]
    
    def __delitem__(self, key):
        raise AttributeError('Deleting items not allowed')
    
    def pop(self, key):
        raise AttributeError('Popping items not allowed')

class Environ(object):
    def __getitem__(self, key):
        if key in os.environ:
            return os.environ[key]
        else:
            return ''
environ = Environ()

class Shell(object):
    def __init__(self):
        self.exitCmds = []
    
    def __del__(self):
        self.doExitCmds()
    
    def sysrun(self, cmd, echo=True):
        if echo:
            print('$ ' + cmd)
        return os.system(cmd)
    
    def addExitCmds(self, cmds):
        if isinstance(cmds,str):
            cmds = [cmds]
        self.exitCmds += cmds
    
    def doExitCmds(self):
        for cmd in self.exitCmds:
            self.sysrun(cmd)
        self.exitCmds = []
    
    def run(self, cmd, throw=True, echo=True):
        rc = self.sysrun(cmd, echo=echo)
        if rc and throw:
            self.doExitCmds()
            raise Exception('Failed to run command(%s) rc(%d)' % (cmd,rc))
        return rc

class File(object):
    @classmethod
    def read(cls, path):
        fd = open(path)
        lines = fd.read().split('\n')
        fd.close()
        return lines
    
    @classmethod
    def write(cls, path, lines=[], mode='w'):
        fd = open(path, mode)
        fd.write('\n'.join(lines))
        fd.close()

class KSym(object):
    kallsyms = []
    
    @classmethod
    def ksymTable(cls, update=False):
        if not cls.kallsyms or update:
            if environ['USER'] != 'root':
                raise Exception('Run script by root')
            cls.kallsyms = File.read('/proc/kallsyms')
        return cls.kallsyms
    
    def __init__(self, update=False):
        self.kallsyms = type(self).ksymTable(update)
        self.modules = {}
    
    def lookupSymbol(self, sym='', mod='', exact=False, getAll=False):
        mod = self._getMod(self._validModName(mod))
        modDesc = 'module(%s)' % mod['name']
        if self._isKernel(mod['name']):
            modDesc = 'kernel'
        
        validSymName = self._validSymName(sym)
        if not validSymName:
            raise Exception("Symbol's name(%s) is not supported" % sym)
        if validSymName not in mod['symtab']:
            raise Exception('Symbol(%s) not found in %s' % (sym, modDesc))
        d = mod['symtab'][validSymName]
        
        if getAll:
            addrs = []
            for s in d:
                addrs += d[s]
            return addrs
        
        if exact or '.' in sym:
            if sym not in d:
                raise Exception('Exact symbol(%s) not found in %s' % (
                    sym, modDesc))
            symName = sym
        else:
            if len(d) != 1:
                raise Exception('Too many symbols with name(%s) in %s' % (
                    sym, modDesc))
            symName = d.keys()[0]
        
        if len(d[symName]) != 1:
            raise Exception('Too many symbols with same name(%s) in %s' % (
                symName, modDesc))
        
        return d[symName][0]
    
    def getSymtab(self, mod):
        symtab = list(self._getMod(self._validModName(mod))['symtab'].keys())
        symtab.sort()
        return symtab
    
    def _getMod(self, modName):
        if modName not in self.modules:
            self.modules[modName] = self._loadModImpl(modName)
        return self.modules[modName]
    
    def _loadModImpl(self, modName):
        # Load symbols
        if self._isKernel(modName):
            symbols = [sym for sym in self.kallsyms if sym and '[' not in sym]
        else:
            pattern = '[%s]' % modName
            symbols = [sym for sym in self.kallsyms if pattern in sym]
        
        # if not symbols:
            # raise Exception('No symbols found in module(%s)' % modName)
        
        # Order symbols
        #
        # An example of structure of symtab:
        #   symtab = {
        #       'raidz_rec_q_coeff' : {
        #           'raidz_rec_q_coeff.isra.2' : [
        #               'ffffffffc0bb8190',
        #           ]
        #           'raidz_rec_q_coeff.isra.4' : [
        #               'ffffffffc0c47170',
        #               'ffffffffc0c4bb80',
        #               'ffffffffc0c77ab0',
        #           ]
        #           'raidz_rec_q_coeff.isra.3' : [
        #               'ffffffffc0c9d790',
        #               'ffffffffc0ca1eb0',
        #           ]
        #       }
        #   }
        symtab = MyDict(desc=['validSymName','symName'])
        for sym in symbols:
            arr = sym.split()
            addr,symName,validSymName = arr[0],arr[2],self._validSymName(arr[2])
            if validSymName:
                symtab[validSymName][symName].append(addr)
        
        return {
            'name' : modName,
            'symtab' : symtab
        }
    
    def _validModName(self, name):
        if name:
            return name
        else:
            return '_'
    
    def _isKernel(self, name):
        return self._validModName(name) == '_'
    
    def _validSymName(self, name):
        if name.startswith('.'):
            return ''
        elif name.startswith('__func__.'):
            return ''
        elif name.startswith('__FUNCTION__.'):
            return ''
        elif name.startswith('__key.'):
            return ''
        elif name.startswith('CSWTCH.'):
            return ''
        else:
            return name.split('.')[0]

class Code(object):
    def __init__(self, type='', name='', impl=[]):
        assert(type in ['func', 'macro', 'typedef'])
        assert(name)
        assert(isinstance(impl,list))
        
        self.desc = {
            'tag'  : ('%s:%s' % (type,name)),
            'type' : type,
            'name' : name,
            'impl' : impl
        }
    
    def add(self, impl):
        if isinstance(impl,str):
            self.desc['impl'].append(impl)
        else: # impl is a List
            self.desc['impl'] += impl
    
    def removeLast(self, lineCnt=1):
        self.desc['impl'] = self.desc['impl'][:-lineCnt]
    
    def __getitem__(self, key):
        return self.desc[key]
    
    def __bool__(self):
        return bool(self.desc['impl'])
    __nonzero__ = __bool__
    
    def __eq__(self, other):
        return self['impl'] == other['impl']
    
    def __ne__(self, other):
        return self['impl'] != other['impl']

class CodeForTrace(object):
    def __init__(self, ksym=KSym()):
        self.ksym = ksym
    
    def genCodes(self, modules, quiet=True):
        self.traces = MyDict(desc=['localMod','remoteMod','name'])
        self.parseModules(modules)
        if not self.traces:
            if not quiet:
                print('No modules need trace.')
            return []
        
        codes = []
        codes.append(Code(type='macro', name='ARRAY_SIZE', impl=[
            '#ifndef ARRAY_SIZE',
            '#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))',
            '#endif',
        ]))
        codes.append(Code(type='macro', name='DEF_FUNC', impl=[
            '#define DEF_FUNC(type,name,addr) type name = (type)0x##addr##UL'
        ]))
        codes.append(Code(type='macro', name='DEF_TRACE', impl=[
            '#define DEF_TRACE(addr) (kdbg_trace_def_t*)0x##addr##UL'
        ]))
        codes.append(Code(type='macro', name='REG_TRACE', impl=[
            '#define REG_TRACE(lmod, rmod, traces, trace_cnt)' + '\t'*3 + '\\',
            '\tdo {' + '\t'*8 + '\\',
            '\t\tint errcnt = reg_fn(rmod, traces);' + '\t'*3 + '\\',
            '\t\tprintk("KDBG:KSYM: Register traces module"' + '\t'*2 + '\\',
            '\t\t    "(%s->%s), total(%ld), errcnt(%d)",' + '\t'*3 + '\\',
            '\t\t    lmod, rmod, trace_cnt, errcnt);' + '\t'*3 +'\\',
            '\t} while (0)'
        ]))
        codes.append(Code(type='typedef', name='kdbg_trace_def_t', impl=[
            'typedef struct kdbg_trace_def kdbg_trace_def_t;'
        ]))
        codes.append(Code(type='typedef', name='reg_trace_fn_t', impl=[
            'typedef int (*reg_trace_fn_t)(const char *, kdbg_trace_def_t**);'
        ]))
        
        codes.append(Code(type='func', name='do_trace', impl=[
            'static void',
            'do_trace(void)',
            '{'
        ]))
        
        for localMod in self.traces:
            self.traceModule(localMod, codes[-1], quiet)
        
        codes[-1].add('}')
        return codes
    
    def parseModules(self, modules):
        for modName in modules:
            self.parseModule(modName)
    
    def parseModule(self, localMod):
        remoteModSet,pattern = set(),self.traceName(flag='ptr')
        for sym in self.ksym.getSymtab(localMod):
            grp = re.search(pattern, sym)
            if grp:
                remoteModSet.add(grp.groups()[0])
        
        pattern = self.traceName(mod='_', flag='def')
        for remoteMod in remoteModSet:
            for sym in self.ksym.getSymtab(remoteMod):
                grp = re.search(pattern, sym)
                if grp:
                    name = grp.groups()[0]
                    addrs = self.ksym.lookupSymbol(mod=remoteMod, getAll=True,
                        sym=self.traceName(mod='_', name=name, flag='def'))
                    self.traces[localMod][remoteMod][name] += addrs
    
    def traceModule(self, localMod, code, quiet):
        if not self.traces[localMod]:
            if not quiet:
                print('No traces found in module(%s)' % localMod)
                return
        
        code.add('\t/* module:%s */ {' % localMod)
        code.add('\t\t/* import function: kdbg_trace_register */')
        code.add('\t\tDEF_FUNC(reg_trace_fn_t, reg_fn, %s);' %
            self.ksym.lookupSymbol(mod=localMod, sym='kdbg_trace_register'))
        code.add('')
        
        for remoteMod in self.traces[localMod]:
            varTracesName = '%s_%s_traces' % (localMod, remoteMod)
            code.add('\t\tstatic kdbg_trace_def_t *%s[] = {' % varTracesName)
            nameList = [name for name in self.traces[localMod][remoteMod]]
            nameList.sort()
            for name in nameList:
                traceDesc = '%s->%s:%s' % (localMod, remoteMod, name)
                for addr in self.traces[localMod][remoteMod][name]:
                    code.add('\t\t\tDEF_TRACE(%s), /* %s */' % (addr,traceDesc))
                    if not quiet:
                        print('Trace %s: trace-def-addr=%s' % (traceDesc, addr))
            code.add('\t\t\tDEF_TRACE(0)')
            code.add('\t\t};')
            code.add('\t\tREG_TRACE("%s", "%s", %s,' % (
                localMod, remoteMod, varTracesName))
            code.add('\t\t    ARRAY_SIZE(%s) - 1);' % varTracesName)
            code.add('')
        
        code.removeLast()
        code.add('\t}')
    
    def traceName(self, mod='(\w+)', name='(\w+)', flag='(\w+)'):
        return '_kdbg_trace_%s_v1_%s_%s_ff_aa_55_' % (flag,mod,name)

class CodeForImport(object):
    def __init__(self, ksym=KSym()):
        self.ksym = ksym
    
    def genCodes(self, modules, quiet=True):
        self.modules = modules
        self.imports = MyDict(
            desc=['localMod','remoteMod','symName','importNdx'],
            init=lambda:{'localAddr':'','remoteAddr':''}
        )
        
        for localMod in self.modules:
            self.parseModule(localMod)
        if not self.imports:
            if not quiet:
                print('No modules import symbols from others')
            return []
        
        codes = [Code(type='typedef', name='kdbg_hold_module_fn_t', impl=[
            'typedef int (*kdbg_hold_module_fn_t)(const char *mod_name);'
        ])]
        codes.append(Code(type='macro', name='CALL_HOLD_MODULE', impl=[
            '#define CALL_HOLD_MODULE(mod,addr)' + '\t'*5 + '\\',
            '\t(((kdbg_hold_module_fn_t)0x##addr##UL)(#mod))'
        ]))
        codes.append(Code(type='macro', name='ASSIGN_SYMBOL_ADDR', impl=[
            '#define ASSIGN_SYMBOL_ADDR(lmod,rmod,name,ndx,laddr,raddr)\t\t\\',
            '\tprintk("KDBG:KSYM: import "#lmod"->"#rmod":"#name":"#ndx\t\\',
            '\t    " = *("#laddr") = ("#raddr")\\n");\t\t\t\t\\',
            '\t*(void**)0x##laddr##UL = (void*)0x##raddr##UL'
        ]))
        
        codes.append(Code(type='func', name='do_import', impl=[
            'static void',
            'do_import(void)',
            '{'
        ]))
        
        cnt = 0
        self.msgs = []
        for localMod in self.imports:
            cnt += self.importModule(localMod, codes[-1])
        assert(cnt > 0)
        codes[-1].removeLast() # Remove last one empty line
        codes[-1].add('}')
        
        if not quiet:
            print('\n'.join(self.msgs))
        return codes
    
    def parseModule(self, localMod):
        pattern = self.ksymImportName()
        self.setUsrConf(localMod)
        for symName in self.ksym.getSymtab(localMod):
            grp = re.search(pattern, symName)
            if grp:
                sym,mod,ndx = grp.groups()
                self.imports[localMod][mod][sym][ndx] = {
                    'localAddr' : self.lookupSymbol(mod=localMod,sym=symName),
                    'remoteAddr' : self.lookupImportSymbol(
                        mod=mod,sym=sym,ndx=ndx)
                }
    
    def importModule(self, localMod, code):
        msgs = []
        
        importFlag = '__kdbg_ksym_imported'
        importAddr = self.lookupSymbol(mod=localMod, sym=importFlag)
        msgs.append('%s:&%s = (int*)0x%s' % (localMod, importFlag, importAddr))
        
        holdFunc = 'kdbg_hold_module'
        holdAddr = self.lookupSymbol(mod=localMod, sym=holdFunc)
        msgs.append('%s:&%s = (int*)0x%s' % (localMod, holdFunc, holdAddr))
        
        code.add('\t/* Import symbols for module(%s) */' % localMod)
        code.add('\tdo {')
        code.add('\t\tint *imported = (int*)0x%sUL;' % importAddr)
        code.add('\t\tif (*imported) {')
        code.add('\t\t\tprintk("KDBG:KSYM: module(kdbg) is ' +
            'already imported\\n");')
        code.add('\t\t\tbreak;')
        code.add('\t\t}')
        code.add('')
        
        cnt = 0
        for mod in self.imports[localMod]:
            code.add('\t\t/* Import symbols from module(%s) */' % mod)
            code.add('\t\tif (!CALL_HOLD_MODULE(%s,%s)) {' % (mod,holdAddr))
            code.add('\t\t\tprintk("KDBG:KSYM: Failed to hold module(' +
                mod + ') by module(' + localMod + ')\\n");')
            code.add('\t\t\tbreak;')
            code.add('\t\t}')
            code.add('')
            
            for sym in self.imports[localMod][mod]:
                for ndx in self.imports[localMod][mod][sym]:
                    info = self.imports[localMod][mod][sym][ndx]
                    code.add('\t\tASSIGN_SYMBOL_ADDR(%s, %s, %s, %s, %s, %s);'
                        % (localMod, mod, sym, ndx,
                            info['localAddr'], info['remoteAddr']))
                    msgs.append('%s->%s:%s:%s = *(%s) = (%s)' % (localMod, mod,
                        sym, ndx, info['localAddr'], info['remoteAddr']))
                    cnt += 1
            code.add('')
        
        code.add('\t\tprintk("KDBG:KSYM: ' + localMod +
            ':__kdbg_ksym_imported = *(' + importAddr + ') = 1\\n");')
        code.add('\t\t*imported = 1;')
        code.add('\t} while (0);')
        code.add('')
        
        self.msgs += msgs
        return cnt
    
    def ksymImportName(self, mod='(\w+)', name='(\w+)', ndx='(\w+)'):
        return '__ksym_1537_%s_2489_mod_%s_ndx_%s_ffff_' % (name,mod,ndx)
    
    def lookupImportSymbol(self, mod='', sym='', ndx='0'):
        key = ':'.join([mod,sym,ndx])
        if key not in self.usrSymtab:
            return self.ksym.lookupSymbol(mod=mod, sym=sym)
        
        val = self.usrSymtab[key]['val']
        if val.startswith('addr:'):
            return val[5:]
        elif val.startswith('exact:'):
            return self.ksym.lookupSymbol(mod=mod, sym=val[6:], exact=True)
        else: # val.startswith('sym:')
            return self.ksym.lookupSymbol(mod=mod, sym=val[4:])
    
    def lookupSymbol(self, mod='', sym='', exact=False):
        return self.ksym.lookupSymbol(mod=mod, sym=sym, exact=exact)
    
    def setUsrConf(self, localMod):
        self.usrSymtab = {}
        for usrConf in self.modules[localMod]:
            lineNo = 0
            for line in File.read(usrConf):
                lineNo += 1
                arr = self.split(line, '=')
                if len(arr) == 2 and not arr[0].startswith('#'):
                    self.setSym(arr[0], arr[1], usrConf, lineNo)
    
    def setSym(self, symName, symVal, confFile, lineNo):
        #
        # Format of symName: '<[modName]:symName[:ndx]>'
        #
        arr = self.split(symName, ':') + ['0']
        assert(len(arr) in [3,4])
        modName,symName,ndx = arr[0:3]
        
        #
        # Format of symVal: '[exact:]<symName>' or '<addressInHexFormat>'
        #          valType: 'sym','addr','exact'
        #
        valType = 'sym'
        hexPattern = '^[0-9a-fA-F]+$'
        
        if symVal[0:2] in ['0x','0X']:
            assert(re.match(hexPattern, symVal[2:]))
            symVal = symVal[2:]
            valType = 'addr'
        elif re.match(hexPattern, symVal):
            valType = 'addr'
        elif ':' in symVal:
            arr = self.split(symVal, ':')
            if arr[0] == 'exact' and len(arr) == 2:
                valType = 'exact'
                symVal = arr[1]
        
        key,val = ':'.join([modName,symName,ndx]),':'.join([valType,symVal])
        if key not in self.usrSymtab:
            self.usrSymtab[key] = {
                'key'  : key,
                'val'  : val,
                'file' : confFile,
                'line' : lineNo,
            }
        elif self.usrSymtab[key]['val'] != val:
            prev = self.usrSymtab[key]
            msg  = 'Repeat config name=%s, ' % key
            msg += 'previous={%s}@[%s,%d], ' % (
                prev['val'], prev['file'],prev['line'])
            msg += 'current={%s}@[%s,%d]' % (
                val, confFile, lineNo)
            raise Exception(msg)
    
    def split(self, s, sep):
        return [i.strip() for i in s.split(sep)]

class ModMaker(object):
    def __init__(self):
        self.clearCodes()
    
    def clearCodes(self):
        self.implements = MyDict()
    
    def addCodes(self, codes):
        for code in codes:
            if code['tag'] not in self.implements:
                self.implements[code['tag']] = code
            elif self.implements[code['tag']] != code:
                msgs = []
                msgs.append('Definitions conflict:')
                msgs.append('=' * 40 + '>>>')
                msgs += self.implements[code['tag']]['impl']
                msgs.append('=' * 40 + '...')
                msgs += code['impl']
                msgs.append('=' * 40 + '<<<')
                raise Exception(msgs)
    
    def genMakefile(self, path, modname):
        lines = []
        lines.append('ifeq ($(KERNELRELEASE),)')
        lines.append('')
        lines.append('.PHONY: all modules clean')
        lines.append('')
        lines.append('KBUILD_DIR := /lib/modules/$(shell uname -r)/build')
        lines.append('')
        lines.append('all: modules')
        lines.append('')
        lines.append('modules clean:')
        lines.append('\t$(MAKE) -C $(KBUILD_DIR) M=$(shell pwd) $@')
        lines.append('')
        lines.append('else')
        lines.append('')
        lines.append('MODULE         := ' + modname)
        lines.append('obj-m          += $(MODULE).o')
        lines.append('$(MODULE)-objs += drv.o')
        lines.append('')
        lines.append('ccflags-y  += -std=gnu99 -Wall -Werror')
        lines.append('ccflags-y  += -Wno-declaration-after-statement')
        lines.append('')
        lines.append('endif')
        lines.append('')
        File.write(path, lines)
    
    def genCCodes(self, path):
        lines = []
        lines.append('#include <linux/init.h>')
        lines.append('#include <linux/module.h>')
        lines.append('#include <linux/kernel.h>')
        lines.append('')
        
        for tag in self.implements:
            lines += self.implements[tag]['impl']
            lines.append('')
        
        lines.append('static int __init')
        lines.append('ksym_drv_init(void)')
        lines.append('{')
        
        for tag in [f for f in self.implements if f.startswith('func:')]:
            lines.append('\t%s();' % self.implements[tag]['name'])
        
        lines.append('\treturn (0);')
        lines.append('}')
        lines.append('')
        lines.append('static void __exit')
        lines.append('ksym_drv_exit(void)')
        lines.append('{')
        lines.append('}')
        lines.append('')
        lines.append('module_init(ksym_drv_init);')
        lines.append('module_exit(ksym_drv_exit);')
        lines.append('')
        lines.append('MODULE_LICENSE("GPL");')
        lines.append('MODULE_AUTHOR("zhengyp");')
        lines.append('MODULE_DESCRIPTION("kernel debugger");')
        lines.append('MODULE_VERSION("1.0");')
        lines.append('')
        
        File.write(path, lines)
    
    def run(self):
        rndstr = lambda n : [str(random.randint(100,999)) for i in range(n)]
        workspace = '/tmp/ksym.' + '.'.join(rndstr(3))
        modname = 'ksym_' + '_'.join(rndstr(3))
        
        shell = Shell()
        shell.run('mkdir ' + workspace)
        if not environ['KSYM_RESERVE_WORKSPACE'] in ['yes','true']:
            shell.addExitCmds('rm -rf ' + workspace)
        
        self.genMakefile(workspace + '/Makefile', modname)
        self.genCCodes(workspace + '/drv.c')
        shell.run('make -C ' + workspace)
        shell.run('insmod %s/%s.ko' % (workspace,modname))
        shell.run('rmmod %s' % modname)

class Main(object):
    def __init__(self):
        self.appname = os.path.basename(sys.argv[0])
        self.args = sys.argv[1:]
        self.coders = [CodeForTrace, CodeForImport]
    
    def run(self):
        if len(self.args) == 0:
            self.usage()
            sys.exit(0)
        else:
            self.parseArgs(self.args).execute()
    
    def usage(self):
        print('Usage: %s <mod>.ko [*.conf ...] [<mod1>.ko *.conf]'
            % self.appname)
        print('       %s <path>/<head>.in [<path1>/<head1>.in ...]'
            % self.appname)
    
    def error(self, msg):
        sys.stderr.write('Error: *** ' + msg + '\n')
    
    def parseArgs(self, args):
        unknown = '*.ko'
        modName = unknown
        modules = MyDict()
        inFiles = MyDict(init=int)
        
        for arg in args:
            if arg.endswith('.ko'):
                modName = arg[:-3]
                modules[modName] += []
            elif arg.endswith('.conf'):
                modules[modName].append(arg)
            elif arg.endswith('.in'):
                inFiles[arg] += 1
            else:
                self.error('Invalid args(%s) not *.conf, *.ko, *.in' % arg)
                self.usage()
                sys.exit(1)
        
        if unknown in modules:
            self.error('There are some *.conf not given modules: %s' %
                ' '.join(modules[unknown]))
            self.usage()
            sys.exit(1)
        
        if modules and inFiles:
            self.error("It's not allowed both converting *.in and " +
                "handling *.ko")
            self.usage()
            sys.exit(1)
        
        self.inFiles = inFiles
        self.modules = modules
        
        return self
    
    def execute(self):
        if self.inFiles:
            self.genHeader(self.inFiles)
        
        if self.modules:
            self.handleModules()
    
    def checkModInserted(self, mod, shell=Shell()):
        cmd = "lsmod | awk '{print $1}' | grep -w %s >/dev/null" % mod
        rc = shell.run(cmd, throw=False, echo=False)
        if rc:
            self.error('module(%s.ko) is not inserted.' % mod)
            sys.exit(1)
    
    def checkFileExist(self, file):
        if not os.path.isfile(file):
            self.error('%s does not exist or is not a file.' % file)
            sys.exit(1)
    
    def handleModules(self):
        for mod in self.modules:
            self.checkModInserted(mod)
            for file in self.modules[mod]:
                self.checkFileExist(file)
        
        maker = ModMaker()
        for coder in self.coders:
            maker.addCodes(coder().genCodes(self.modules))
        maker.run()
    
    def parseModulesFromInFile(self, inFile):
        fd = open(inFile)
        lines = fd.read().split('\n')
        fd.close()
        
        insmodList = []
        for line in lines:
            line = line.strip()
            grp = re.search('#\s*MOD=(\w+)\s*', line)
            if grp:
                insmodList.append('mod -s ' + grp.groups()[0])
        return insmodList
    
    def genHeader(self, inFiles=[], shell=Shell()):
        self.inFiles,self.outFiles = [],[]
        for f in inFiles:
            if f in self.inFiles:
                continue
            elif not f.startswith('/'):
                raise Exception('%s is not an absolute-path' % f)
            elif not f.endswith('.in'):
                raise Exception("%s does not end with '.in'." % f)
            elif not os.path.exists(f):
                raise Exception('%s does not exist' % f)
            else:
                self.inFiles.append(f)
                self.outFiles.append(f[:-3]+'.h')
        
        # Generate commands for crash
        lines = []
        lines.append('# Usage: crash -i <crash.cmd.txt>')
        lines.append('extend extpy.so')
        for f in self.inFiles:
            lines += self.parseModulesFromInFile(f)
            lines.append('extpy ctyp.py ' + f)
        lines.append('q')
        
        rndstr = lambda n : [str(random.randint(100,999)) for i in range(n)]
        cmdFile = '/tmp/ksym.crash.' + '_'.join(rndstr(3)) + '.txt'
        shell.addExitCmds('rm -f ' + cmdFile)
        File.write(cmdFile, lines)
        
        shell.run('crash -i ' + cmdFile)
        print('')
        shell.doExitCmds()
        
        for f in self.outFiles:
            if not os.path.isfile(f):
                raise Exception('Failed to generate ' + f)
            
            # TODO: how to fix the bug of crash-ext-tools
            fd = open(f)
            lines = fd.read().replace('...','char dummy[1];').split('\n')
            fd.close()
            
            File.write(f, lines)
            
        print('Generate %d headers success.' % len(self.outFiles))

if __name__ == '__main__':
    Main().run()
