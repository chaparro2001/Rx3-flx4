#!/usr/bin/env python3
"""Where is each thread of the running player waiting? For hangs: the firmware stops answering keys, never opens audio...

For every thread of rbp-pi: the system call it sits in, and the code addresses found on its stack, named from the
symbol tables of fbshim.so and of the (unstripped) player binary. Stack words are scanned, not unwound, so a name can be
a stale value rather than a live caller; the first few, nearest the stack pointer, are the ones to trust.

usage: sudo python3 rx3-stacks.py [words-per-stack]     (default 2048 words = 8 KB from the stack pointer)"""
import os, struct, subprocess, sys
import rx3_env

SYSCALLS = {162: 'nanosleep', 265: 'clock_nanosleep', 240: 'futex', 3: 'read', 4: 'write', 168: 'poll', 142: 'select',
            336: 'ppoll', 252: 'epoll_wait', 346: 'epoll_pwait', 291: 'mq_timedreceive', 0: 'restart_syscall'}

def elf_symbols(path):
    """[(start, end, name)] of the FUNC symbols of a 32-bit little-endian ELF, plus its PT_LOAD (offset, vaddr, flags)."""
    d = open(path, 'rb').read()
    if d[:4] != b'\x7fELF' or d[4] != 1: return [], []
    phoff, shoff = struct.unpack_from('<II', d, 28); phentsize, phnum, shentsize, shnum = struct.unpack_from('<HHHH', d, 42)
    loads = []
    for i in range(phnum):
        p_type, p_offset, p_vaddr, _, _, _, p_flags = struct.unpack_from('<7I', d, phoff + i * phentsize)
        if p_type == 1: loads.append((p_offset, p_vaddr, p_flags))
    secs = [struct.unpack_from('<IIIIIIIIII', d, shoff + i * shentsize) for i in range(shnum)]
    syms = []
    for s in secs:
        if s[1] not in (2, 11): continue                     # SYMTAB, DYNSYM
        strtab = secs[s[6]]; off, size, entsize = s[4], s[5], s[9] or 16
        for k in range(size // entsize):
            nm, val, sz, info, _, _ = struct.unpack_from('<IIIBBH', d, off + k * entsize)
            if info & 15 != 2 or not val: continue            # STT_FUNC
            name = d[strtab[4] + nm:d.index(b'\0', strtab[4] + nm)].decode(errors='replace')
            syms.append((val & ~1, (val & ~1) + max(sz, 4), name))
    return sorted(set(syms)), loads

def demangle(names):
    try: out = subprocess.run(['c++filt'], input='\n'.join(names), capture_output=True, text=True, timeout=10).stdout.split('\n')
    except OSError: return names
    return out[:len(names)] if len(out) >= len(names) else names

def main():
    pid = subprocess.run(['pgrep', '-x', 'rbp-pi'], capture_output=True, text=True).stdout.split()
    if not pid: sys.exit('rbp-pi is not running')
    pid = pid[0]; nwords = int(sys.argv[1]) if len(sys.argv) > 1 else 2048
    mem = open('/proc/%s/mem' % pid, 'rb')
    images = {}                                               # path -> (symbols, [(lo, hi, bias)])
    for line in open('/proc/%s/maps' % pid):
        f = line.split()
        if len(f) < 6 or 'x' not in f[1] or not (f[5].endswith('fbshim.so') or f[5].endswith('rbp-pi')): continue
        lo, hi = (int(v, 16) for v in f[0].split('-')); off = int(f[2], 16)
        path = rx3_env.ROOT + f[5] if not f[5].startswith(rx3_env.ROOT) else f[5]
        if path not in images: images[path] = (*elf_symbols(path), [])
        syms, loads, ranges = images[path]
        # The executable segment: lld puts it a page-multiple higher in memory than in the file, after a read-only one.
        seg = next(((o, v) for o, v, fl in loads if fl & 1 and o & ~0xfff == off), (off, off))
        ranges.append((lo, hi, lo - (seg[1] & ~0xfff)))
    def name_of(a):
        for path, (syms, loads, ranges) in images.items():
            for lo, hi, bias in ranges:
                if lo <= a < hi:
                    v = a - bias; tag = os.path.basename(path)
                    for s, e, n in syms:
                        if s <= v < e: return '%s:%s+%#x' % (tag, n, v - s)
                    return '%s:%#x' % (tag, v)
        return None
    for path, (syms, loads, ranges) in images.items():
        print('%s: %d functions, mapped %s' % (path, len(syms), ' '.join('%#x-%#x' % (lo, hi) for lo, hi, _ in ranges)))
    rows = []
    for t in sorted(os.listdir('/proc/%s/task' % pid), key=int):
        try: f = open('/proc/%s/task/%s/syscall' % (pid, t)).read().split()
        except OSError: continue
        comm = open('/proc/%s/task/%s/comm' % (pid, t)).read().strip()
        if f[0] in ('running', '-1'): rows.append((t, comm, f[0], [])); continue
        sp, pc = int(f[-2], 16), int(f[-1], 16)
        try: mem.seek(sp); words = struct.unpack('<%dI' % nwords, mem.read(4 * nwords))
        except (OSError, struct.error):
            try: mem.seek(sp); data = mem.read(4096); words = struct.unpack('<%dI' % (len(data) // 4), data)
            except OSError: words = ()
        hits = [n for n in (name_of(w) for w in words) if n]
        rows.append((t, comm, SYSCALLS.get(int(f[0]), 'syscall %s' % f[0]), hits))
    allnames = sorted({h.split(':', 1)[1].rsplit('+', 1)[0] for _, _, _, hs in rows for h in hs})
    dm = dict(zip(allnames, demangle(allnames)))
    for t, comm, sc, hits in rows:
        shim = any(h.startswith('fbshim.so') for h in hits)
        print('\nthread %s (%s)%s: %s' % (t, comm, '  <-- in the shim' if shim else '', sc))
        seen = []
        for h in hits:
            tag, rest = h.split(':', 1); base = rest.rsplit('+', 1)[0]
            h = '%s:%s' % (tag, rest.replace(base, dm.get(base, base), 1))
            if h not in seen: seen.append(h)
        for h in seen[:10]: print('    ' + h)

if __name__ == '__main__':
    main()
