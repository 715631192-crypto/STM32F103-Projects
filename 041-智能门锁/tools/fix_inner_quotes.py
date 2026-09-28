#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""把 .py 里"内嵌在正文中的 ASCII 直引号"改成中文弯引号 “ ”。

判据（比"看相邻字符"更稳）：
  1. 先排除三连双引号（Python 文档字符串定界符）—— 整段跳过；
  2. 一个 " 若是**合法字符串定界符**则跳过。定界符的判据是：
       - 前一个字符属于 ( [ { = , 或空白 → 是"开定界符"；
       - 后一个字符属于 , ) ] } 或空白 → 是"闭定界符"；
  3. 剩下的 " 全部是内嵌引号，需要替换。开合方向按三种线索判定：
       - 前一个字符也是 "  → 一定是**开**引号（""中文… 的情形）；
       - 后一个字符也是 "  → 一定是**闭**引号（…中文"" 的情形）；
       - 否则按行内已出现的 “ / ” 配对状态决定（已有 “ 未闭合 → 本次是闭引号）。
"""
import io
import sys

DELIM_PREV = set('([{=,')
DELIM_NEXT = set(',)]}')


def _is_part_of_triple(s, i):
    """该双引号是否属于三连双引号文档定界符的一部分（是则跳过）。"""
    run = 1
    j = i - 1
    while j >= 0 and s[j] == '"':
        run += 1; j -= 1
    j = i + 1
    while j < len(s) and s[j] == '"':
        run += 1; j += 1
    return run >= 3


def _is_delimiter(s, i):
    prev = s[i - 1] if i > 0 else '\n'
    nxt = s[i + 1] if (i + 1) < len(s) else '\n'
    if prev in DELIM_PREV or prev in ' \t\n':
        return True
    if nxt in DELIM_NEXT or nxt in ' \t\n':
        return True
    return False


def fix_line(line):
    out = list(line)
    expect_close = False          # 行内是否已有未闭合的 “
    n = len(line)
    for i, ch in enumerate(line):
        if ch == '\u201c':        # 已是弯开引号
            expect_close = True
        elif ch == '\u201d':      # 已是弯闭引号
            expect_close = False
        elif ch == '"':
            if _is_part_of_triple(line, i) or _is_delimiter(line, i):
                continue
            prev = line[i - 1] if i > 0 else ''
            nxt = line[i + 1] if (i + 1) < n else ''
            if prev == '"':
                out[i] = '\u201c'; expect_close = True
            elif nxt == '"':
                out[i] = '\u201d'; expect_close = False
            elif expect_close:
                out[i] = '\u201d'; expect_close = False
            else:
                out[i] = '\u201c'; expect_close = True
    return ''.join(out)


def main(path):
    with io.open(path, 'r', encoding='utf-8', newline='') as f:
        lines = f.readlines()
    changed = 0
    new = []
    for i, ln in enumerate(lines, 1):
        fixed = fix_line(ln)
        if fixed != ln:
            changed += 1
            print('  L%-4d %s' % (i, fixed.rstrip('\r\n')[:100]))
        new.append(fixed)
    with io.open(path, 'w', encoding='utf-8', newline='') as f:
        f.writelines(new)
    print('fixed lines: %d -> %s' % (changed, path))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1]))
