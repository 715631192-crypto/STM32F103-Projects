#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""全仓库扫描：本工程 firmware/ 与参考工程逐文件对比，列出所有被改动过的文件。

忽略 BOM 与 CRLF/LF 差异，并按"是否触碰 GPIO / 串口"标注，便于快速定位
"参考工程能跑、本工程不能"时的硬件相关改动。

用法：python tools/diff_tree.py [--verbose]
"""
from __future__ import annotations

import difflib
import os
import re
import sys

REF_ROOT = r"C:\Users\71563\Desktop\智能门锁\STM32_SmartLock"
OUR_ROOT = r"C:\Users\71563\Desktop\block\firmware"

# 只关心这些子目录（FreeRTOS / .cmsis 是原样搬运，不参与）
SCAN_DIRS = ["HARDWARE", "SYSTEM", "MIDDLEWARE", "USER"]
EXTS = {".c", ".h"}

# 与硬件相关的关键字：命中就把文件名标出来
HW_PAT = re.compile(
    r"GPIO_Pin_|GPIO_Init|RCC_APB|PinRemap|USART[123]|GPIOA|GPIOB|GPIOC|EXTI_|TIM[0-9]"
)


def read_norm(path):
    if not os.path.exists(path):
        return None
    with open(path, "rb") as fh:
        raw = fh.read()
    if raw.startswith(b"\xef\xbb\xbf"):
        raw = raw[3:]
    t = raw.decode("utf-8", errors="replace").replace("\r\n", "\n").replace("\r", "\n")
    return t.split("\n")


def hw_hits(lines):
    """统计改动行里出现的硬件关键字"""
    hits = set()
    for ln in lines:
        if ln.startswith(("+", "-")) and not ln.startswith(("+++", "---")):
            for m in HW_PAT.findall(ln):
                hits.add(m)
    return sorted(hits)


def main():
    verbose = "--verbose" in sys.argv
    same, changed, only_here, only_ref = [], [], [], []

    for d in SCAN_DIRS:
        ref_dir = os.path.join(REF_ROOT, d)
        our_dir = os.path.join(OUR_ROOT, d)
        names = set()
        for base in (ref_dir, our_dir):
            if os.path.isdir(base):
                for fn in os.listdir(base):
                    if os.path.splitext(fn)[1].lower() in EXTS:
                        names.add(fn)
        for fn in sorted(names):
            rp = os.path.join(ref_dir, fn)
            op = os.path.join(our_dir, fn)
            r = read_norm(rp)
            o = read_norm(op)
            rel = f"{d}/{fn}"
            if r is None:
                only_here.append(rel)
                continue
            if o is None:
                only_ref.append(rel)
                continue
            if r == o:
                same.append(rel)
                continue
            diff = list(difflib.unified_diff(r, o, lineterm="", n=1))
            hits = hw_hits(diff)
            add = sum(1 for x in diff if x.startswith("+") and not x.startswith("+++"))
            rem = sum(1 for x in diff if x.startswith("-") and not x.startswith("---"))
            changed.append((rel, add, rem, hits, diff))

    print(f"完全一致的源文件：{len(same)} 个")
    print(f"仅本工程有：{only_here if only_here else '无'}")
    print(f"仅参考工程有：{only_ref}")
    print()
    print("=" * 78)
    print(f"被改动过的文件：{len(changed)} 个")
    print("=" * 78)
    for rel, add, rem, hits, diff in changed:
        flag = "  <<< 含硬件相关改动，重点看" if hits else ""
        print(f"\n[{rel}]  +{add}/-{rem}{flag}")
        if hits:
            print(f"    命中关键字: {', '.join(hits)}")
        if verbose:
            for x in diff:
                print("   " + x)


if __name__ == "__main__":
    main()
