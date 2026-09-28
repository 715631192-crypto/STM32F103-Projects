#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
从 Windows 自带 simhei.ttf 生成 SSD1306 用的 16x16 汉字点阵字库
（firmware/HARDWARE/oledfont_cn.h）。

用法
----
  python tools/gen_oled_font.py --check     # 只检查源码用到的汉字是否都在字库里
  python tools/gen_oled_font.py             # 缺字就用 simhei.ttf 补齐并改写字库
  python tools/gen_oled_font.py --trim      # 裁到"源码实际用到的汉字"，并补缺字

约定（与 oled.c 的 OLED_ShowText 严格一致）
------------------------------------------
  每个汉字 32 字节：
    [0..15]  上半页 —— 第 i 字节 = 第 i 列在行 0~7 的像素，bit0 = 行 0
    [16..31] 下半页 —— 第 i 字节 = 第 i 列在行 8~15 的像素，bit0 = 行 8

  生成参数 (size=16, dx=0, dy=-1, thr=120) 是用仓库里已有 125 个字形做基准
  反推出来的：平均汉明距离 2~3 个像素/字，与手工取模软件的效果肉眼无差。

注意：原本的字库是另一个取模工具生成的，本脚本无法做到**逐位**复刻，
      因此只用于「补齐缺字」，不要用它整体重刷已有字形（会造成肉眼难辨但
      无谓的差异）。

--trim 的安全性
---------------
  裁剪**只做过滤**：留下的汉字原封不动沿用文件里已有的点阵字节，
  绝不用 simhei.ttf 重新渲染。因此它同样不会引入"肉眼难辨的差异"，
  掉的只是没人会显示的多余字形（每字 32 字节）。
  唯一的例外是"既缺字又要裁"时：缺的那几个仍由 TTF 补齐后再一起留下。

  ⚠ 已知坑：**被裁掉的字如果日后又被 UI 用到**，补齐模式会用 TTF 重新渲染它，
  于是这个字的位图就与最初出模的字库不一致了（还是"肉眼难辨"级别，但没必要）。
  正确做法是事后用版本库/备份里的旧 `oledfont_cn.h` 把那个字的 32 字节换回来，
  别让它停在 TTF 版本上。
