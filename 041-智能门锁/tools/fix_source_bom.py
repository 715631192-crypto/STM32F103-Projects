# -*- coding: utf-8 -*-
"""
fix_source_bom.py —— 源码 UTF-8 BOM 检查 / 修补工具

【为什么需要它】
ARM Compiler 5（Keil MDK 的 AC5，即 armcc V5.06）**只认 UTF-8 BOM**。
源文件若含中文但没有 BOM，armcc 会按 Windows 系统代码页（简体中文下是
CP936/GBK）去解析，把 3 字节的 UTF-8 汉字错位成"双字节字符"，于是字符串
字面量末尾的右引号被当作汉字的后半字节吃掉：

    board_port.c(756): error:  #8: missing closing quote
        case LOCK_STATE_LOCKED:   return "宸蹭笂閿?";

加 BOM 后同一文件 0 error / 0 warning。

参考工程在 Keil 里编辑过的文件天然带 BOM（uVision 保存 UTF-8 时写入），
所以它在 Keil 下能编过；从外部工具（本仓库的代码生成）写出的文件不带 BOM，
就会踩这个坑。

GCC（arm-none-eabi-gcc / MinGW gcc）会忽略 BOM，因此加 BOM 对 CMake 的
两条构建线没有影响，可以放心统一。

【用法】
    python tools/fix_source_bom.py            # 修补：为缺 BOM 的中文源文件补上
    python tools/fix_source_bom.py --check    # 只检查，不修改；有缺失则退出码 1

扫描范围：firmware/ 下全部 .c/.h。
纯 ASCII 文件不加 BOM（没必要）；.s 汇编文件不动（armasm 当前编译正常）。
"""
from __future__ import annotations

import os
import sys

BOM = b"\xef\xbb\xbf"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCAN_DIR = os.path.join(ROOT, "firmware")


def iter_sources():
    for dirpath, dirnames, filenames in os.walk(SCAN_DIR):
        dirnames[:] = [d for d in dirnames if d not in (".git",)]
        for name in sorted(filenames):
            if name.endswith((".c", ".h")):
                yield os.path.join(dirpath, name)


def main() -> int:
    check_only = "--check" in sys.argv
    fixed, missing, skipped_non_utf8, skipped_ascii = [], [], [], []

    for path in iter_sources():
        with open(path, "rb") as fh:
            raw = fh.read()
        rel = os.path.relpath(path, ROOT)

        if raw.startswith(BOM):
            continue
        if not any(byte >= 0x80 for byte in raw):
            skipped_ascii.append(rel)
            continue
        try:
            raw.decode("utf-8")
        except UnicodeDecodeError as exc:
            skipped_non_utf8.append("%s  (%s)" % (rel, exc))
            continue

        if check_only:
            missing.append(rel)
            continue

        with open(path, "wb") as fh:
            fh.write(BOM + raw)
        fixed.append(rel)

    if check_only:
        if missing:
            print("以下 %d 个含中文的源文件缺少 UTF-8 BOM（Keil AC5 会编译失败）：" % len(missing))
            for rel in missing:
                print("  " + rel)
            print("\n执行 `python tools/fix_source_bom.py` 修复。")
            return 1
        print("OK：全部含非 ASCII 的源文件都已带 UTF-8 BOM。")
        return 0

    print("已补 BOM：%d 个" % len(fixed))
    for rel in fixed:
        print("  + " + rel)
    print("纯 ASCII 跳过：%d 个" % len(skipped_ascii))
    if skipped_non_utf8:
        print("非 UTF-8（未处理，请人工确认）：")
        for item in skipped_non_utf8:
            print("  ! " + item)
    return 0


if __name__ == "__main__":
    sys.exit(main())
