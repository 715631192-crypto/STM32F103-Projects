#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""导出自研代码中的英文注释原文（带文件/行号），供人工精读后翻译。

范围：app / stm32 / HARDWARE / drivers / USER / SYSTEM / tests
排除：FreeRTOS / MIDDLEWARE（第三方）
"""
import os
import re
import sys

ROOT = r'C:\Users\71563\Desktop\block\firmware'
TARGET_TOP = {'app', 'stm32', 'HARDWARE', 'drivers', 'USER', 'SYSTEM', 'tests'}
CJK = re.compile(r'[\u4e00-\u9fff]')
ASCII_LETTERS = re.compile(r'[A-Za-z]')
COMMENT_START = re.compile(r'^\s*(//|/\*|\*)')

def is_english_comment(line):
    if not COMMENT_START.match(line):
        return False
    if CJK.search(line):
        return False
    return len(ASCII_LETTERS.findall(line)) >= 2

def main():
    out = []
    total = 0
    files = 0
    for dirpath, dirnames, filenames in os.walk(ROOT):
        dirnames[:] = [d for d in dirnames if d not in ('build', 'obj', '.git', 'FreeRTOS', 'MIDDLEWARE')]
        for fn in sorted(filenames):
            if not fn.endswith(('.c', '.h')):
                continue
            full = os.path.join(dirpath, fn)
            rel = os.path.relpath(full, ROOT)
            if rel.split(os.sep)[0] not in TARGET_TOP:
                continue
            hits = []
            with open(full, 'r', encoding='utf-8', errors='replace') as f:
                for i, line in enumerate(f, 1):
                    if is_english_comment(line):
                        hits.append((i, line.rstrip('\n').rstrip('\r')))
            if not hits:
                continue
            files += 1
            total += len(hits)
            out.append('')
            out.append('#### %s  (%d 行)' % (rel, len(hits)))
            for i, text in hits:
                out.append('%5d| %s' % (i, text))
    print('\n'.join(out))
    print()
    print('=== 总计 %d 行 / %d 个文件 ===' % (total, files))
    return 0

if __name__ == '__main__':
    sys.exit(main())
