#!/usr/bin/python

import re
import os
import sys
import random

class Kallsyms(object):
    kallsyms = []
    
    @classmethod
    def loadSymTable(cls, update=False):
        if not cls.kallsyms or update:
            if os.environ['USER'] != 'root':
                raise Exception('Run script by root')
            fd = open('/proc/kallsyms')
            cls.kallsyms = fd.read().split('\n')
            fd.close()
        return cls.kallsyms
    
    def addExitCmds(self, cmd):
        self.exitCmds.append(cmd)
    
    def doExitCmds(self):
        for cmd in self.exitCmds:
            os.system(cmd)
        self.exitCmds = []
    
    def shell(self, cmd):
        rc = os.system(cmd)
        if rc:
            self.doExitCmds()
            raise Exception('Failed to run command {%s}' % cmd)
    
    def ksymImportName(self, mod='(\w+)', name='(\w+)', ndx='(\w+)'):
        return '__ksym_1537_%s_2489_mod_%s_ndx_%s_ffff_' % (name,mod,ndx)
    
    def validSymName(self, name):
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
    
    def get(self, modName):
        if modName not in self.modules:
            self.modules[modName] = {
                'name'       : modName,
                'list'       : [],
                'dict'       : {},
                'importInfo' : {},
                'listValid'  : False,
                'dictValid'  : False,
                'importLoad' : False,
            }
        return self.modules[modName]
    
    def defaultImportInfo(self):
        return {
            'mods' : set(),
            'ents' : set(),
            'flag' : None,
        }
    
    def __init__(self, update=False):
        self.kallsyms = type(self).loadSymTable(update)
        self.modules = {}
        self.config = {}
        self.exitCmds = []
    
    def load(self, modName):
        mod = self.get(modName)
        if not mod['listValid']:
            if modName == '':
                mod['list'] = [sym for sym in self.kallsyms if '[' not in sym]
            else:
                pat = '[%s]' % modName
                mod['list'] = [sym for sym in self.kallsyms if pat in sym]
            mod['listValid'] = True
        return self
    
    def sort(self, modName):
        mod = self.get(modName)
        if not mod['listValid']:
            self.load(modName)
        if not mod['dictValid']:
            for sym in mod['list']:
                arr = sym.split()
                
                shortName = self.validSymName(arr[2])
                if not shortName:
                    continue
                
                if shortName not in mod['dict']:
                    mod['dict'][shortName] = {}
                d = mod['dict'][shortName]
                
                if arr[2] not in d:
                    d[arr[2]] = []
                d[arr[2]].append(arr[0])
            mod['dictValid'] = True
        return self
    
    def lookupSymbolAddr(self, modName, sym, exact=False):
        shortName = self.validSymName(sym)
        mod = self.sort(modName).get(modName)
        d = mod['dict']
        
        if shortName not in d:
            raise Exception(
                'Symbol(%s) not found in module(%s)' % (sym, modName))
        d = d[shortName]
        
        if shortName == sym and not exact:
            if len(d) == 1:
                addr = d[d.keys()[0]]
            else:
                raise Exception(
                    'More than ONE address for symbol(%s) in module(%s)'
                    % (sym, modName))
        else:
            if sym not in d:
                raise Exception(
                    'Symbol(%s) not found in module(%s)' % (sym, modName))
            addr = d[sym]
        
        if len(addr) != 1:
            raise Exception('More than ONE address for symbol(%s) in module(%s)'
                % (sym, modName))
        return addr[0]
    
    def parseImportInfo(self, modName):
        mod = self.get(modName)
        if not mod['listValid']:
            self.load(modName)
        
        if not mod['importLoad']:
            info,pattern = None,self.ksymImportName()
            for sym in mod['list']:
                arr = sym.split()
                grp = re.search(pattern, arr[2])
                if grp:
                    if not info:
                        info = self.defaultImportInfo()
                        mod['importInfo'] = info
                    name,_mod,ndx = grp.groups()
                    self.addImportInfo(info,_mod,name,ndx)
                elif arr[2] == '__kdbg_ksym_imported':
                    if not info:
                        info = self.defaultImportInfo()
                        mod['importInfo'] = info
                    elif info['flag']:
                        raise Exception('repeat sym(__kdbg_ksym_imported) addr '
                            + '(%s) vs (%s)' % (info['flag'], arr[0]))
                    info['flag'] = arr[0]
            
            if info and (info['ents'] and not info['flag']):
                raise Exception(('import symbols(%s) but no ' +
                    'sym(__kdbg_ksym_imported)') % str(info['ents']))
            mod['importLoad'] = True
        
        return self
    
    def addImportInfo(self, info, _mod, name, ndx):
        if _mod == '_':
            _mod = ''
        if _mod not in info['mods']:
            self.sort(_mod)
            info['mods'].add(_mod)
        
        ent = '%s:%s:%s' % (_mod,name,ndx)
        if ent in info['ents']:
            raise Exception(('repeat importings _mod(%s), name(%s), ndx(%s) '
                + 'in mod(%s)') % (_mod, name, ndx, modName))
        
        info['ents'].add(ent)
    
    def isModImporting(self, modName):
        mod = self.get(modName)
        if not mod['importLoad']:
            self.parseImportInfo(modName)
        return ('importInfo' in mod
            and 'ents' in mod['importInfo']
            and len(mod['importInfo']['ents']) > 0)
    
    def loadConfigFile(self, configFile=''):
        if configFile:
            fd = open(configFile)
            lines = fd.read().split('\n')
            fd.close()
        else:
            lines = []
        
        lineNo = 0
        confPattern = '(\w*):(\w+)([:\d]*)\s*=\s*([\w:\.]+)'
        addrPattern = '^(0x|0X){0,1}[0-9a-fA-F]+$'
        
        for line in lines:
            lineNo += 1
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            
            grp = re.search(confPattern, line)
            if not grp:
                msg  = 'Line %d is invalid: %s\n' % (lineNo, line)
                msg += 'There are THREE valid config formats '
                msg += 'and examples below:\n'
                msg += '\tzfs:spa_open   = spa_open.part.5\n'
                msg += '\tzfs:spa_open:1 = ffffffffc07c9c60\n'
                msg += '\tzfs:spa_open   = exact:spa_open'
                raise Exception(msg)
            
            _mod,name,ndx,addr = grp.groups()
            if _mod == '_':
                _mod = ''
            if ndx == '':
                ndx = '0'
            elif ndx.count(':') != 1:
                raise Exception
            else:
                ndx = ndx.split(':')[1]
            
            if addr.startswith('exact:'):
                exact = True
                addr = addr.split(':')[1]
            else:
                exact = False
            addr = addr.strip()
            
            grp = re.search(addrPattern, addr)
            if grp:
                if addr.startswith('0x') or addr.startswith('0X'):
                    addr = addr[2:]
            else:
                addr = self.lookupSymbolAddr(_mod, addr, exact)
            
            key = '%s:%s:%s' % (_mod,name,ndx)
            if key in self.config and self.config[key] != addr:
                raise Exception('Line %d: Repeat config %s' % (lineNo, key))
            self.config[key] = addr
        
        return self
    
    def loadConfigFiles(self, configFiles=[]):
        self.config = {}
        for f in configFiles:
            self.loadConfigFile(f)
    
    def genImportCode(self, modName, configFiles=[]):
        if not self.isModImporting(modName):
            return {}
        
        self.loadConfigFiles(configFiles)
        
        info     = self.get(modName)['importInfo']
        funcName = 'import_symbols_for_' + modName
        codes    = []
        
        codes.append('static void')
        codes.append(funcName + '(void)')
        codes.append('{')
        codes.append('\tprintk("KDBG:KSYM: Import symbols for %s...\\n");'
            % modName)
        codes.append('')
        
        for ent in info['ents']:
            _mod,name,ndx = ent.split(':')
            _sym = self.ksymImportName(_mod,name,ndx)
            
            if ent in self.config:
                addr = self.config[ent]
            else:
                addr = self.lookupSymbolAddr(_mod,name)
            
            codes.append('\tprintk("KDBG:KSYM: %s = %s\\n");' % (ent,addr))
            codes.append('\t*(void**)0x%sUL = (void*)0x%sUL;' % (
                self.lookupSymbolAddr(modName,_sym), addr))
            codes.append('')
        
        codes.append('\tprintk("KDBG:KSYM: __kdbg_ksym_imported = 1\\n");')
        codes.append('\t*(int*)0x%sUL = 1;' % info['flag'])
        codes.append('}')
        
        return { 'func' : funcName, 'code' : codes }
    
    def usage(self):
        usage = [
            '<mod1.ko> [<mod1_1.conf> ...] [<mod2.ko> <mod2.conf> ...]',
            '<file1.in> [...]'
        ]
        contents  = 'Usage: %s ' % self.appname
        contents += ('\n       %s ' % self.appname).join(usage)
        print(contents)
    
    def writeFile(self, file, lines):
        fd = open(file, 'w')
        fd.write('\n'.join(lines))
        fd.close()
    
    def doImport(self, mods, codes):
        if len(mods) > 1:
            print('Info: There are %d modules need to be imported: %s' % (
                len(mods), ' '.join(mods)))
        
        rndstr = lambda n : [str(random.randint(100,999)) for i in range(n)]
        modname = 'ksym_' + '_'.join(rndstr(3))
        
        workspace = '/tmp/ksym.' + '.'.join(rndstr(3))
        self.shell('mkdir ' + workspace)
        self.addExitCmds('rm -rf ' + workspace)
        
        # Generate Makefile
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
        self.writeFile(workspace + '/Makefile', lines)
        
        # Generate C-codes
        lines = []
        lines.append('#include <linux/init.h>')
        lines.append('#include <linux/module.h>')
        lines.append('#include <linux/kernel.h>')
        lines.append('')
        
        funcs = []
        for mod in mods:
            code = codes[mod]
            if code:
                funcs.append(code['func'])
                lines += code['code']
                lines.append('')
        
        lines.append('static int __init')
        lines.append('ksym_drv_init(void)')
        lines.append('{')
        
        for func in funcs:
            lines.append('\t%s();' % func)
        
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
        self.writeFile(workspace + '/drv.c', lines)
        
        self.shell('make -C ' + workspace)
        self.shell('insmod %s/%s.ko' % (workspace, modname))
        self.addExitCmds('rmmod ' + modname)
        self.doExitCmds()
        
        print('Import symbols success.')
    
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
    
    def genHeader(self, inFiles=[]):
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
        self.addExitCmds('rm -f ' + cmdFile)
        self.writeFile(cmdFile, lines)
        
        self.shell('crash -i ' + cmdFile)
        self.doExitCmds()
        
        for f in self.outFiles:
            if not os.path.isfile(f):
                raise Exception('Failed to generate ' + f)
            
            # TODO: how to fix the bug of crash-ext-tools
            fd = open(f)
            lines = fd.read().replace('...','char dummy[1];').split('\n')
            fd.close()
            self.writeFile(f, lines)
            
        print('\nGenerate %d headers success.' % len(self.outFiles))
    
    def run(self):
        self.appname = os.path.basename(sys.argv[0])
        if len(sys.argv) > 1:
            allInFiles = True
            for f in sys.argv[1:]:
                if not f.endswith('.in'):
                    allInFiles = False
                    break
            if allInFiles:
                self.genHeader(sys.argv[1:])
                sys.exit(0)
        
        if len(sys.argv) == 1:
            self.usage()
            sys.exit(0)
        elif not sys.argv[1].endswith('.ko'):
            print("Error: *** Argument 1 '%s' is not <mod>.ko" % sys.argv[1])
            self.usage()
            sys.exit(1)
        
        mod,confs,modList = '',{},[]
        for arg in sys.argv[1:]:
            if arg.endswith('.ko'):
                mod = arg.split('.ko')[0]
                if mod not in confs:
                    confs[mod] = set()
                    modList.append(mod)
            elif arg.endswith('.conf'):
                confs[mod].add(arg)
            else:
                print("Error: *** invalid arg '%s'" % arg)
                sys.exit(1)
        
        importMods,importCodes = [],{}
        for mod in modList:
            code = self.genImportCode(mod, confs[mod])
            if code:
                importMods.append(mod)
                importCodes[mod] = code
        
        if not importMods:
            print('Warning: *** No modules need to be imported.')
            sys.exit(0)
        else:
            self.doImport(importMods, importCodes)

if __name__ == '__main__':
    if os.environ['USER'] != 'root':
        cmd = 'sudo ' + ' '.join(sys.argv)
        print('$ ' + cmd)
        rc = os.system(cmd)
        if rc != 0 and (rc & 0xFF) == 0:
            rc = 1
        sys.exit(rc)
    else:
        Kallsyms().run()
