#!/usr/bin/env python3
import os, re, sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
API_HEADER = os.path.join(SCRIPT_DIR, '..', 'il2cpp', 'libil2cpp', 'il2cpp-api-functions.h')
OUT_CPP = os.path.join(SCRIPT_DIR, '..', 'projects', 'unityloader', 'monocompat', 'il2cpp_log_shim.cpp')
OUT_H = os.path.join(SCRIPT_DIR, '..', 'projects', 'unityloader', 'monocompat', 'il2cpp_log_shim.h')

pattern = re.compile(
    r'DO_API(?:_NO_RETURN)?\s*\(\s*'
    r'([^,]+?)\s*,'
    r'\s*(\w+)\s*,'
    r'\s*\(([^)]*)\)\s*'
    r'\)'
)

def split_param(p):
    """Split a C parameter like 'const char * name' into (type, name). Handles arrays."""
    p = p.strip()
    # Handle array params like 'const char* const argv[]' - convert [] to *
    arr_suffix = ''
    if p.endswith(']'):
        bracket = p.rfind('[')
        if bracket > 0:
            arr_suffix = '*'  # Convert [] to pointer
            p = p[:bracket].strip()
    
    tokens = p.split()
    if not tokens:
        return (p, '')
    
    # Check if last token is a name (identifier, optionally prefixed with *)
    last = tokens[-1]
    # Strip leading * to check if it's a name
    last_stripped = last.lstrip('*')
    if len(tokens) >= 2 and re.match(r'^[a-zA-Z_]\w*$', last_stripped):
        # Has both type and name
        if last.startswith('*'):
            name = last_stripped
            stars = '*' * (len(last) - len(name))
            ptype = ' '.join(tokens[:-1]) + ' ' + stars
        else:
            name = last
            ptype = ' '.join(tokens[:-1])
        return (ptype.strip() + arr_suffix, name.strip())
    else:
        # Only type, no name - use placeholder
        return (p.strip() + arr_suffix, '')


funcs = []
with open(API_HEADER) as f:
    for line in f:
        line = line.strip()
        if not line.startswith('DO_API'):
            continue
        m = pattern.search(line)
        if not m:
            continue
        ret = m.group(1).strip()
        name = m.group(2).strip()
        ps = m.group(3).strip()
        params = []
        if ps and ps != 'void':
            for p in ps.split(','):
                p = p.strip()
                if not p:
                    continue
                params.append(split_param(p))
        funcs.append((ret, name, params))

print(f"Parsed {len(funcs)} functions")

# Verify some params
for f in funcs:
    if f[1] == 'il2cpp_set_config_dir':
        print(f"  il2cpp_set_config_dir: {f}")
    if f[1] == 'il2cpp_init':
        print(f"  il2cpp_init: {f}")
    if f[1] == 'il2cpp_class_from_name':
        print(f"  il2cpp_class_from_name: {f}")

# Generate header
with open(OUT_H, 'w') as f:
    f.write('#pragma once\n#include "so_util.h"\n\nvoid il2cpp_log_shim_init(so_module* mod, DynLibFunction* fallback_table);\nextern DynLibFunction symtable_il2cpp_log[];\nextern DynLibFunction* so_dynamic_libraries[];\n')

# Generate cpp
o = []
o.append('// THIS FILE WAS AUTO-GENERATED VIA tools/gen_il2cpp_log_shim.py')
o.append('#ifdef IL2CPP_TRACE')
o.append('#include "il2cpp_log_shim.h"')
o.append('#include "logging.h"')
o.append('#include "platform.h"')
o.append('#include "so_util.h"')
o.append('#include <cstring>')
o.append('')

# Include il2cpp type definitions
o.append('#include "il2cpp-api-types.h"')
o.append('')

# Function pointers
for ret, name, params in funcs:
    sig = ', '.join(f'{t} {n}' if n else t for t, n in params) if params else 'void'
    o.append(f'static {ret} (*real_{name})({sig}) = NULL;')
o.append('')

# Helper to get base type for logging decisions
def base_type(t):
    return t.replace('const ', '').strip()

def is_string_ptr(t):
    bt = base_type(t)
    return bt in ('char *', 'char*', 'const char *', 'const char*')

def log_param(idx, t, n):
    bt = base_type(t)
    if not n:
        return []
    if is_string_ptr(t):
        return [f'    verbose("IL2CPP", "  arg{idx} ({n}) = %s", {n} ? {n} : "(null)");']
    elif '*' in bt:
        return [f'    verbose("IL2CPP", "  arg{idx} ({n}) = %p", (void*){n});']
    elif bt == 'bool':
        return [f'    verbose("IL2CPP", "  arg{idx} ({n}) = %s", {n} ? "true" : "false");']
    elif bt in ('int', 'int32_t', 'int64_t', 'int16_t', 'int8_t', 'long'):
        return [f'    verbose("IL2CPP", "  arg{idx} ({n}) = %d", (int){n});']
    elif bt in ('uint32_t', 'size_t', 'uintptr_t', 'uint64_t', 'uint16_t', 'uint8_t', 'unsigned int'):
        return [f'    verbose("IL2CPP", "  arg{idx} ({n}) = %u", (unsigned){n});']
    else:
        return [f'    verbose("IL2CPP", "  arg{idx} ({n}) = 0x%lx", (unsigned long){n});']

