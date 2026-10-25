#!/usr/bin/env python3
#
# Report how much cyclomatic complexity a patch adds.
#
# Usage: scripts/complexity-delta.py [--max-ccn N] [--max-delta N] BASE [HEAD]
#
# Compares every C/C++ function touched between BASE and HEAD (default:
# the working tree) using lizard (pip install lizard) and prints the
# functions whose cyclomatic complexity (CCN) changed, plus the total.
# With --max-delta the script fails when the total grows by more than N;
# with --max-ccn it fails when a function that the patch made more
# complex ends up above N.
#

import argparse
import subprocess
import sys

import lizard

EXTS = ('.c', '.h', '.cc', '.cpp', '.cxx', '.hpp')


def git(*args):
    return subprocess.run(('git',) + args, check=True,
                          stdout=subprocess.PIPE).stdout.decode('utf-8',
                                                                'replace')


def changed_files(base, head):
    args = ['diff', '--name-status', '-M', base]
    if head:
        args.append(head)
    ret = []
    for line in git(*args).splitlines():
        f = line.split('\t')
        st = f[0][0]
        old = f[1]
        new = f[2] if st in 'RC' else f[1]
        if not new.endswith(EXTS) and not old.endswith(EXTS):
            continue
        ret.append((None if st == 'A' else old, None if st == 'D' else new))
    return ret


def read_rev(rev, path):
    if path is None:
        return None
    if rev is None:
        with open(path, encoding='utf-8', errors='replace') as f:
            return f.read()
    return git('show', rev + ':' + path)


def functions(path, code):
    if code is None:
        return {}
    res = lizard.analyze_file.analyze_source_code(path, code)
    ret = {}
    for fn in res.function_list:
        # static functions with the same name may repeat, keep the worst
        if fn.long_name not in ret or \
                fn.cyclomatic_complexity > ret[fn.long_name][0]:
            ret[fn.long_name] = (fn.cyclomatic_complexity, fn.name)
    return ret


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--max-ccn', type=int)
    ap.add_argument('--max-delta', type=int)
    ap.add_argument('base')
    ap.add_argument('head', nargs='?')
    a = ap.parse_args()

    total = 0
    over = []
    for old, new in changed_files(a.base, a.head):
        fo = functions(old, read_rev(a.base, old))
        fn = functions(new, read_rev(a.head, new))
        rows = []
        for k in sorted(set(fo) | set(fn)):
            co = fo[k][0] if k in fo else 0
            cn = fn[k][0] if k in fn else 0
            if co == cn:
                continue
            name = (fn.get(k) or fo.get(k))[1]
            rows.append((cn - co, name, co if k in fo else '-',
                         cn if k in fn else '-'))
            if a.max_ccn is not None and cn > co and cn > a.max_ccn:
                over.append((new, name, cn))
        if not rows:
            continue
        print(new or old)
        for d, name, co, cn in rows:
            print('  %+4d  %-40s %3s -> %s' % (d, name, co, cn))
        total += sum(r[0] for r in rows)

    print('total CCN delta: %+d' % total)
    ret = 0
    for path, name, cn in over:
        print('%s: %s: CCN %d > %d' % (path, name, cn, a.max_ccn))
        ret = 1
    if a.max_delta is not None and total > a.max_delta:
        print('CCN delta %d > %d' % (total, a.max_delta))
        ret = 1
    return ret


if __name__ == '__main__':
    sys.exit(main())
