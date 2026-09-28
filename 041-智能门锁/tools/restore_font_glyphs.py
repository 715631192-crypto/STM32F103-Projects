# -*- coding: utf-8 -*-
"""从 oledfont_cn.h.bak151 还原"原厂字形"。

背景：gen_oled_font.py 补缺字时用 simhei.ttf 重新渲染，位图与最初出模的
字库不一致（肉眼难辨但不是原件）。本脚本把"补进来的、但 .bak151 里本来
就有"的字，用 .bak151 的 32 字节点阵原样换回；只有 .bak151 里也没有的
（界面全新用字）才保留 TTF 渲染结果。

用法: python restore_font_glyphs.py
"""
import re
import sys

CUR = r'C:\Users\71563\Desktop\block\firmware\HARDWARE\oledfont_cn.h'
BAK = r'C:\Users\71563\Desktop\block\firmware\HARDWARE\oledfont_cn.h.bak151'


def parse(path):
    text = open(path, 'r', encoding='utf-8-sig').read()
    # 码点表
    m = re.search(r'CN_CODE\[CN_FONT_COUNT\]\s*=\s*\{(.*?)\};', text, re.S)
    codes = [int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]{4})', m.group(1))]
    # 点阵行：{...},   /* 汉字 */
    rows = re.findall(r'\{((?:0x[0-9A-Fa-f]{2},\s*)+0x[0-9A-Fa-f]{2})\},\s*/\*\s*(.)\s*\*/', text)
    glyphs = {}
    for body, ch in rows:
        glyphs[ch] = [int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]{2})', body)]
    return text, codes, glyphs


def main():
    _, _, bak = parse(BAK)
    cur_text, cur_codes, cur = parse(CUR)

    restored, kept = [], []
    for ch in cur:
        if ch in bak:
            if cur[ch] != bak[ch]:
                restored.append(ch)
            else:
                kept.append(ch)  # 本来就一致（裁剪保留下来的 73 字）
        else:
            kept.append(ch)      # bak151 里没有 → 保留 TTF 渲染

    # 逐行替换点阵
    lines = cur_text.split('\n')

    def repl(mo):
        ch = mo.group(2)
        if ch in restored:
            body = ','.join('0x%02X' % b for b in bak[ch])
            return '{%s},   /* %s */' % (body, ch)
        return mo.group(0)

    pat = re.compile(r'\{((?:0x[0-9A-Fa-f]{2},\s*)+0x[0-9A-Fa-f]{2})\},\s*/\*\s*(.)\s*\*/')
    out = '\n'.join(pat.sub(repl, ln) for ln in lines)
    open(CUR, 'w', encoding='utf-8-sig').write(out)

    print('total=%d restored=%d kept=%d' % (len(cur), len(restored), len(kept)))
    print('restored chars:', ''.join(restored))
    print('kept (TTF, not in bak151):', ''.join(c for c in kept if c not in bak))


if __name__ == '__main__':
    sys.exit(main())