# Wrapper functions
for ret, name, params in funcs:
    if not params or (len(params) == 1 and params[0][0] == 'void' and not params[0][1]):
        sig = 'void'
        args = ''
    else:
        sig = ', '.join(f'{t} {n}' if n else f'{t} _p{i}' for i, (t, n) in enumerate(params))
        args = ', '.join(n if n else f'_p{i}' for i, (t, n) in enumerate(params) if n or t != 'void')

    o.append(f'static {ret} shim_{name}({sig})')
    o.append('{')
    o.append(f'    verbose("IL2CPP", ">>> {name}");')

    for i, (t, n) in enumerate(params):
        if t == 'void' and not n:
            continue
        pn = n if n else f'_p{i}'
        o.extend(log_param(i, t, pn))

    if ret == 'void':
        if args:
            o.append(f'    real_{name}({args});')
        else:
            o.append(f'    real_{name}();')
    else:
        bt = base_type(ret)
        if is_string_ptr(ret):
            o.append(f'    {ret} _ret = real_{name}({args});')
            o.append('    verbose("IL2CPP", "  -> %s", _ret ? _ret : "(null)");')
        elif '*' in bt:
            o.append(f'    {ret} _ret = real_{name}({args});')
            o.append('    verbose("IL2CPP", "  -> %p", (void*)_ret);')
        elif bt == 'bool':
            o.append(f'    bool _ret = real_{name}({args});')
            o.append('    verbose("IL2CPP", "  -> %s", _ret ? "true" : "false");')
        elif bt in ('int', 'int32_t', 'int64_t', 'int16_t', 'int8_t', 'long'):
            o.append(f'    {ret} _ret = real_{name}({args});')
            o.append('    verbose("IL2CPP", "  -> %d", (int)_ret);')
        elif bt in ('uint32_t', 'size_t', 'uintptr_t', 'uint64_t', 'uint16_t', 'uint8_t', 'unsigned int'):
            o.append(f'    {ret} _ret = real_{name}({args});')
            o.append('    verbose("IL2CPP", "  -> %u", (unsigned)_ret);')
        else:
            o.append(f'    {ret} _ret = real_{name}({args});')
            o.append('    verbose("IL2CPP", "  -> 0x%lx", (unsigned long)_ret);')
        o.append('    return _ret;')

    o.append('}')
    o.append('')

# Symbol table - just pass pointers, we don't need the actual types here
o.append('DynLibFunction symtable_il2cpp_log[] = {')
for ret, name, params in funcs:
    o.append(f'    {{ "{name}", (uintptr_t)&shim_{name} }},')
o.append('    { NULL, (uintptr_t)NULL }')
o.append('};')
o.append('')

# Init function - resolves real function pointers from loaded module,
# falls back to the provided function table (e.g. symtable_monobridge)
o.append('void il2cpp_log_shim_init(so_module* mod, DynLibFunction* fallback_table)')
o.append('{')
o.append('    verbose("IL2CPP", "Initializing il2cpp log shim");')
o.append('')

# Helper: generate proper function pointer type for casting
def fptype(ret, params):
    if not params or (len(params) == 1 and params[0][0] == 'void' and not params[0][1]):
        return f'{ret} (*)()'
    ptypes = ', '.join(t for t, n in params)
    return f'{ret} (*)({ptypes})'

o.append('    // Resolve from loaded module (native il2cpp)')
o.append('    if (mod) {')
for ret, name, params in funcs:
    o.append(f'        real_{name} = ({fptype(ret, params)})so_symbol(mod, "{name}");')
o.append('    }')
o.append('')
o.append('    // Fallback: resolve from function table (monobridge for mono games)')
o.append('    if (fallback_table) {')
for ret, name, params in funcs:
    cast = fptype(ret, params)
    o.append(f'        if (!real_{name}) {{')
    o.append(f'            for (int j = 0; fallback_table[j].symbol; j++) {{')
    o.append(f'                if (strcmp(fallback_table[j].symbol, "{name}") == 0) {{')
    o.append(f'                    real_{name} = ({cast})fallback_table[j].func;')
    o.append(f'                    break;')
    o.append(f'                }}')
    o.append(f'            }}')
    o.append(f'        }}')
o.append('    }')
o.append('')
o.append('    int missing = 0;')
for ret, name, params in funcs:
    o.append(f'    if (!real_{name}) {{ verbose("IL2CPP", "MISSING: {name}"); missing++; }}')
o.append(f'    verbose("IL2CPP", "Shim ready: {len(funcs)} funcs, %d missing", missing);')
o.append('')
o.append('    // Register shim table at slot 0 so game code goes through our wrappers')
o.append('    for (int i = 31; i > 0; i--)')
o.append('        so_dynamic_libraries[i] = so_dynamic_libraries[i-1];')
o.append('    so_dynamic_libraries[0] = symtable_il2cpp_log;')
o.append('}')
o.append('')
o.append('#endif')

with open(OUT_CPP, 'w') as f:
    f.write('\n'.join(o))
print(f"Written {OUT_CPP} and {OUT_H}")