"""

import argparse
import os
import re
import sys

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:  # pragma: no cover
    sys.exit("需要 Pillow：pip install pillow")

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HEADER = os.path.join(REPO, "firmware", "HARDWARE", "oledfont_cn.h")
# 字库要覆盖的所有"会显示到 OLED 的中文"都写在 board_port.c 里
UI_SOURCES = [os.path.join(REPO, "firmware", "stm32", "src", "board_port.c")]

TTF_CANDIDATES = [
    r"C:\Windows\Fonts\simhei.ttf",
    "/usr/share/fonts/truetype/simhei.ttf",
]

GEN_SIZE = 16
GEN_DX = 0
GEN_DY = -1
GEN_THR = 120

CODES_PER_LINE = 12


# ----------------------------------------------------------------------
# 解析 / 渲染
# ----------------------------------------------------------------------

def parse_header(text):
    m = re.search(r"CN_CODE\[CN_FONT_COUNT\]\s*=\s*\{(.*?)\};", text, re.S)
    codes = [int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{4})", m.group(1))]
    m = re.search(r"CN_FONT\[CN_FONT_COUNT\]\[32\]\s*=\s*\{(.*?)\n\};", text, re.S)
    rows = re.findall(r"\{([^{}]*)\}", m.group(1))
    if len(codes) != len(rows):
        sys.exit("字库自检失败：CN_CODE 有 %d 项，CN_FONT 有 %d 项" % (len(codes), len(rows)))
    glyph = {}
    for cp, row in zip(codes, rows):
        vals = [int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{2})", row)]
        if len(vals) != 32:
            sys.exit("字库自检失败：0x%04X 只有 %d 字节" % (cp, len(vals)))
        glyph[cp] = vals
    return codes, glyph


def find_ttf():
    for p in TTF_CANDIDATES:
        if os.path.exists(p):
            return p
    sys.exit("找不到 simhei.ttf，请把路径加进 TTF_CANDIDATES")


def render_glyph(ch, ttf):
    img = Image.new("L", (48, 48), 0)
    ImageDraw.Draw(img).text(
        (GEN_DX, GEN_DY), ch,
        font=ImageFont.truetype(ttf, GEN_SIZE), fill=255)
    px = img.load()
    out = []
    for col in range(16):
        b = 0
        for r in range(8):
            if px[col, r] > GEN_THR:
                b |= 1 << r
        out.append(b)
    for col in range(16):
        b = 0
        for r in range(8):
            if px[col, r + 8] > GEN_THR:
                b |= 1 << r
        out.append(b)
    return out


def is_cjk(cp):
    return 0x4E00 <= cp <= 0x9FFF


def ui_charset():
    """扫描 UI 源码里的字符串字面量，取出所有汉字码点。"""
    found = set()
    for path in UI_SOURCES:
        src = open(path, encoding="utf-8").read()
        src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)     # 去掉块注释
        src = re.sub(r"//[^\n]*", "", src)                  # 去掉行注释
        for lit in re.findall(r'"((?:[^"\\\n]|\\.)*)"', src):
            for ch in lit:
                if is_cjk(ord(ch)):
                    found.add(ord(ch))
    return found


# ----------------------------------------------------------------------
# 输出
# ----------------------------------------------------------------------

def format_codes(codes):
    lines = []
    for i in range(0, len(codes), CODES_PER_LINE):
        chunk = codes[i:i + CODES_PER_LINE]
        lines.append("    " + " ".join("0x%04X," % c for c in chunk))
    return "\n".join(lines)


def format_font(codes, glyph):
    lines = []
    for cp in codes:
        body = ",".join("0x%02X" % v for v in glyph[cp])
        lines.append("    {%s},   /* %s */" % (body, chr(cp)))
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true",
                    help="只检查缺字，不修改文件（缺字时返回 1）")
    ap.add_argument("--trim", action="store_true",
                    help="把字库裁到 UI 源码实际用到的汉字（保留原有字形点阵）")
    args = ap.parse_args()

    text = open(HEADER, encoding="utf-8").read()
    codes, glyph = parse_header(text)
    old_count = len(codes)

    want = ui_charset()
    missing = sorted(cp for cp in want if cp not in glyph)
    extra = sorted(cp for cp in codes if cp not in want)
    print("字库现有 %d 字；UI 源码共用到 %d 个汉字；缺 %d 个。"
          % (len(codes), len(want), len(missing)))
    if args.trim:
        print("其中 %d 个字界面上用不到，可裁掉（省 %d 字节 Flash）。"
              % (len(extra), len(extra) * 32))

    if (not missing) and (not (args.trim and extra)):
        print("OK：字库已覆盖全部界面用字，且没有多余项。")
        return 0

    if args.check:
        if missing:
            print("缺字清单：" + "".join(chr(c) for c in missing))
        if args.trim and extra:
            print("可裁剪清单：" + "".join(chr(c) for c in extra))
        return 1

    if missing:
        ttf = find_ttf()
        for cp in missing:
            glyph[cp] = render_glyph(chr(cp), ttf)

    # 裁剪时只按 want 过滤，字形字节沿用 glyph（原有 + 新补），不重新渲染。
    codes = sorted(want) if args.trim else sorted(glyph)

    text = re.sub(r"#define CN_FONT_COUNT\s+\d+",
                  "#define CN_FONT_COUNT   %d" % len(codes), text)
    text = re.sub(r"(CN_CODE\[CN_FONT_COUNT\]\s*=\s*\{).*?(\n\};)",
                  lambda m: m.group(1) + "\n" + format_codes(codes) + m.group(2),
                  text, flags=re.S)
    text = re.sub(r"(CN_FONT\[CN_FONT_COUNT\]\[32\]\s*=\s*\{).*?(\n\};)",
                  lambda m: m.group(1) + "\n" + format_font(codes, glyph) + m.group(2),
                  text, flags=re.S)
    text = re.sub(r"^ \* SSD1306 点阵字库：汉字 16x16（\d+ 字）",
                  " * SSD1306 点阵字库：汉字 16x16（%d 字）" % len(codes),
                  text, flags=re.M)

    with open(HEADER, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)

    cost = (len(codes) - old_count) * 32
    if missing:
        print("已补齐 " + "".join(chr(c) for c in missing))
    if args.trim:
        print("已裁掉 " + "".join(chr(c) for c in extra))
    print("字库现在共 %d 字（%+d 字节 Flash）。" % (len(codes), cost))
    return 0


if __name__ == "__main__":
    sys.exit(main())
