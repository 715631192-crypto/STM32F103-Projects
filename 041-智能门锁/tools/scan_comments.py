#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""扫描 firmware/ 下的英文注释分布，用于评估"注释中文化"工作量。

判据：以 // 或 * 或 /* 开头的行，若不含任何 CJK 字符但含 >=2 个 ASCII 字母，
视为"英文注释行"。按顶层目录汇总。
"""
import os
import re
import sys

ROOT = r'C:\Users\71563\Desktop\block\firmware'
CJK = re.compile(r'[\u4e00-\u9fff]')
ASCII_LETTERS = re.compile(r'[A-Za-z]')
COMMENT_START = re.compile(r'^\s*(//|/\*|\*)')

def count_english_comment_lines(path):
    """返回 (英文注释行数, 总行数)"""
    eng = 0
    total = 0
    try:
        with open(path, 'r', encoding='utf-8', errors='replace') as f:
            for line in f:
                total += 1
                if not COMMENT_START.match(line):
                    continue
                if CJK.search(line):
                    continue
                if len(ASCII_LETTERS.findall(line)) >= 2:
                    eng += 1
    except Exception as e:
        print('ERR %s: %s' % (path, e))
        return (0, 0)
    return (eng, total)

def main():
    stats = {}          # topdir -> [eng, total, files]
    file_hits = []      # (eng, relpath)
    for dirpath, dirnames, filenames in os.walk(ROOT):
        # 跳过 build 产物
        dirnames[:] = [d for d in dirnames if d not in ('build', 'obj', '.git')]
        for fn in filenames:
            if not fn.endswith(('.c', '.h')):
                continue
            full = os.path.join(dirpath, fn)
            rel = os.path.relpath(full, ROOT)
            eng, total = count_english_comment_lines(full)
            if eng == 0:
                continue
            top = rel.split(os.sep)[0] if os.sep in rel else '(root)'
            acc = stats.setdefault(top, [0, 0, 0])
            acc[0] += eng
            acc[1] += total
            acc[2] += 1
            file_hits.append((eng, rel))
    print('=' * 62)
    print('%-22s %10s %10s %8s' % ('目录', '英文注释行', '文件总行', '文件数'))
    print('=' * 62)
    for top in sorted(stats, key=lambda k: -stats[k][0]):
        eng, total, files = stats[top]
        print('%-22s %10d %10d %8d' % (top, eng, total, files))
    print('=' * 62)
    g_eng = sum(v[0] for v in stats.values())
    g_files = sum(v[2] for v in stats.values())
    print('合计：英文注释行 %d，涉及文件 %d 个' % (g_eng, g_files))
    print()
    print('--- 英文注释最多的 25 个文件 ---')
    for eng, rel in sorted(file_hits, reverse=True)[:25]:
        print('%6d  %s' % (eng, rel))
    return 0

if __name__ == '__main__':
    sys.exit(main())
