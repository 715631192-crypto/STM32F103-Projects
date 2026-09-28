#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""把本工程的文件与参考工程逐行对比（忽略 UTF-8 BOM 与 CRLF/LF 差异）。

用途：排查"参考工程能联网、本工程不能"时，先确认**同名文件到底有没有被改过**。
只看字节数会误判 —— 本工程给所有含中文的 .c/.h 补过 BOM（+3 字节），
行尾也可能不同，所以必须规范化后再比。

用法：
    python tools/diff_with_reference.py            # 对比默认清单
    python tools/diff_with_reference.py --list     # 只打印清单与结论
"""
from __future__ import annotations

import difflib
import os
import sys

REF_ROOT = r"C:\Users\71563\Desktop\智能门锁\STM32_SmartLock"
OUR_ROOT = r"C:\Users\71563\Desktop\block\firmware"

# 与 ESP8266 联网链路直接相关的文件；顺序 = 排查优先级
PAIRS = [
    ("HARDWARE/esp8266.c", "ESP8266 驱动主体（AT 指令收发）"),
    ("HARDWARE/esp8266.h", "ESP8266 配置：WiFi 名/密码、OneNET 三元组"),
    ("USER/app_wifi.c", "联网任务：连 WiFi、连 MQTT、上报"),
    ("USER/app_wifi.h", "联网任务对外接口"),
    ("SYSTEM/usart.c", "串口初始化（USART1/2/3 与中断）"),
    ("SYSTEM/usart.h", "串口对外接口"),
    ("SYSTEM/sys.c", "引脚重映射 / JTAG 释放"),
    ("SYSTEM/delay.c", "延时与毫秒计数"),
    ("USER/main.c", "初始化顺序（谁先谁后很关键）"),
]


def read_norm(path: str) -> list[str] | None:
    """读文件 → 去 BOM → 统一换行 → 拆行。读不到返回 None。"""
    if not os.path.exists(path):
        return None
    with open(path, "rb") as fh:
        raw = fh.read()
    if raw.startswith(b"\xef\xbb\xbf"):
        raw = raw[3:]
    text = raw.decode("utf-8", errors="replace")
    text = text.replace("\r\n", "\n").replace("\r", "\n")
    return text.split("\n")


def main() -> int:
    only_list = "--list" in sys.argv
    changed_total = 0

    for rel, desc in PAIRS:
        ref_path = os.path.join(REF_ROOT, rel.replace("/", os.sep))
        our_path = os.path.join(OUR_ROOT, rel.replace("/", os.sep))

        ref_lines = read_norm(ref_path)
        our_lines = read_norm(our_path)

        print("=" * 78)
        print(f"{rel}   —— {desc}")

        if ref_lines is None:
            print("  [跳过] 参考工程里没有这个文件")
            continue
        if our_lines is None:
            print("  [跳过] 本工程里没有这个文件")
            continue

        diff = list(
            difflib.unified_diff(
                ref_lines, our_lines, fromfile="参考工程", tofile="本工程", lineterm="", n=2
            )
        )
        if not diff:
            print("  >>> 完全一致（忽略 BOM/换行），代码没被改过")
            continue

        changed_total += 1
        add = sum(1 for d in diff if d.startswith("+") and not d.startswith("+++"))
        rem = sum(1 for d in diff if d.startswith("-") and not d.startswith("---"))
        print(f"  >>> 有差异：新增/改了 {add} 行，去掉 {rem} 行")
        if not only_list:
            for d in diff:
                print("   " + d)

    print("=" * 78)
    print(f"结论：{changed_total} 个文件与参考工程不同。")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
