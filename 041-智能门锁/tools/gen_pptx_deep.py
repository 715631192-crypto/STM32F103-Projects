# -*- coding: utf-8 -*-
"""
Generate docs/code_walkthrough_deep.pptx
深度代码讲解版（>=70 页，40+ 分钟）：

  第 1 部分  整体架构与模块分工
  第 2 部分  按“编码顺序”逐块拆解代码逻辑（逐行讲解）
  第 3 部分  技术点 / API / 原理
  第 4 部分  优缺点、边界情况、Bug 风险
  第 5 部分  项目流程总结 + 讲解节奏表

内容以**当前代码**为准（已删除 MFRC522_ProbeMiso 等脚手架）。
"""
import math
import os
from pptx import Presentation
from pptx.util import Inches, Pt
from pptx.dml.color import RGBColor
from pptx.enum.text import PP_ALIGN, MSO_ANCHOR, MSO_AUTO_SIZE
from pptx.enum.shapes import MSO_SHAPE
from pptx.oxml.ns import qn

# ---------------- palette ----------------
BG          = RGBColor(0xF5, 0xF7, 0xFA)
DARK        = RGBColor(0x12, 0x21, 0x33)
ACCENT      = RGBColor(0x1F, 0x6F, 0xB2)
ACCENT2     = RGBColor(0x2E, 0x8B, 0x57)
WARN        = RGBColor(0xC0, 0x39, 0x2B)
CODE_BG     = RGBColor(0x0F, 0x1B, 0x2B)
CODE_FG     = RGBColor(0xE6, 0xED, 0xF3)
HEAD_FG     = RGBColor(0xFF, 0xFF, 0xFF)
SUB         = RGBColor(0x5A, 0x6B, 0x7B)
CARD        = RGBColor(0xFF, 0xFF, 0xFF)
ORANGE      = RGBColor(0xCB, 0x8E, 0x00)
PURPLE      = RGBColor(0x8E, 0x44, 0xAD)

FONT_UI   = "Microsoft YaHei"
FONT_CODE = "Consolas"

PPTX_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                         "..", "docs", "code_walkthrough_deep.pptx")

prs = Presentation()
prs.slide_width  = Inches(13.333)
prs.slide_height = Inches(7.5)
SW, SH = prs.slide_width, prs.slide_height
BLANK = prs.slide_layouts[6]

LX = Inches(0.7)                 # 左边距
CW = SW - Inches(1.4)            # 内容宽

CONTENT = []                     # 需要加页脚的页

# ---------------- 文本适配估算（★ 关键：不依赖打开 PPT 才算的自动缩放） ----------------
# 静态预览器（腾讯文档 / WPS 预览）不会重算 normAutofit 的缩放系数，
# 所以“超长自动缩放”在预览里是无效的 —— 必须在生成时就按内容算好行数与字号。

def _vis_em(text):
    """估算文本视觉宽度（em 单位）：CJK/全角≈1.0，窄字符≈0.35，其余 ASCII≈0.55"""
    w = 0.0
    for ch in text:
        if ord(ch) >= 0x2E80:
            w += 1.0
        elif ch in 'iljItf.,:;!|\'"()[]{} ':
            w += 0.35
        else:
            w += 0.55
    return w


def est_lines(text, width_in, size_pt):
    """估算文本在给定宽度/字号下的换行行数"""
    per_line = max(4.0, (width_in * 72.0) / size_pt)
    n = 0
    for seg in text.split('\n'):
        n += max(1, int(math.ceil(_vis_em(seg) / per_line)))
    return n


def fit_size(texts, width_in, height_in, base_size, line_factor, gap_pt=0.0,
             floor=7.0, extra_em=0.0):
    """从 base_size 逐级下调，直到所有文本行数×行高 ≤ 可用高度。"""
    s = float(base_size)
    while s > floor:
        total = 0.0
        for t in texts:
            total += est_lines(t, width_in, s) * s * line_factor
        if total + gap_pt <= height_in * 72.0:
            break
        s -= 0.5
    return s

# ---------------- helpers ----------------
def _set_run_font(run, name):
    run.font.name = name
    rPr = run._r.get_or_add_rPr()
    for tag in ("a:latin", "a:ea", "a:cs"):
        el = rPr.find(qn(tag))
        if el is None:
            el = rPr.makeelement(qn(tag), {})
            rPr.append(el)
        el.set("typeface", name)

def add_slide():
    return prs.slides.add_slide(BLANK)

def bg(slide, color=BG):
    s = slide.shapes.add_shape(MSO_SHAPE.RECTANGLE, 0, 0, SW, SH)
    s.fill.solid(); s.fill.fore_color.rgb = color
    s.line.fill.background(); s.shadow.inherit = False
    slide.shapes._spTree.remove(s._element)
    slide.shapes._spTree.insert(2, s._element)
    return s

def rect(slide, x, y, w, h, color, line=None, shadow=False):
    s = slide.shapes.add_shape(MSO_SHAPE.RECTANGLE, x, y, w, h)
    s.fill.solid(); s.fill.fore_color.rgb = color
    if line is None:
        s.line.fill.background()
    else:
        s.line.color.rgb = line; s.line.width = Pt(1)
    if not shadow:
        s.shadow.inherit = False
    return s

def textbox(slide, x, y, w, h, anchor=MSO_ANCHOR.TOP):
    tb = slide.shapes.add_textbox(x, y, w, h)
    tf = tb.text_frame
    tf.word_wrap = True
    tf.vertical_anchor = anchor
    tf.margin_left = Pt(4); tf.margin_right = Pt(4)
    tf.margin_top = Pt(2); tf.margin_bottom = Pt(2)
    return tb, tf

def add_para(tf, text, size, color, bold=False, align=PP_ALIGN.LEFT, font=FONT_UI,
             space_before=4, space_after=4, line=1.05):
    p = tf.add_paragraph()
    p.alignment = align
    p.space_before = Pt(space_before); p.space_after = Pt(space_after)
    try: p.line_spacing = line
    except Exception: pass
    r = p.add_run(); r.text = text
    r.font.size = Pt(size); r.font.bold = bold
    r.font.color.rgb = color; r.font.name = font
    _set_run_font(r, font)
    return p, r

def title_bar(slide, kicker, title):
    col = WARN if kicker in ("安全", "风险", "踩坑") else (
        PURPLE if kicker in ("原理", "技术") else (
        ORANGE if kicker in ("边界", "取舍") else ACCENT))
    rect(slide, 0, 0, SW, Inches(1.12), color=col)
    tb, tf = textbox(slide, Inches(0.5), Inches(0.10), SW-Inches(1), Inches(0.95),
                     anchor=MSO_ANCHOR.MIDDLE)
    add_para(tf, kicker, 12, RGBColor(0xFF, 0xE8, 0xE8) if col == WARN
             else RGBColor(0xCF, 0xE6, 0xFF), bold=True, space_after=2)
    add_para(tf, title, 25, HEAD_FG, bold=True, space_after=0)

def footer(slide, idx, total, note=""):
    rect(slide, 0, SH-Inches(0.32), SW, Inches(0.32), color=DARK)
    tb, tf = textbox(slide, Inches(0.4), SH-Inches(0.32), Inches(6.0), Inches(0.32),
                     anchor=MSO_ANCHOR.MIDDLE)
    add_para(tf, "STM32 智能门锁 · 深度代码讲解（按编码顺序）", 9,
             RGBColor(0xB8, 0xC4, 0xD0), space_after=0)
    tb2, tf2 = textbox(slide, SW-Inches(3.2), SH-Inches(0.32), Inches(2.8), Inches(0.32),
                       anchor=MSO_ANCHOR.MIDDLE)
    add_para(tf2, "%d / %d  %s" % (idx, total, note), 9, RGBColor(0xB8, 0xC4, 0xD0),
             align=PP_ALIGN.RIGHT, space_after=0)

def code_box(slide, x, y, w, h, code, size=11.0, line_spacing=1.02):
    # ★ 生成时就按内容算好字号：Consolas 每行可容纳的 em 数 ≈ 宽度/字号，
    #   行高 ≈ 字号×1.24。算不下就逐级降字号（最低 6pt），保证预览也不裁字。
    w_in = w / 914400.0
    h_in = h / 914400.0
    s = size
    while s > 6.0:
        per_line = max(8.0, ((w_in - 0.14) * 72.0) / s)
        lines = 0
        for seg in code.split('\n'):
            lines += max(1, int(math.ceil(_vis_em(seg) / per_line)))
        if lines * s * 1.24 + 8.0 <= h_in * 72.0:
            break
        s -= 0.5
    if s != size:
        print('  [fit] code %s -> %.1fpt' % (str(slide.slide_id), s))
    size = s

    rect(slide, x-Pt(2), y-Pt(2), w+Pt(4), h+Pt(4), color=RGBColor(0x22, 0x33, 0x48))
    tb, tf = textbox(slide, x, y, w, h, anchor=MSO_ANCHOR.TOP)
    tf.word_wrap = True
    tf.auto_size = MSO_AUTO_SIZE.NONE     # 已按内容算好字号，不再依赖打开后重算
    first = True
    for raw in code.split("\n"):
        p = tf.paragraphs[0] if first else tf.add_paragraph()
        first = False
        p.alignment = PP_ALIGN.LEFT
        p.space_before = Pt(0); p.space_after = Pt(0)
        try: p.line_spacing = line_spacing
        except Exception: pass
        run = p.add_run(); run.text = raw if raw else " "
        run.font.size = Pt(size); run.font.name = FONT_CODE
        run.font.color.rgb = CODE_FG
        _set_run_font(run, FONT_CODE)
    return tb

def bullets(slide, x, y, w, h, items, size=14, color=DARK, gap=6):
    # ★ 按内容行数自动降字号，保证不越过文本框（静态预览器不会自动缩放）
    w_in = w / 914400.0
    h_in = h / 914400.0
    texts = []
    for it in items:
        txt = it[0] if isinstance(it, tuple) else it
        lvl = it[1] if isinstance(it, tuple) else 0
        texts.append(('• ' if lvl == 0 else '– ') + txt)
    s = fit_size(texts, w_in - 0.18, h_in, size, 1.45,
                 gap_pt=gap * len(items), floor=9.0)
    if s != size:
        print('  [fit] bullets -> %.1fpt' % s)
    size = s

    tb, tf = textbox(slide, x, y, w, h)
    for it in items:
        txt, lvl, c, b = (it if isinstance(it, tuple) else (it, 0, color, False))
        p = tf.add_paragraph()
        p.level = lvl
        p.space_before = Pt(gap); p.space_after = Pt(0)
        try: p.line_spacing = 1.08
        except Exception: pass
        run = p.add_run(); run.text = ("• " if lvl == 0 else "– ") + txt
        run.font.size = Pt(size); run.font.color.rgb = c
        run.font.bold = b; run.font.name = FONT_UI
        _set_run_font(run, FONT_UI)
    return tb

def callout(slide, x, y, w, h, text, color=ACCENT2, size=12.5):
    # ★ 提示条也按内容自动降字号（预留左右内边距与行高）
    w_in = (w - Pt(16)) / 914400.0
    h_in = (h - Pt(6)) / 914400.0
    s = fit_size([text], w_in, h_in, size, 1.42, floor=8.5)
    if s != size:
        print('  [fit] callout -> %.1fpt' % s)
    size = s

    rect(slide, x, y, w, h,
         color=RGBColor(0xEC, 0xF6, 0xF0) if color == ACCENT2 else RGBColor(0xFB, 0xEC, 0xEA))
    rect(slide, x, y, Pt(5), h, color=color)
    tb, tf = textbox(slide, x+Pt(10), y+Pt(3), w-Pt(16), h-Pt(6), anchor=MSO_ANCHOR.MIDDLE)
    add_para(tf, text, size, DARK, space_after=0)
    return tb

def mk(kicker, title):
    s = add_slide(); bg(s, BG); title_bar(s, kicker, title)
    CONTENT.append(s)
    return s

def _bottom_cap(top, h, note_h, gap=0.12):
    """保证内容 + 提示条不越过页脚（页脚顶边在 7.18in）。"""
    max_bottom = 7.10
    if note_h:
        max_h = max_bottom - top - gap - note_h
    else:
        max_h = max_bottom - top
    return max(1.6, min(h, max_h))


def code_slide(kicker, title, code, note=None, size=11.0, top=1.42, h=4.5,
               note_h=0.95, note_color=ACCENT2):
    s = mk(kicker, title)
    h = _bottom_cap(top, h, note_h if note else 0)
    code_box(s, LX, Inches(top), CW, Inches(h), code, size=size)
    if note:
        callout(s, LX, Inches(top + h + 0.12), CW, Inches(note_h), note, color=note_color)
    return s

def bullet_slide(kicker, title, items, note=None, size=13.5, gap=9,
                 top=1.5, h=4.6, note_h=1.0, note_color=ACCENT2):
    s = mk(kicker, title)
    h = _bottom_cap(top, h, note_h if note else 0, gap=0.1)
    bullets(s, LX, Inches(top), CW, Inches(h), items, size=size, gap=gap)
    if note:
        callout(s, LX, Inches(top + h + 0.1), CW, Inches(note_h), note, color=note_color)
    return s

def table_slide(kicker, title, rows, widths, note=None, top=1.45, rh=0.5,
                note_h=0.9, size=11.5, note_color=ACCENT2):
    """rows[0] 为表头。widths 为 Inches 列表。

    ★ 行高按单元格内容自动计算（估算换行数），不再用固定行高 ——
      固定行高在静态预览器里会把两行文字裁成一行半（截图里的裁字问题）。
    """
    s = mk(kicker, title)
    widths_in = [wd / 914400.0 for wd in widths]

    def row_heights(sz):
        hs = []
        for ri, row in enumerate(rows):
            lines = 1
            for ci, cell in enumerate(row):
                lines = max(lines, est_lines(cell, widths_in[ci] - 0.18, sz))
            # 1.5 倍行高 + 0.24in 底部余量：预览器行高普遍比 PowerPoint 大，
            # 余量不足就会像截图那样把字的下半截裁掉
            hs.append(lines * sz * 1.5 / 72.0 + 0.24)
        return hs

    # 若总高超出可用区（到页脚为止，且要给提示条留位），逐级降字号
    avail = 7.10 - top - (note_h + 0.15 if note else 0.10)
    sz = float(size)
    while sz > 8.0:
        if sum(row_heights(sz)) <= avail:
            break
        sz -= 0.5
    if sz != size:
        print('  [fit] table -> %.1fpt' % sz)
    heights = row_heights(sz)

    # ★ 兜底：降到字号下限仍放不下时，按比例压缩行高，保证表格+提示条绝不越界
    total = sum(heights)
    if total > avail:
        k = avail / total
        heights = [hh * k for hh in heights]
        print('  [fit] table rows squeezed x%.2f' % k)

    y = Inches(top)
    for ri, row in enumerate(rows):
        x = LX
        h = Inches(heights[ri])
        for ci, cell in enumerate(row):
            col = DARK if ri == 0 else (CARD if ri % 2 else RGBColor(0xEC, 0xF1, 0xF6))
            rect(s, x, y, widths[ci], h, color=col, line=RGBColor(0xCB, 0xD5, 0xE0))
            tb, tf = textbox(s, x+Pt(4), y, widths[ci]-Pt(8), h,
                             anchor=MSO_ANCHOR.MIDDLE)
            add_para(tf, cell, sz, HEAD_FG if ri == 0 else DARK,
                     bold=(ri == 0), space_after=0)
            x = x + widths[ci]
        y = y + h
    if note:
        ny = top + sum(heights) + 0.15
        callout(s, LX, Inches(ny), CW, Inches(note_h), note, color=note_color)
    return s

def steps_slide(kicker, title, steps, note=None, top=1.5, note_h=0.9, note_color=ACCENT2):
    s = mk(kicker, title)
    n = len(steps)
    # 可用高度平分给每一步（步骤多时自动压扁），保证不越过页脚
    avail = 7.10 - top - (note_h + 0.12 if note else 0.10)
    box_h = max(0.40, min(0.60, avail / n - 0.08))
    step_gap = max(0.02, min(0.12, (avail - box_h * n) / max(1, n - 1)))
    font = max(9.5, min(13.0, box_h * 72.0 * 0.42))
    y = Inches(top)
    for i, (t, c) in enumerate(steps):
        rect(s, LX, y, Inches(0.45), Inches(box_h), color=c)
        tb, tf = textbox(s, LX, y, Inches(0.45), Inches(box_h), anchor=MSO_ANCHOR.MIDDLE)
        add_para(tf, str(i+1), min(15, font+2), HEAD_FG, bold=True,
                 align=PP_ALIGN.CENTER, space_after=0)
        rect(s, LX+Inches(0.55), y, CW-Inches(0.55), Inches(box_h),
             color=CARD, line=RGBColor(0xCB, 0xD5, 0xE0))
        tb, tf = textbox(s, LX+Inches(0.7), y, CW-Inches(0.85), Inches(box_h),
                         anchor=MSO_ANCHOR.MIDDLE)
        add_para(tf, t, font, DARK, space_after=0)
        y = y + Inches(box_h + step_gap)
    if note:
        callout(s, LX, y + Inches(0.08), CW, Inches(note_h), note, color=note_color)
    return s


# =====================================================================
# 封面
# =====================================================================
s = add_slide()
bg(s, DARK)
rect(s, 0, Inches(2.35), SW, Inches(2.5), color=ACCENT)
tb, tf = textbox(s, Inches(0.9), Inches(2.45), SW-Inches(1.8), Inches(2.3),
                 anchor=MSO_ANCHOR.MIDDLE)
add_para(tf, "STM32 金融级安全物联网智能门锁", 34, HEAD_FG, bold=True, space_after=8)
add_para(tf, "深度代码讲解 · 按「编码顺序」逐行拆解", 21, RGBColor(0xCF, 0xE6, 0xFF), space_after=6)
add_para(tf, "架构分工 → 逐块代码 → 技术原理 → 边界与风险 → 流程总结", 14,
         RGBColor(0xCF, 0xE6, 0xFF), space_after=0)
tb2, tf2 = textbox(s, Inches(0.9), Inches(5.15), SW-Inches(1.8), Inches(1.5))
add_para(tf2, "讲解时长：40 ~ 60 分钟（72 页，源码级逐行）", 13,
         RGBColor(0x9F, 0xB3, 0xC8), space_after=4)
add_para(tf2, "平台：STM32F103C8 @72MHz · 20KB RAM / 64KB Flash · FreeRTOS V10.5.1", 12,
         RGBColor(0x9F, 0xB3, 0xC8), space_after=3)
add_para(tf2, "外设：RC522 · AS608 · DS3231 · W25Q64 · OLED · ESP-12F · 舵机/蜂鸣/RGB", 12,
         RGBColor(0x9F, 0xB3, 0xC8), space_after=0)

# =====================================================================
# 路线图（按编码顺序的 9 个里程碑）
# =====================================================================
steps_slide("路线图", "跟着「写代码的人」走一遍：9 个里程碑", [
    ("① 定约束：64KB Flash / 20KB RAM 能做什么 → 定架构分层", ACCENT),
    ("② 写骨架：main() → smart_lock_app_start() → board_init() 9 步", ACCENT2),
    ("③ 上 RTOS：FreeRTOSConfig 裁剪 → 5 个静态任务 + 6 个静态队列", ACCENT2),
    ("④ 接通输入：键盘 / RC522 刷卡 / AS608 指纹 → 统一事件模型", ACCENT2),
    ("⑤ 写决策：handle_board_input() 总入口 + lock_controller 状态机", ACCENT),
    ("⑥ 做凭据：常数时间比较 / 虚拟前后缀胁迫码 / TOTP 动态口令", WARN),
    ("⑦ 落盘：config_store A/B 信封 + event_log CRC32 环形日志", PURPLE),
    ("⑧ 补驱动与抗干扰：I2C 事务锁、RC522 自愈、喂狗、低功耗", PURPLE),
    ("⑨ 复盘：边界情况、Bug 风险、流程串联与讲解节奏", ORANGE),
], note="每一块都按「先想清楚为什么 → 再写代码 → 最后讲踩过的坑」三段式推进。",
   note_h=0.75, top=1.35)

# =====================================================================
# 第 1 部分：架构与模块分工
# =====================================================================
s = add_slide(); bg(s, DARK)
rect(s, 0, Inches(3.0), SW, Inches(1.4), color=ACCENT)
tb, tf = textbox(s, Inches(0.9), Inches(3.05), SW-Inches(1.8), Inches(1.3),
                 anchor=MSO_ANCHOR.MIDDLE)
add_para(tf, "第 1 部分", 16, RGBColor(0xCF, 0xE6, 0xFF), space_after=4)
add_para(tf, "整体架构与模块分工", 32, HEAD_FG, bold=True, space_after=0)

bullet_slide("架构", "先接受现实：STM32F103C8 的资源约束", [
    ("Flash 64KB：实测已用 Code=53388 + RO=8104 ≈ 60KB，余量 <4KB。", 0, WARN, True),
    ("RAM 20KB：ZI=18208 + RW=408 + 1KB MSP 栈，几乎吃满。", 0, WARN, True),
    ("结论①：不能用动态分配（碎片+失败风险）→ 全部静态分配，heap 只留 4KB 给 cJSON/MqttKit。", 0, DARK, False),
    ("结论②：不能用 C++ / 大库 → 纯 C89，标准库只留最小集（无 printf 浮点）。", 0, DARK, False),
    ("结论③：不能上 TLS/mbedTLS → 云端走 OneNET token（HMAC-SHA1 签名）而非证书。", 0, DARK, False),
    ("结论④：字库是 ROM 大户 → 只保留实际用到的 73 个汉字（trim 省 ~2.5KB）。", 0, DARK, False),
    ("结论⑤：省空间先砍字库，绝不砍安全逻辑（常数时间比较/CRC/双因子）。", 0, ACCENT2, True),
], note="所有后面的设计取舍，根源都在这页：不是“想不想”，而是“只能这样”。", note_h=0.85)

s = mk("架构", "四层架构：为什么要分层")
layers = [
    ("① 应用逻辑层  firmware/app/   纯 C，不碰硬件 → 可在 PC 上跑单元测试", ACCENT),
    ("② 板级适配层  firmware/stm32/  board_port.c + smart_lock_app.c（任务/队列/菜单）", ACCENT2),
    ("③ 驱动 / 中间件  firmware/HARDWARE/ · MIDDLEWARE/ · drivers/ · USER/", PURPLE),
    ("④ RTOS 内核  FreeRTOS V10.5.1（抢占式，静态分配，configUSE_TIMERS=0）", ORANGE),
]
y = Inches(1.45)
for txt, col in layers:
    rect(s, LX, y, CW, Inches(0.82), color=col)
    tb, tf = textbox(s, LX+Inches(0.25), y, CW-Inches(0.5), Inches(0.82),
                     anchor=MSO_ANCHOR.MIDDLE)
    add_para(tf, txt, 14.5, HEAD_FG, bold=True, space_after=0)
    y = y + Inches(1.0)
callout(s, LX, Inches(5.65), CW, Inches(1.2),
        "分层的硬收益：① 认证/锁控/凭据这些“最需要正确性”的代码不依赖 MCU，能在 PC 上用 "
        "firmware/tests/ 跑断言（test_main.c 已覆盖 TOTP/base64/config_store/event_log）；"
        "② 换 MCU 只改 board_port，不动业务；③ 驱动出问题（如 RC522 掉电）能在 board 层自愈，"
        "不污染决策逻辑。", size=12)

table_slide("架构", "模块分工速查表", [
    ("目录", "职责", "关键点"),
    ("firmware/app/", "纯业务逻辑：锁控/凭据/TOTP/访客/周期/配置信封/日志", "可 PC 单测，零硬件依赖"),
    ("firmware/stm32/", "板级适配 + 任务编排：board_port.c / smart_lock_app.c", "唯一知道“引脚/中断”的地方"),
    ("firmware/HARDWARE/", "片外驱动：RC522/AS608/OLED/I2C/W25Q64/舵机/蜂鸣/RGB", "软 I2C 带事务级递归锁"),
    ("firmware/MIDDLEWARE/", "cJSON / MqttKit / sha1 / base64 / onenet_token", "第三方，注释保留英文"),
    ("firmware/drivers/", "ds3231 / esp8266_at / json_codec 适配层", "本项目改造过，算自研"),
    ("firmware/USER/", "app_wifi.c（ESP8266 联网状态机）+ 中断向量头", "参考工程复用，尽量不改"),
    ("firmware/FreeRTOS/", "内核源码 V10.5.1（RVDS 移植）", "vendored，不动"),
], [Inches(2.9), Inches(6.0), Inches(3.7)],
    note="记住一条主线：app/ 负责“对不对”，stm32/ 负责“怎么接”，HARDWARE/ 负责“怎么驱动”。",
    rh=0.46, note_h=0.7)

table_slide("架构", "引脚资源全景（GPIO 几乎占满）", [
    ("外设", "引脚", "说明"),
    ("ESP8266", "USART1 PA9(TX,AF_PP) / PA10(RX,浮空)", "@115200，AT 指令 + MQTT"),
    ("指纹 AS608", "USART2 PA2 / PA3", "模块内部存指纹，擦 Flash 不清"),
    ("调试串口", "USART3 PB10 / PB11", "@115200，printf 走这里（COM7）"),
    ("RC522 读卡", "SPI1 PA4(CS) PA5(SCK) PA6(MISO) PA7(MOSI) + PC14(RST)", "4.5MHz，模式0"),
    ("W25Q64", "SPI2 PB12~PB15", "配置 A/B 槽 + 卡表 + 日志"),
    ("OLED + DS3231", "软 I2C PB8(SCL) / PB9(SDA)", "★ 共总线 → 必须事务级加锁"),
    ("舵机 / 蜂鸣 / RGB", "TIM3_CH3 PB0 / PC15 / PA0,PA1,PB1", "50Hz PWM 0°锁 / 90°开"),
    ("键盘 4×4", "行 PA11,PA12,PA15,PB3  列 PB4~PB7", "PA15/PB3/PB4 需关 JTAG 释放"),
    ("唤醒键", "PA8 (EXTI8)", "中断只置标志，不碰 RTOS 对象"),
], [Inches(2.0), Inches(5.2), Inches(5.4)],
    note="引脚紧张导致的连锁设计：JTAG 必须 GPIO_Remap_SWJ_JTAGDisable 释放 PA15/PB3/PB4；"
         "PB2(BOOT1) 悬空；OLED 与 RTC 只能共用一条 I2C（埋下花屏隐患，后面详讲）。",
    rh=0.44, note_h=0.85, size=11)

code_slide("架构", "并发模型：5 任务 + 6 队列（创建代码）", r'''/* firmware/stm32/src/smart_lock_app.c —— 全部 Static 版本 */
input_queue  = xQueueCreateStatic(12, sizeof(board_input_event_t), ...);
remote_queue = xQueueCreateStatic( 4, sizeof(remote_command_t),     ...);
log_queue    = xQueueCreateStatic( 8, sizeof(event_log_record_t),   ...);
notify_queue = xQueueCreateStatic( 8, sizeof(board_notification_t), ...);
ui_queue     = xQueueCreateStatic( 6, sizeof(app_ui_message_t),     ...);
log_request_queue = xQueueCreateStatic(4, sizeof(uint32_t),         ...);

xTaskCreateStatic(input_task,   "input",   256, NULL, 3U, input_task_stack,   ...);
xTaskCreateStatic(access_task,  "access",  384, NULL, 4U, access_task_stack,  ...);
xTaskCreateStatic(network_task, "network", 768, NULL, 2U, network_task_stack, ...);
xTaskCreateStatic(log_task,     "log",     256, NULL, 1U, log_task_stack,     ...);
xTaskCreateStatic(ui_task,      "ui",      160, NULL, 1U, ui_task_stack,      ...);''',
    note="队列缓冲区与任务栈都来自 .bss 全局数组，不向 4KB 堆申请 → 无碎片、无分配失败。"
         "优先级 access(4) > input(3) > network(2) > log/ui(1)。",
    size=11.5, h=3.9, note_h=0.95)

table_slide("架构", "5 个任务的分层职责（采集/决策/通信/持久/呈现）", [
    ("层", "任务", "优先级", "栈(字)", "周期/阻塞", "职责"),
    ("决策", "access_task", "4", "384", "等 input_queue(100ms)", "唯一仲裁：校验凭据、跑菜单、锁控"),
    ("采集", "input_task", "3", "256", "vTaskDelay(10ms)", "扫键盘/卡/指纹，产出统一事件"),
    ("通信", "network_task", "2", "768", "vTaskDelay(50ms)", "ESP8266 MQTT 收发、远程命令"),
    ("持久", "log_task", "1", "256", "等 log_queue(50ms)", "事件落盘 W25Q64 环形日志"),
    ("呈现", "ui_task", "1", "160", "等 ui_queue(永久)", "刷 OLED；只被队列唤醒"),
], [Inches(1.0), Inches(2.0), Inches(1.1), Inches(1.0), Inches(2.6), Inches(4.6)],
    note="运行时共 6 个 RTOS 任务（5 应用 + 1 idle）；configUSE_TIMERS=0 所以没有定时器任务。"
         "access_task 栈 384 字要跑菜单与凭据比对；network 栈最大 768 字（MQTT 局部 392B，曾溢出）。",
    rh=0.46, note_h=0.95, size=11)

bullet_slide("架构", "为什么用队列，而不是“生产者直接调消费者”", [
    ("在抢占式 RTOS 里，“直接调” = 让一个任务替另一个任务干活，会同时带来 5 个问题：", 0, DARK, True),
    ("① 生产者被阻塞：input_task 会卡在判决逻辑里，10ms 扫描与 RC522 自愈全停。", 0, DARK, False),
    ("② 优先级丢失：判决代码会跑在调用方优先级（3 或 2），而不是设计好的 4。", 0, DARK, False),
    ("③ 共享内存竞态：直接传指针/全局 → 两个任务并发改同一块数据，要自己加锁。", 0, DARK, False),
    ("④ 无法削峰：连按/连刷时事件直接丢失。input_queue 12 槽可以缓冲。", 0, DARK, False),
    ("⑤ 双判竞态：刷卡与云端命令同时到 → 两个上下文同时改锁状态、同时驱动舵机。", 0, WARN, True),
    ("队列用“一次值拷贝”换来：解耦、异步、保优先级、免锁、削峰、单一决策点。", 0, ACCENT2, True),
], note="核心结论：access_task 是唯一决策中心，所有输入（本地+云端）只“发”不“判”，"
        "天然被队列串行化，从架构上消除了双判竞态。", note_h=0.9)

# =====================================================================
# 第 2 部分：逐块拆解（按编码顺序）
# =====================================================================
s = add_slide(); bg(s, DARK)
rect(s, 0, Inches(3.0), SW, Inches(1.4), color=ACCENT2)
tb, tf = textbox(s, Inches(0.9), Inches(3.05), SW-Inches(1.8), Inches(1.3),
                 anchor=MSO_ANCHOR.MIDDLE)
add_para(tf, "第 2 部分", 16, RGBColor(0xD8, 0xF0, 0xE4), space_after=4)
add_para(tf, "逐块拆解代码逻辑（逐行讲解）", 32, HEAD_FG, bold=True, space_after=0)

# ---- 里程碑 ② 骨架 ----
code_slide("编码①", "main()：极薄的入口", r'''/* firmware/stm32/src/main.c */
int main(void)
{
    if (!smart_lock_app_start()) {
        /* 启动失败不可恢复：停等独立看门狗复位（发布版 IWDG=1）
           调试版可直接挂调试器看调用栈 */
        for (;;) { }
    }
    vTaskStartScheduler();     /* 启动 FreeRTOS 调度器，此后由内核接管 */
    /* 正常永不返回；只有堆不足导致调度器没起来才会走到这里 */
    for (;;) { }
}''',
    note="设计要点：72MHz 时钟由启动文件的 SystemInit() 完成，main 不配时钟。"
         "main 极薄——所有初始化下沉到 smart_lock_app_start()，避免“两份初始化互相打架”。"
         "该函数据返回 false 只有三种可能：Flash 无响应 / 配置非法 / 任务或队列创建失败。",
    size=12, h=2.9, note_h=1.5)

code_slide("编码②", "smart_lock_app_start() 上半：初始化 + 运行态复位", r'''bool smart_lock_app_start(void)
{
    /* 1) 先准备日志存储后端（函数指针表，应用层不认识 W25Q64） */
    const event_log_storage_t storage = {
        .context = NULL, .slot_count = EVENT_LOG_SLOT_COUNT,
        .read = board_log_read, .write = board_log_write, .erase = board_log_erase };

    /* 2) 硬件初始化 + 读配置 + 校验配置 + 初始化日志，任一失败即返回 false */
    if ((!board_init()) || (!board_security_load(&security_config)) ||
        (!security_config_valid(&security_config)) ||
        (!event_log_init(&event_log, &storage))) {
        return false;
    }

    /* 3) 运行态清零：失败计数 / 菜单 / 门状态 / 第二因子 / 锁控器 */
    memset(&pin_auth_state, 0, sizeof(pin_auth_state));
    menu_reset();
    door_opened_at = 0U;  door_ajar_reported = false;
    clear_pending_factor();
    last_activity_at = board_unix_time();
    lock_controller_init(&controller, AUTO_LOCK_DELAY_SECONDS);
    set_physical_state(controller.state);   /* 上电先落到“锁闭”物理态 */''',
    note="为什么用函数指针表 storage：让 app/event_log.c 完全不依赖 W25Q64，换介质只换这张表。"
         "set_physical_state 在这里先调一次，保证“上电一定是锁着的”。",
    size=10.5, h=4.6, note_h=0.95)

code_slide("编码③", "board_init() 第 1~4 步：系统 / 串口 / 显示 / 存储", r'''  /* 1) 中断优先级分组 + 释放 PA15/PB3/PB4（不释放键盘就用不了） */
  sys_init();  delay_init();
  /* 2) 三路串口：USART1=ESP8266  USART2=指纹  USART3=调试打印 */
  usart3_init(DEBUG_UART_BAUD);
  usart1_init(ESP8266_UART_BAUD);
  usart2_init(AS608_UART_BAUD);
  /* 3) 显示与本地交互：IIC_Init 必须先于 OLED（锁在 IIC_Init 里建） */
  IIC_Init();  OLED_Init();  OLED_Clear();      /* 不清屏 → 随机噪点带 */
  buzzer_init();  rgb_init();
  /* 4) 存储：W25Q64，日志与配置都在这里 */
  if (W25Q64_Init() != 1U) {
      board_diagnostic_report(BOARD_DIAG_LOG_STORAGE_FAILURE);
      return false;                              /* Flash 挂了直接放弃启动 */
  }''',
    note="顺序讲究：I2C 先 Init 才能创建总线互斥锁 s_i2c_mtx；OLED_Clear() 必须在 Init 后立即做。"
         "W25Q64 失败直接 return false —— 没有存储就没有审计与配置，宁可不起来。",
    size=11, h=4.0, note_h=1.2)

code_slide("编码④", "board_init() 第 5 步：RC522 二次初始化（踩坑经验）", r'''  /* 5) 射频读卡 */
  {
      MFRC522_Init();                 /* 内含 PC14 RST 硬复位脉冲 + 软复位 + 重配寄存器 */

      /* 实测：MCU 复位后 RC522 经常“首次初始化不成功”（MISO 无驱动 / Version 0x00）
         ——上电瞬间芯片内部状态机没起来，写进去的配置读不回来。
         再跑一次 MFRC522_Init()（再来一遍硬复位）就能救回。
         缺了这一步，上板现象就是“卡贴上去没反应”。 */
      if (!MFRC522_Check()) {
          MFRC522_Init();
      }
      if (MFRC522_Check()) {
          printf("[OK] MFRC522 RFID (Version 0x%02X)\r\n", (unsigned)MFRC522_Version());
      } else {
          printf("[ERR] MFRC522 no response (Version 0x%02X) -- "
                 "check SCK/MOSI/MISO/PWR\r\n", (unsigned)MFRC522_Version());
      }
  }''',
    note="★ 判据是 MFRC522_Check()（回读 Init 写过的 TModeReg=0x8D / TPrescalerReg=0x3E），"
         "绝不能用 VersionReg —— 它是固定硅片 ID，芯片配置全丢了照样读得回 0x92。",
    size=10.5, h=4.5, note_h=0.95, note_color=WARN)

code_slide("编码⑤", "board_init() 第 6~9 步：时钟 / 执行器 / 键盘 / 网络", r'''  /* 6) 时钟：优先真实 DS3231（芯片里存 UTC），探测不到自动回落软时钟 */
  const uint8_t rtc_ok = DS3231_Init();
  DS3231_Time now;  DS3231_GetTime(&now);
  printf("[RTC] %s %04u-%02u-%02u %02u:%02u:%02u UTC\r\n",
         rtc_ok ? "DS3231 online" : "soft clock", ...);

  /* 7) 锁体执行器：上电先回到锁闭位 */
  servo_init();  servo_lock();  s_lock_engaged = true;

  /* 8) 键盘与唤醒键 + 卡表加载 */
  keypad_hardware_init();  wake_key_init();
  s_matrix_io.select_row   = keypad_select_row;      /* 函数指针：解耦扫描算法与硬件 */
  s_matrix_io.read_column  = keypad_read_column;
  keypad_scan_init(&s_scan);  keypad_input_init(&s_keypad);
  (void)card_table_load();

  /* 9) 云端 + 看门狗 */
  app_wifi_init();
  watchdog_init();
  printf("[BOOT] smart lock board ready\r\n");
  return true;''',
    note="DS3231 在线一眼可见：日志出现 [RTC] DS3231 online <UTC> 即正常；"
         "出现 soft clock 说明 I2C 0xD0 无应答 → 断电时间会跳回编译时刻 → TOTP 会失效。",
    size=10.5, h=4.8, note_h=0.85)

code_slide("编码⑥", "FreeRTOSConfig.h：为 20KB RAM 做的裁剪", r'''#define configUSE_PREEMPTION              1      /* 抢占式 */
#define configCPU_CLOCK_HZ                72000000UL
#define configTICK_RATE_HZ                1000   /* 1ms 节拍 */
#define configMAX_PRIORITIES              5      /* 本项目正好用满 1~4 */
#define configMINIMAL_STACK_SIZE          128    /* idle 任务栈 */

#define configSUPPORT_STATIC_ALLOCATION   1      /* ★ 全部 xTask/xQueueCreateStatic */
#define configSUPPORT_DYNAMIC_ALLOCATION  1      /* cJSON/MqttKit 还要 pvPortMalloc */
#define configTOTAL_HEAP_SIZE             (4*1024)   /* 只服务第三方库，不是任务栈 */

#define configUSE_MUTEXES                 1      /* I2C 总线锁 */
#define configUSE_RECURSIVE_MUTEXES       1      /* 递归锁（oled.c 会嵌套进 I2C） */
#define configUSE_TIMERS                  0      /* ★ 无 xTimer* 调用 → 省约 1.2KB */
#define configCHECK_FOR_STACK_OVERFLOW    2      /* 栈溢出检查（方法2） */

#define configPRIO_BITS                   4
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY  5   /* ISR 可调用 RTOS API 的边界 */
#define configASSERT(x)  if((x)==0){ taskDISABLE_INTERRUPTS(); for(;;){} }''',
    note="★ 血泪：照抄参考工程的 16KB 堆会直接链接失败（L6406E: No space in execution regions），"
         "因为 20KB RAM 已被 MSP 栈 + 静态区 + 驱动缓冲占掉约 11KB。4KB 是算出来的，不是拍的。",
    size=10.5, h=4.6, note_h=0.95, note_color=WARN)

code_slide("编码⑦", "freertos_hooks.c：静态内存供给 + 三个“死给你看”的钩子", r'''static StaticTask_t idle_task_control;
static StackType_t  idle_task_stack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory(StaticTask_t **tc, StackType_t **st, uint32_t *sz)
{   /* configSUPPORT_STATIC_ALLOCATION=1 时内核强制要求提供 idle 任务内存 */
    *tc = &idle_task_control;  *st = idle_task_stack;  *sz = configMINIMAL_STACK_SIZE;
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *task_name)
{
    (void)task; (void)task_name;
    taskDISABLE_INTERRUPTS();
    for (;;) { }        /* ★ 不打印！调试器看 task_name，量产机靠看门狗复位 */
}

void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();
    for (;;) { }        /* 4KB 堆耗尽（只可能是 cJSON/MqttKit）*/
}''',
    note="这三个钩子 + configASSERT 全是 for(;;) 死循环且不打印 —— 这就是“静默死机”的根源："
         "串口输出突然停在某行中间、xTickCount 冻结。排查方法见第 4 部分。",
    size=11, h=3.9, note_h=1.2, note_color=WARN)

# ---- 里程碑 ③ 任务 ----
code_slide("编码⑧", "input_task：10ms 节拍的采集循环（逐行）", r'''static void input_task(void *argument)
{
    board_input_event_t event;
    (void)argument;
    for (;;) {
        /* 10ms 节拍：与 buzzer_process()/rgb_process() 周期一致，
           也让键盘消抖（KEYPAD_DEBOUNCE_MS=25）有足够采样拍数 */
        if (board_poll_input(&event)) {
            /* 非阻塞投递：队列满只等 20ms，超时就丢并上报诊断，绝不挂死采集 */
            if (xQueueSend(input_queue, &event, pdMS_TO_TICKS(20U)) != pdTRUE) {
                board_diagnostic_report(BOARD_DIAG_INPUT_QUEUE_FULL);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10U));    /* 主动让出 CPU → 其他任务照常跑 */
    }
}''',
    note="三个细节：① 10ms 是键盘消抖与蜂鸣/RGB 处理的公约数；② 发送用 20ms 超时而非 portMAX_DELAY，"
         "避免队列满时采集任务被拖死；③ vTaskDelay 是“让出”不是“忙等”，所以自愈的 13ms 不卡系统。",
    size=11.5, h=3.9, note_h=1.2)

code_slide("编码⑨", "access_task：唯一决策中心（逐行）", r'''static void access_task(void *argument)
{
    board_input_event_t event;
    remote_command_t command;
    TickType_t last_tick = xTaskGetTickCount();
    for (;;) {
        /* ① 本地输入：阻塞等 100ms，没事件就去做菜单与远程，不空转 */
        if (xQueueReceive(input_queue, &event, pdMS_TO_TICKS(100U)) == pdTRUE) {
            handle_board_input(&event, board_unix_time());
        }
        /* ② 管理菜单周期动作：指纹采集步进、等刷卡、无操作超时（≈100ms 一轮） */
        menu_poll(board_unix_time());
        /* ③ 清空远程命令队列（0 超时，有多少收多少，不阻塞） */
        while (xQueueReceive(remote_queue, &command, 0U) == pdTRUE) {
            const uint64_t now = board_unix_time();
            last_activity_at = now;
            handle_remote_command(&command, now);
            memset(&command, 0, sizeof(command));   /* 清掉内存里的凭据残留 */
        }
        /* ④ 每秒一次的周期事务：自动上锁、门未关提醒、超时熄屏 */
        if ((xTaskGetTickCount() - last_tick) >= pdMS_TO_TICKS(1000U)) { ... }
    }
}''',
    note="为什么 access 优先级最高(4)：刷卡/输密要立刻判。为什么 receive 用 100ms 而不是死等："
         "给菜单轮询和远程命令留出执行窗口 —— 这是“单任务里做时间片”的写法。",
    size=10.5, h=4.9, note_h=0.85)

code_slide("编码⑩", "network_task / log_task / ui_task（逐行）", r'''/* network_task：50ms 一轮，唯一碰网络 */
    if (收到云端报文) {
        /* 解析后投递给 access 决策；失败只重试，绝不自己开锁 */
        if (xQueueSend(remote_queue, &command, pdMS_TO_TICKS(20U)) != pdTRUE) { ... }
    }
    /* 取 access 产生的通知上报云端（0 超时，取完就走） */
    if (xQueueReceive(notify_queue, &notification, 0U) == pdTRUE) { 上报; }
    vTaskDelay(pdMS_TO_TICKS(50U));

/* log_task：等 log_queue(50ms)，同时也处理“查记录”请求 */
    if (xQueueReceive(log_queue, &record, pdMS_TO_TICKS(50U)) == pdTRUE) {
        if (!event_log_append(&event_log, &record))
            board_diagnostic_report(BOARD_DIAG_LOG_STORAGE_FAILURE);
    }
    while (xQueueReceive(log_request_queue, &offset, 0U) == pdTRUE) {
        /* 菜单翻看历史：读最近记录 → 回显给 ui_queue */
        if (event_log_read_recent(&event_log, offset, &record)) queue_ui_log(&record, true);
        else queue_ui_log(NULL, false);
    }

/* ui_task：永久阻塞，没消息就睡，不占 CPU */
    if (xQueueReceive(ui_queue, &ui, portMAX_DELAY) == pdTRUE) {
        board_ui_power(ui.power_on);
        if (ui.is_menu) { 菜单渲染; } else { 通用渲染; }
    }''',
    note="三者形态各异但原则一致：阻塞等待而非轮询；能 0 超时取完就取完；"
         "log/ui 优先级最低(1)，因为“记日志/刷屏”晚几十毫秒人无感。",
    size=10.2, h=5.0, note_h=0.7)

# ---- 里程碑 ④ 输入 ----
code_slide("编码⑪", "board_poll_input()：三类输入如何汇成一条事件", r'''bool board_poll_input(board_input_event_t *event)
{
    /* 菜单激活期间：读头（卡/指纹）整体让路，只喂菜单按键 */
    if (s_menu_active) {
        const char key = keypad_scan_step(&s_matrix_io, &s_scan);
        if (key != KEYPAD_CHARACTER_NONE) {
            buzzer_beep(BEEP_KEY);
            event->type = BOARD_MENU_KEY;  event->digits[0] = key;
            event->digit_count = 1U;  return true;
        }
        return false;      /* 菜单期间不轮询读头 */
    }

    /* 键盘：数字进输入缓冲，'#'/A/B/D 产生事件，'C' 取消 */
    { ... keypad_scan_step() ... keypad_input_feed() ... keypad_input_tick() ... }

    /* 轮询计数：每 CARD_POLL_TICKS(10) 扫一次卡，每 FINGER_POLL_TICKS 扫一次指纹 */
    ++s_poll_ticks;
    if ((s_poll_ticks % CARD_POLL_TICKS)   == 0U) { if (poll_card(event))   return true; }
    if ((s_poll_ticks % FINGER_POLL_TICKS) == 0U) { if (poll_finger(event)) return true; }
    return false;
}''',
    note="统一事件模型（board_input_event_t）让认证逻辑只写一套 switch。"
         "菜单期间读头让路是为了避免“菜单里等刷卡”和“正常刷卡开锁”互相干扰。",
    size=10.5, h=4.9, note_h=0.85)

code_slide("编码⑫", "read_card_uid()：单轮 3 次重试 + 读后 HALT", r'''/* 为什么重试 3 次：原来只做 1 次 Request+Anticoll，一次失败整轮作废，
   用户体感“时灵时不灵”（卡还在移动、刚进场的瞬间采样不到就错过一整轮）。
   3 次独立 REQA/ATQA 往返把单轮命中率从约 60% 拉到 95% 以上。 */
static bool read_card_uid(uint8_t *uid)
{
    uint8_t tag_type[2];
    uint8_t attempt;

    for (attempt = 0U; attempt < CARD_READ_ATTEMPTS; ++attempt) {   /* =3 */
        if (MFRC522_Request(PICC_REQIDL, tag_type) != MI_OK) {
            continue;                       /* 本刻无卡/未回包，立刻再试 */
        }
        if (MFRC522_Anticoll(uid) != MI_OK) {
            continue;                       /* 防冲撞/BCC 校验失败：绝不放行 */
        }
        /* 读到就把卡放回 IDLE，卡继续压在读头上也能被下一轮寻到 */
        MFRC522_Halt();
        return true;
    }
    return false;
}''',
    note="★ MFRC522_Halt() 是关键：不加它，卡停在 ACTIVE 态不再应答 REQA → "
         "“读一次再也读不到，必须拿开重放“。加上后卡压在读头上也能连续识别。",
    size=10.5, h=4.6, note_h=0.95)

code_slide("编码⑬", "card_reader_heal()：两级自愈（逐行）", r'''/* 连续 50 轮无卡（≈5s）触发一次；每 6 次自愈（≈30s）无条件整片重初始化 */
static void card_reader_heal(void)
{
    const bool regs_ok = (MFRC522_Check() != 0U);   /* 回读配置寄存器，不是读 ID */

    if ((!regs_ok) || (++s_card_heal_count >= CARD_REINIT_AFTER_HEAL)) {
        s_card_heal_count = 0U;
        printf("[RC522] heal: %s, re-init\r\n",
               regs_ok ? "periodic refresh" : "config lost");
        MFRC522_Init();                    /* 硬复位 + 重配，把丢掉的配置补回来 */
        if (!MFRC522_Check()) {
            printf("[RC522] WARN re-init failed -- check SCK/MOSI/MISO/PWR\r\n");
        }
    } else {
        MFRC522_AntennaOn();               /* 寄存器还在，只是天线位被清了 */
    }
}

/* 调用点：只在“连续无卡（空闲）”时触发 —— 场上无卡，耗时 ~13ms 也安全 */
if (!read_card_uid(uid)) {
    if (++s_card_heal_streak >= CARD_HEAL_AFTER_POLLS) {   /* 50 */
        s_card_heal_streak = 0U;
        card_reader_heal();
    }
    return false;
}
s_card_heal_streak = 0U;      /* 读到卡即清零 */
s_card_heal_count  = 0U;      /* 周期刷新重新计时 */''',
    note="★ 为什么不阻塞系统：自愈跑在 input_task 里，里面的 delay_ms 在调度器运行时走 vTaskDelay"
         "（让出 CPU），只挂起 input_task 自己约 13ms，access(4)/network(2) 照常运行。",
    size=10, h=5.1, note_h=0.7)

code_slide("编码⑭", "MFRC522_Init()：硬复位 + 软复位 + 两个可靠性优化", r'''void MFRC522_Init(void)
{
    /* SPI1: PA5/PA7 复用推挽, PA6 浮空输入, PA4 软件 CS；72MHz/16 = 4.5MHz */
    ... SPI_Init(SPI1, &s);  SPI_Cmd(SPI1, ENABLE);

    /* 复位：RC522 没有上电自动清状态机，MCU 复位后经常起不来 */
    GPIO_ResetBits(GPIOC, GPIO_Pin_14);  delay_ms(2);   /* PC14 = RST 拉低 */
    GPIO_SetBits(GPIOC, GPIO_Pin_14);    delay_ms(2);   /* 释放 */
    rc522_set_bitmask(CommandReg, 0x10);   delay_ms(2); /* PowerDown=1 */
    rc522_clear_bitmask(CommandReg, 0x10); delay_ms(2); /* PowerDown=0 */
    rc522_write(CommandReg, PCD_RESETPHASE); delay_ms(5);  /* 等振荡器起振 */

    /* 优化①：无卡等待超时 30(≈25ms) → 10(≈8ms)。这个超时只在天线区没卡时走完，
       25ms 纯属白等，还会阻塞输入任务；缩到 8ms 后轮询反而能提到 100ms。 */
    rc522_write(TReloadRegL, 10);
    /* 优化②：接收增益拉满。复位值 0x48=33dB，弱耦合时“时灵时不灵”；
       写 0x70=48dB（寄存器允许的最大值）显著提高读到概率。 */
    rc522_write(RFCfgReg, 0x70);

    MFRC522_AntennaOn();
}''',
    note="两个优化都是从“用户体感”倒推的：轮询慢 → 缩超时；读不到 → 拉增益。"
         "门锁场景同一时刻只有一张卡，不用担心多卡增益过载。",
    size=10.2, h=5.0, note_h=0.75)

# ---- 里程碑 ⑤ 决策 ----
code_slide("编码⑮", "handle_board_input()：认证总入口（全貌）", r'''static void handle_board_input(const board_input_event_t *event, uint64_t now)
{
    last_activity_at = now;      /* 防“静止 60s 熄屏”把输入截断 */
    switch (event->type) {
    case BOARD_AUTH_PIN:         /* 密码：胁迫 > 业主 > 第二因子 > 访客 > 失败 */
    case BOARD_AUTH_TOTP:        /* 动态口令：6 位数字 → totp_verify */
    case BOARD_AUTH_VISITOR:     /* APP 一次性临时码 */
    case BOARD_AUTH_FINGERPRINT: /* 指纹：单因子 or 置 pending 第一因子 */
    case BOARD_AUTH_DURESS_FINGERPRINT:  /* 胁迫指纹 → 静默开门 + 后台告警 */
    case BOARD_AUTH_RFID:        /* 卡：单因子 or 置 pending 第一因子 */
    case BOARD_AUTH_PERIODIC:    /* 周期口令（时段窗口） */
    case BOARD_DOOR_OPENED:      /* 门磁开 → 锁控 DOOR_OPENED */
    case BOARD_DOOR_CLOSED:      /* 门磁关 → 自动落锁 + 审计 */
    case BOARD_TAMPER_TRIGGERED: /* 防拆 → 报警 */
    case BOARD_MENU_REQUEST:     /* 'D'：进管理菜单（先验主密码/30s 免验） */
    case BOARD_MENU_KEY:         /* 菜单态原始按键 */
    case BOARD_KEYPAD_CANCEL:    /* 'C'：退出菜单 */
    default: break;
    }
}''',
    note="所有凭据走同一个 switch：决策在函数内完成，执行交给 grant/deny。"
         "这样“加一种新认证方式”只需加一个 case，不用改执行链。",
    size=11, h=4.7, note_h=0.9)

code_slide("编码⑯a", "PIN 分支（上）：锁定 → 胁迫 → 业主码", r'''case BOARD_AUTH_PIN:
    /* 顺序即安全优先级，一个 else-if 链写完，绝不让低优先级分支先命中 */

    if (keypad_is_locked(now)) {                 /* ① 先看键盘是否被锁 */
        deny_access(PIN, id, true, now);         /*    锁定期直接拒绝 */
    }
    else if (pin_credential_matches(&duress_pin, secret, secret_len,
                                    digits, n, true)) {
        grant_duress(PIN, id, now);              /* ② 胁迫码：静默开门 + 告警 */
    }
    else if (pin_credential_matches(&owner_pin, secret, secret_len,
                                    digits, n, true)) {
        keypad_clear_failures();                 /* ③ 命中业主码：清失败计数 */
        ... 双因子判断见下一页 ...
    }
    ... 访客码 / 失败计数见下一页 ...''',
    note="先判“锁定”再判“胁迫”再判“业主”：顺序错了，被锁定的设备仍可能被胁迫码打开。",
    size=11.5, h=4.4, note_h=0.8, note_color=WARN)

code_slide("编码⑯b", "PIN 分支（下）：双因子细节 → 访客码 → 失败计数", r'''    else if (pin_credential_matches(&owner_pin, secret, secret_len,
                                    digits, n, true)) {
        keypad_clear_failures();
        if (second_factor_mode == DISABLED)      /* ③ 业主码，单因子 */
            grant_access(PIN, id, now);
        else if ((pending_first_factor_until >= now) && 匹配第一因子)
            { clear_pending_factor(); grant_access(PIN, user_id, now); }
        else {                                   /* 双因子但没第一因子 */
            clear_pending_factor(); deny_access(PIN, id, false, now);
            queue_ui("Use first factor first");  /* 提示先刷指纹/刷卡 */
        }
    }
    else if (visitor_credential_verify_and_consume(&visitor, secret,
              secret_len, digits, n, now)) {     /* ④ 访客一次性码 */
        keypad_clear_failures();
        if (board_security_save(&security_config))  /* ★ 消耗后必须落盘 */
            grant_access(VISITOR, id, now);
    }
    else deny_access(PIN, id, keypad_register_failure(now), now);  /* ⑤ 失败计数 */
    break;''',
    note="访客码刻意绕过第二因子（访客没有卡/指纹）。"
         "★ visitor 消耗后必须 board_security_save 落盘，否则重启后一次性码又能用一次。",
    size=11, h=4.7, note_h=0.8, note_color=WARN)

code_slide("编码⑰a", "状态机（上）：守卫段 —— 防拆 / 报警 / 锁定优先处理", r'''lock_state_t lock_controller_handle(lock_controller_t *c,
                                lock_control_event_t event, uint64_t now)
{
    if (c == NULL) return LOCK_STATE_ALARM;          /* 空指针→最安全态 */

    if (event == LOCK_CONTROL_TAMPER) {              /* 防拆：立即报警并锁死 */
        c->state = LOCK_STATE_ALARM; c->unlock_deadline = 0U; return c->state;
    }
    if ((event == LOCK_CONTROL_CLEAR_ALARM) && (c->state == LOCK_STATE_ALARM))
        { c->state = LOCK_STATE_LOCKED; return c->state; }

    if (c->state == LOCK_STATE_ALARM)                /* 报警中无视其它事件 */
        return c->state;

    if (c->state == LOCK_STATE_LOCKOUT) {            /* 锁定中：只等解锁/到期 */
        if ((event == LOCK_CONTROL_CLEAR_LOCKOUT) ||
            ((event == LOCK_CONTROL_TICK) && (now >= c->lockout_deadline)))
            { c->state = LOCK_STATE_LOCKED; c->lockout_deadline = 0U; }
        return c->state;
    }
    ... switch 事件处理见下一页 ...
}''',
    note="守卫段把最高危事件放在最前：TAMPER 一票进入 ALARM；ALARM/LOCKOUT 态下普通事件一律被吞掉。",
    size=11.5, h=4.6, note_h=0.8, note_color=WARN)

code_slide("编码⑰b", "状态机（下）：switch 事件 —— 开锁 / 上锁 / 门磁 / 节拍", r'''    switch (event) {
    case LOCK_CONTROL_AUTH_GRANTED:
    case LOCK_CONTROL_REMOTE_UNLOCK:
        c->state = LOCK_STATE_UNLOCKED;
        c->unlock_deadline = now + c->auto_lock_seconds; break;   /* 自动落锁期限 */

    case LOCK_CONTROL_FORCE_LOCK:
        c->state = LOCK_STATE_LOCKED; c->unlock_deadline = 0U; break;

    case LOCK_CONTROL_DOOR_OPENED:
        c->door_closed = false; break;

    case LOCK_CONTROL_DOOR_CLOSED:
        c->door_closed = true;
        if (c->state == LOCK_STATE_UNLOCKED)   /* 门一关立即落锁 */
            { c->state = LOCK_STATE_LOCKED; c->unlock_deadline = 0U; }
        break;

    case LOCK_CONTROL_TICK:                    /* 兜底：到期且门已关 → 落锁 */
        if ((c->state == LOCK_STATE_UNLOCKED) && (c->unlock_deadline != 0U) &&
            (now >= c->unlock_deadline) && c->door_closed)
            { c->state = LOCK_STATE_LOCKED; c->unlock_deadline = 0U; }
        break;

    default: break;
    }
    return c->state;
}''',
    note="状态机只改“状态”，永不直接动硬件 —— 这是全项目最重要的解耦点。"
         "注意 door_closed 参与自动落锁判断：门还开着就不落锁（避免夹人/夹门）。",
    size=11.5, h=4.9, note_h=0.75)

code_slide("编码⑱", "grant_access / deny_access / set_physical_state", r'''/* 唯一真正动硬件的两个点 */
static void set_physical_state(lock_state_t state)
{
    board_lock_set(state != LOCK_STATE_UNLOCKED);   /* 非 UNLOCKED 都上锁 */
    board_alarm_set(state == LOCK_STATE_ALARM);     /* 仅 ALARM 拉报警 */
}

static void grant_access(lock_auth_method_t method, uint16_t user_id, uint64_t now)
{
    (void)lock_controller_handle(&controller, LOCK_CONTROL_AUTH_GRANTED, now);
    set_physical_state(controller.state);       /* ① 物理解锁 */
    board_auth_feedback(true);                  /* ② 蜂鸣/绿灯 */
    queue_ui(controller.state, "Access granted", true);   /* ③ → ui_queue */
    queue_audit(LOCK_EVENT_UNLOCK, method, user_id, 1U, now);  /* ④ → log_queue */
}

static void deny_access(lock_auth_method_t method, uint16_t user_id,
                        bool locked_out, uint64_t now)
{
    if (locked_out) {
        lock_controller_set_lockout(&controller, pin_auth_state.locked_until);
        set_physical_state(controller.state);
        queue_ui(controller.state, "Try again later", true);
        queue_audit(LOCK_EVENT_LOCKOUT, method, user_id, 0U, now);
    } else {
        board_auth_feedback(false);
        queue_ui(controller.state, "Access denied", true);
        queue_audit(LOCK_EVENT_AUTH_FAILURE, method, user_id, 0U, now);
    }
}''',
    note="三路并行（物理/界面/审计）都通过队列异步完成，access 任务不阻塞在 I2C 或 Flash 上。"
         "这是“决策快、执行慢”场景的标准解法。",
    size=10.2, h=5.0, note_h=0.7)

# ---- 里程碑 ⑥ 凭据 ----
code_slide("编码⑲", "lock_constant_time_equal()：防时序侧信道", r'''/* app/src/lock_auth.c —— 比较耗时与内容无关 */
bool lock_constant_time_equal(const uint8_t *left, const uint8_t *right,
                              size_t length)
{
    uint8_t difference = 0U;
    if ((left == NULL) || (right == NULL)) return false;
    for (size_t i = 0U; i < length; ++i)
        difference |= (uint8_t)(left[i] ^ right[i]);   /* 永远比完，不提前 return */
    return difference == 0U;
}''',
    note="★ 为什么不用 strcmp：strcmp 遇到首个不同字节就返回，比较时间随“匹配前缀长度”变化，"
         "攻击者可借响应耗时逐字符爆破 PIN。这里把差异“或”进一个字节、最后才判断，耗时恒定。"
         "注意 length 不一致时也不提前退出（靠上层保证等长）。",
    size=12, h=2.5, note_h=1.9, note_color=WARN)

code_slide("编码⑳", "pin_matches()：虚拟前后缀 = 胁迫码机制", r'''/* 允许“虚拟前后缀”：在正确码前后输任意数字也算对 → 实现胁迫码 */
static bool pin_matches(const char *stored_pin, size_t stored_length,
                        const char *input, size_t input_length, bool allow_virtual)
{
    size_t last_start;
    if (stored_length == 0U || input_length < stored_length) return false;
    if ((!allow_virtual) && (input_length != stored_length)) return false;
    /* 允许虚拟前后缀时，起始位置可以从 0 滑到 (input_len - stored_len) */
    last_start = allow_virtual ? (input_length - stored_length) : 0U;

    uint8_t matched = 0U;
    for (size_t start = 0U; start <= last_start; ++start) {
        uint8_t difference = 0U;
        for (size_t i = 0U; i < stored_length; ++i)
            difference |= (uint8_t)stored_pin[i] ^ (uint8_t)input[start + i];
        if (difference == 0U) matched = 1U;        /* 子串命中即算对 */
    }
    return matched != 0U;
}

lock_auth_result_t lock_auth_verify_pin(...) {
    if (now < state->locked_until) return LOCK_AUTH_LOCKED;      /* 锁定期 */
    if (pin_matches(...)) { state->failed_attempts = 0U; return LOCK_AUTH_GRANTED; }
    if (++state->failed_attempts >= policy->maximum_failures) {   /* 满 3 次 */
        state->locked_until = now + policy->lockout_seconds;      /* 锁 60s */
        return LOCK_AUTH_LOCKED;
    }
    return LOCK_AUTH_DENIED;
}''',
    note="胁迫码用法：被劫持时输入“正确码+任意数字”，门照开但后台记录胁迫事件。"
         "内层比较同样用“或”累积，不提前退出 → 仍是常数时间。",
    size=10.2, h=5.0, note_h=0.7)

code_slide("编码21", "hotp_generate()：RFC4226 动态截断（逐行）", r'''static uint32_t hotp_generate(const uint8_t *secret, size_t secret_length,
                              uint64_t counter, uint8_t digits)
{
    uint8_t counter_bytes[8];  uint8_t digest[SHA1_DIGEST_SIZE];

    /* ① 计数器按大端（网络序）写入 8 字节 */
    for (size_t i = 0U; i < 8; ++i)
        counter_bytes[7U - i] = (uint8_t)(counter >> (i * 8U));

    /* ② HMAC-SHA1(secret, counter) → 20 字节摘要 */
    hmac_sha1(secret, secret_length, counter_bytes, 8, digest);

    /* ③ 动态截断：取摘要末字节低 4 位作为偏移量 offset ∈ [0,15] */
    uint8_t offset = (uint8_t)(digest[19] & 0x0F);

    /* ④ 从 offset 起取 4 字节，首字节掩 0x7F 去掉符号位 → 31 位正整数 */
    uint32_t binary = ((uint32_t)(digest[offset]   & 0x7F) << 24U) |
                      ((uint32_t)(digest[offset+1] & 0xFF) << 16U) |
                      ((uint32_t)(digest[offset+2] & 0xFF) <<  8U) |
                      ((uint32_t)(digest[offset+3] & 0xFF));

    /* ⑤ 取低 digits 位（6 位 → % 1000000） */
    return binary % decimal_modulus(digits);
}''',
    note="digest[19] 是 SHA1 第 20 个字节（索引从 0）。0x7F 掩掉最高位避免出现负数，"
         "这也是为什么 31 位而不是 32 位。与 Google Authenticator 完全兼容。",
    size=10.5, h=4.7, note_h=0.9)

code_slide("编码22", "totp_verify()：时间窗容差与下溢防护", r'''bool totp_verify(const uint8_t *secret, size_t secret_length,
                 uint64_t unix_time, uint32_t time_step, uint8_t digits,
                 uint32_t candidate, uint8_t allowed_window)
{
    const uint64_t base = (time_step == 0U) ? 0U : unix_time / time_step;
    /* 参数白名单：digits 6~8，window ≤2，防调用方传非法值 */
    if (secret==NULL || secret_length==0U || time_step==0U ||
        digits<6U || digits>8U || allowed_window>2U) return false;

    /* 遍历 [base-w, base+w] 共 2w+1 个计数器，任一命中即放行 */
    for (int32_t delta = -(int32_t)allowed_window;
         delta <= (int32_t)allowed_window; ++delta) {
        uint64_t counter;
        /* ★ 防下溢：delta<0 且 base 不够减时跳过，否则 counter 会绕成天文数字 */
        if ((delta < 0) && (base < (uint64_t)(-delta))) continue;
        counter = (delta < 0) ? base - (uint64_t)(-delta)
                              : base + (uint64_t)delta;
        if (hotp_generate(secret, secret_length, counter, digits) == candidate)
            return true;
    }
    return false;
}''',
    note="调用：totp_verify(totp_secret, len, now, 30U, 6U, code, 1U) —— 30s 步长、6 位、"
         "允许前后各 1 个时间步（±30s），抵消时钟与网络抖动。★ 时钟偏差 >30s 会直接失效。",
    size=10.5, h=4.8, note_h=0.85)

# ---- 里程碑 ⑦ 持久化 ----
code_slide("编码23", "config_store：掉电安全的 A/B 双槽信封", r'''/* 信封格式（W25Q64，一个槽 = 一个 4KiB 扇区）：
     offset 0  : magic   'S''L''C''1'  = 0x31434C53
     offset 4  : version u16
     offset 6  : payload_length u16
     offset 8  : sequence u32
     offset 12 : payload[payload_length]      ← 逐字段编码，不 memcpy 结构体
     末尾 4B   : crc32（覆盖 magic..payload）

   读：两槽都解析，取“CRC 正确 且 sequence 较大”的那个。
   写：永远写“当前非活动”槽（先擦后写），sequence = 活动序号 + 1。
       ★ 擦/写中途断电 → 旧槽仍然完好，配置不丢。 */

bool config_store_load(const config_store_media_t *m, board_security_config_t *c);
bool config_store_save(const config_store_media_t *m,
                       const board_security_config_t *c);''',
    note="两个关键决策：① 逐字段编码而非 memcpy 结构体 —— 避开编译器填充字节的跨平台/跨版本差异；"
         "② A/B 槽 + 序号 + CRC 三重保护 —— 让管理员密码、卡片表在意外断电时不损坏。",
    size=10.5, h=4.6, note_h=0.95)

code_slide("编码24a", "event_log：CRC32 实现（逐行）", r'''/* CRC32（IEEE 802.3 多项式 0xEDB88320），与以太网一致 */
uint32_t event_log_crc32(const void *data, size_t length)
{
    const uint8_t *b = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFU;                 /* 初值全 1 */

    for (size_t i = 0U; i < length; ++i) {
        crc ^= b[i];                            /* 先异或低 8 位 */
        for (uint8_t bit = 0U; bit < 8U; ++bit) {
            /* 用掩码代替 if：-(crc&1) 得到全 1 或全 0，无分支 → 常数时间 */
            uint32_t mask = (uint32_t)(-(int32_t)(crc & 1U));
            crc = (crc >> 1U) ^ (0xEDB88320U & mask);
        }
    }
    return ~crc;                                 /* 结果取反 */
}

bool event_log_record_valid(const event_log_record_t *r)
{
    if (r == NULL || r->magic != EVENT_LOG_MAGIC) return false;
    return event_log_crc32(r, offsetof(event_log_record_t, crc32)) == r->crc32;
}''',
    note="CRC 挡住“写一半的坏记录”：magic 不符或 CRC 不符即整条丢弃，绝不把坏记录当有效日志。",
    size=11, h=4.4, note_h=0.75)

code_slide("编码24b", "event_log：环形写入与序号回绕（逐行）", r'''/* 环形写：填 magic/sequence → 算 CRC → 擦当前槽 → 写 → 槽指针+1 取模 */
bool event_log_append(event_log_t *log, event_log_record_t *rec)
{
    rec->magic = EVENT_LOG_MAGIC;                /* ① 先填魔数 */
    rec->sequence = log->next_sequence;          /* ② 再填序号（用于排序） */
    rec->crc32 = event_log_crc32(rec,
                    offsetof(event_log_record_t, crc32));   /* ③ 覆盖 crc 前的数据 */

    if (!log->storage.erase(log->next_slot)) return false;  /* ④ 先擦后写 */
    if (!log->storage.write(log->next_slot, rec)) return false;  /* ⑤ 再写 */

    log->next_slot = (log->next_slot + 1U) % slot_count;    /* ⑥ 环形前进 */
    ++log->next_sequence;
    return true;
}

/* 比较序号新旧：(int32_t)(a - b) > 0
   ★ 用有符号差正确处理 32 位回绕：直接比较 a>b 在回绕点会判反。
     例：a=0xFFFFFFF0, b=0x00000005 → 差为负的大数 → int32 下溢 → 判 a 旧，符合事实 */''',
    note="★ event_type 按 uint8_t 持久化 → 新增枚举只能追加到末尾，不能重排或重用旧值。",
    size=11, h=4.6, note_h=0.7)

# ---- 里程碑 ⑧ 驱动抗干扰 ----
code_slide("编码25", "i2c_soft：为什么锁要加在“事务层”", r'''static SemaphoreHandle_t s_i2c_mtx = NULL;      /* 总线互斥锁（递归） */

static void i2c_lock(void)
{
    /* 调度器未启动时是单线程，无需加锁（锁也可能还没建） */
    if (s_i2c_mtx && xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
        xSemaphoreTakeRecursive(s_i2c_mtx, portMAX_DELAY);
}
static void i2c_unlock(void)
{
    if (s_i2c_mtx && xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
        xSemaphoreGiveRecursive(s_i2c_mtx);
}

void IIC_Init(void) {                            /* 必须先于 OLED/DS3231 的 Init */
    ... PB8/PB9 开漏输出、拉高 ...
    if (s_i2c_mtx == NULL)
        s_i2c_mtx = xSemaphoreCreateRecursiveMutex();   /* 全局唯一，建一次 */
}

/* 每个对外接口都把“整段事务”包在锁里（START..STOP 不可被抢占） */
uint8_t IIC_WriteBytesRaw(uint8_t dev, const uint8_t *d, uint16_t len)
{ i2c_lock();  iic_start(); ...; iic_stop(); i2c_unlock(); return ok; }
uint8_t IIC_ReadRegs(uint8_t dev, uint8_t reg, uint8_t *buf, uint16_t len)
{ i2c_lock();  iic_start(); ... Repeated START ...; i2c_unlock(); return ok; }''',
    note="★ 用递归锁的原因：oled.c 的 s_oled_mtx 是递归的，会嵌套进入 I2C；"
         "锁序恒为 s_oled_mtx → s_i2c_mtx，无环 → 不会死锁。递归锁还带优先级继承。",
    size=10.2, h=5.0, note_h=0.72)

code_slide("编码26", "OLED 显示与字库：为什么每行要补满 128px", r'''/* oled.c 要点 */
void OLED_ShowText(uint8_t x, uint8_t y, const char *text)
{
    /* 字库 oledfont_cn.h 由 tools/gen_oled_font.py 生成/校验（--check）
       现收录 73 个常用汉字：为省 ROM 做过 trim（约省 2.5KB）。
       ★ 遇到字库里没有的码点 → 画一个“空心方框”，不会崩，但会显示成空白块。 */
    ...
    /* 每行显示完要 ui_pad_line() 把剩余像素补成空白 */
    ui_pad_line();
}

/* 为什么必须补满：OLED 是“点阵覆盖写”，不清行的话上一屏更长的文字
   会在这一屏留下尾巴（残像），看起来像“文字错乱”的另一种形态。 */
/* ★ OLED_Clear() 必须在 OLED_Init() 之后立刻调用一次，
   否则上电时 DDRAM 内容随机 → 屏幕出现噪点带。 */''',
    note="三条实战经验：① 字库缺字画方框（gen_oled_font.py --check 可提前发现）；"
         "② 每行补满防残像；③ Init 后立刻 Clear 防噪点带。",
    size=10.5, h=4.6, note_h=0.95)

bullet_slide("编码27", "W25Q64 驱动要点（配合 config_store / event_log）", [
    (" SPI2（PB12~PB15）软件 CS；指令：0x9F 读 ID、0x06 写使能、0x20 扇区擦除、0x02 页编程、0x03 读。", 0, DARK, False),
    (" 擦除最小单位 4KiB：所以一个配置槽 = 一个扇区，一条日志也占一个扇区（空间换简单）。", 0, DARK, False),
    (" 写之前必须写使能（0x06），且每次写/擦后要轮询状态寄存器等 BUSY 位清零。", 0, DARK, False),
    (" 地址映射：配置 A 槽 0x000000 / B 槽 0x001000 / 卡表 0x002000 / 日志尾部 128 扇区。", 0, DARK, False),
    (" 寿命：约 10 万次擦写；日志环形均摊写入，避免热点扇区；单扇区写失败要上报诊断。", 0, ORANGE, False),
    (" ★ 掉电写一半 → 靠 magic + CRC 在读取侧判无效并丢弃，不在写入侧补救。", 0, ACCENT2, True),
], note="驱动层只保证“能擦能写”，“掉电安全”是 config_store/event_log 在它之上做的信封设计。",
   note_h=0.8, size=12.5)

bullet_slide("编码28", "ESP8266 / app_wifi.c：联网状态机", [
    (" 状态机：STEP_AT(初始化 AT) → STEP_CHECK(检查模块) → STEP_JOIN(连 WiFi)", 0, DARK, False),
    ("        → STEP_MQTT(连 MQTT) → STEP_SUB(订阅主题) → STEP_RUN(正常运行)。", 0, DARK, False),
    (" 任一步失败都会带退避重试回到前一步，不会把 network_task 卡死在里面。", 0, ACCENT2, False),
    (" 下行：云端 JSON → 解析 → remote_command_t → remote_queue → access_task 决策。", 0, DARK, False),
    (" 上行：access 产生 notify → notify_queue → network_task 发布到物模型主题。", 0, DARK, False),
    (" 鉴权：OneNET token = HMAC-SHA1(DeviceSecret, 待签串) 做 base64，带 et 过期时间。", 0, DARK, False),
    (" ★ 已知风险：模组固件崩溃会死循环（rst cause / Fatal exception），非 MCU 责任；", 0, WARN, True),
    (" 峰值 300mA+，与 RC522 共用 3V3 时要就近并 100µF+0.1µF，否则 RC522 会棕色复位。", 0, WARN, False),
], note="app_wifi.c 来自参考工程，原则是不随便改它；出问题先分清“MCU 责任 / 模块责任”。",
   note_h=0.8, size=12.5)

bullet_slide("编码29", "键盘扫描与消抖（为什么 10ms 节拍）", [
    (" 4×4 矩阵：行 PA11/PA12/PA15/PB3 逐行输出低电平，列 PB4~PB7 上拉输入读回。", 0, DARK, False),
    (" PA15/PB3/PB4 默认是 JTAG 引脚 → 必须 GPIO_Remap_SWJ_JTAGDisable 释放，否则键盘全废。", 0, WARN, True),
    (" 扫描用函数指针 s_matrix_io（select_row / read_column）解耦算法与硬件，便于移植与测试。", 0, DARK, False),
    (" 消抖 KEYPAD_DEBOUNCE_MS=25：input_task 的 10ms 节拍保证有 ≥2 次采样来确认稳定。", 0, DARK, False),
    (" 输入缓冲：数字进缓冲，'#' 提交，'B' 删位，'C' 取消，'D' 进菜单，'A' 菜单上翻。", 0, DARK, False),
    (" 超时自动清空（keypad_input_tick）：防止输一半走人后残留，被下一个人接着输。", 0, ACCENT2, False),
    (" 菜单激活时 board_poll_input 不喂 keypad_input（改喂 menu_key），避免两套解析打架。", 0, DARK, False),
], note="10ms 这个数字不是随手定的：它是消抖 25ms 的约数，也是蜂鸣/RGB 处理的公约数。",
   note_h=0.8, size=12.5)

bullet_slide("编码30", "AS608 指纹模块：协议与存储", [
    (" 通信：USART2（PA2/PA3），指令包 + 校验和，模块上电即等待主机指令。", 0, DARK, False),
    (" 用户页：1~47 普通用户，48 = 管理员指纹，49 = 胁迫指纹（AS608_DURESS_PAGE）。", 0, DARK, False),
    (" 指令：PS_GetImage 采集 → PS_GenChar 生成特征 → PS_RegModel 合并 → PS_Store 存页。", 0, DARK, False),
    (" 识别：PS_Search 在整个指纹库里比对，返回命中的页码与得分。", 0, DARK, False),
    (" ★ 指纹存在模块内部 Flash：擦 W25Q64 不会清掉指纹；改主密码也影响不到它。", 0, ACCENT2, True),
    (" 管理员指纹(48) 或管理卡开锁后给 30s 免验窗口 → 期间按 'D' 可直接进菜单。", 0, DARK, False),
    (" 双因子模式（FINGERPRINT_PIN）下指纹只置 pending 第一因子，还必须再输 PIN。", 0, WARN, False),
], note="指纹与密码/卡片完全解耦（存在另一个芯片里）—— 这既是优点（难一起丢失）"
        "也是风险面（模块本身可被单独攻击）。", note_h=0.8, size=12.5)

code_slide("编码31", "远程开锁链路：云端命令如何走完全程", r'''[云端] MQTT 下发 {"params":{"unlock":...}} 或远程开锁指令
   │
   ▼
[ESP8266] USART1 收到报文 → app_wifi.c 解析 JSON
   │
   ▼
[network_task] 组装 remote_command_t
   │  xQueueSend(remote_queue, &command, 20ms)
   ▼
[access_task] while (xQueueReceive(remote_queue, &command, 0U) == pdTRUE)
   │  handle_remote_command(&command, now)
   │    ├─ 校验 request_id 防重放（last_accepted_request_id）
   │    ├─ 命令类型：远程开锁 / 强制上锁 / 查询状态
   │    └─ ★ 最终仍走 lock_controller_handle(REMOTE_UNLOCK) —— 与本地认证同一入口
   ▼
grant_access(REMOTE, ...) → 物理解锁 + 界面 + 审计（三路并行）

/* ★ 关键点：远程命令没有“特权通道”，它和刷卡一样只是“一种输入”，
   最终都汇到 lock_controller 这一个状态机里，权限与锁定策略完全一致。 */''',
    note="安全上的重要设计：远程不开后门。request_id 去重防网络重放；"
         "远程开锁同样受 LOCKOUT / ALARM 状态约束（报警中一律拒绝）。",
    size=10.5, h=4.9, note_h=0.85)

# =====================================================================
# 第 3 部分：技术点 / API / 原理
# =====================================================================
s = add_slide(); bg(s, DARK)
rect(s, 0, Inches(3.0), SW, Inches(1.4), color=PURPLE)
tb, tf = textbox(s, Inches(0.9), Inches(3.05), SW-Inches(1.8), Inches(1.3),
                 anchor=MSO_ANCHOR.MIDDLE)
add_para(tf, "第 3 部分", 16, RGBColor(0xE8, 0xD8, 0xF0), space_after=4)
add_para(tf, "技术点 / API / 原理", 32, HEAD_FG, bold=True, space_after=0)

table_slide("技术", "FreeRTOS API 速查（本项目实际用到的）", [
    ("API", "作用", "本项目用法 / 注意"),
    ("xTaskCreateStatic", "创建任务（栈与 TCB 由调用方提供）", "5 个任务全部用它；栈是全局数组"),
    ("xQueueCreateStatic", "创建队列（存储区由调用方提供）", "6 个队列全部用它；值为拷贝语义"),
    ("xQueueSend / Receive", "投递 / 取走消息（可带超时）", "0=不等待；portMAX_DELAY=永久阻塞"),
    ("vTaskDelay", "阻塞延时（让出 CPU，非忙等）", "input 10ms / network 50ms"),
    ("xSemaphoreCreateRecursiveMutex", "创建递归互斥量（带优先级继承）", "I2C 总线锁 s_i2c_mtx"),
    ("xTaskGetSchedulerState", "查询调度器是否已运行", "delay_ms 与 i2c_lock 都靠它分支"),
    ("vApplicationGetIdleTaskMemory", "静态分配时提供 idle 任务内存", "freertos_hooks.c 必须实现"),
    ("vApplicationStackOverflowHook", "栈溢出钩子", "本工程是死循环，不打印 → 静默死机"),
], [Inches(3.2), Inches(4.2), Inches(5.4)],
    note="全部是 Static 版本 + 递归互斥量，没有动态创建、没有软件定时器、没有事件组 —— "
         "这是 20KB RAM 下的主动裁剪，不是功能缺失。",
    rh=0.46, note_h=0.9, size=11)

bullet_slide("技术", "原理①：抢占式调度与优先级继承", [
    (" configUSE_PREEMPTION=1 + 1ms SysTick：高优先级任务就绪立刻抢走 CPU。", 0, DARK, False),
    (" 任务只在自己调用 vTaskDelay / 带超时 xQueueReceive 时才“让出”，让出的是自己。", 0, DARK, True),
    (" 所以“一个任务卡 13ms”对其他任务是零影响 —— RC522 自愈就是靠这一点。", 0, ACCENT2, True),
    (" 同优先级任务按时间片轮转（configUSE_TIME_SLICING=1）：log 与 ui 都是 1。", 0, DARK, False),
    (" 优先级继承：低优先级任务持有互斥量时，会临时被抬到等待者的优先级，", 0, DARK, False),
    (" 防止“中优先级任务插队”导致高优先级被无限期阻塞（无界优先级反转）。", 0, DARK, False),
    (" 中断优先级边界：configMAX_SYSCALL_INTERRUPT_PRIORITY=5 → 优先级数字 ≥5 的", 0, SUB, False),
    (" ISR 才能调用带 FromISR 后缀的 RTOS API。本项目 ISR 只置标志，不碰 RTOS 对象。", 0, SUB, False),
], note="一句话：让出是主动的，抢占是内核做的；锁要短、要递归、要有继承。", note_h=0.75, size=13)

bullet_slide("技术", "原理②：队列的“值拷贝”语义", [
    (" xQueueSend 把整个结构体**拷贝**进队列存储区，不是存指针。", 0, DARK, True),
    (" 收益①：发送方发完就可以改/复用自己的缓冲区，无生命周期问题。", 0, DARK, False),
    (" 收益②：不需要为消息本身加互斥锁 —— 队列内部用调度锁保证线程安全。", 0, DARK, False),
    (" 收益③：天然解耦，生产者和消费者编译期只依赖同一个结构体定义。", 0, DARK, False),
    (" 代价：大结构体拷贝有开销 → 本项目单个消息最大是 event_log_record_t（几十字节）。", 0, ORANGE, False),
    (" 队列满的策略：input 用 20ms 超时后丢弃 + 上报诊断，绝不挂死采集任务。", 0, ACCENT2, False),
], note="如果消息很大（如整帧图像），应改传指针 + 内存池；本项目消息都很小，值拷贝是最优解。",
   note_h=0.8, size=13.5)

bullet_slide("技术", "原理③：SPI（RC522）与软 I2C（OLED/DS3231）", [
    (" SPI1 硬件外设 @4.5MHz（72MHz/16），模式 0（CPOL=0, CPHA=0），MSB 在前。", 0, DARK, False),
    (" RC522 寄存器访问：地址字节 = (reg<<1)&0x7E，读要置 bit7；CS 用软件 PA4 控制。", 0, DARK, False),
    (" 软 I2C（PB8/PB9 开漏 + 上拉）：用 GPIO 翻转模拟 START/STOP/ACK，延时用 DWT 微秒级。", 0, DARK, False),
    (" 为什么软 I2C：硬件 I2C1 与引脚分配冲突，且 STM32F1 硬件 I2C 有已知缺陷。", 0, SUB, False),
    (" ★ 软 I2C 的致命点：一次事务由几十次 GPIO 翻转组成，中途被抢占就会撕裂 → 必须加锁。", 0, WARN, True),
    (" RC522 峰值 ~100mA、ESP8266 峰值 300mA+：共用 3V3 时要就近并 100µF+0.1µF 退耦。", 0, WARN, False),
], note="两条总线的共同教训：只要是“多位/多字节、有状态、不能被打断”的时序，"
        "就必须保证事务的原子性 —— 硬件层靠关中断，RTOS 层靠互斥量。", note_h=0.9, size=13)

bullet_slide("技术", "原理④：CRC32 / HMAC-SHA1 / TOTP 三件套", [
    (" CRC32：多项式 0xEDB88320，初值 0xFFFFFFFF，结果取反。用途是**检错**不是防篡改。", 0, DARK, False),
    (" 实现用 mask = -(crc&1) 代替 if，无分支 → 常数时间，也更快。", 0, DARK, False),
    (" HMAC-SHA1：把密钥与消息两次哈希（ipad/opad），输出 20 字节；用于凭据标签与 OneNET 签名。", 0, DARK, False),
    (" 凭据不存明文：存 HMAC(device_secret, PIN) 的标签，比对用常数时间比较。", 0, ACCENT2, True),
    (" TOTP = HOTP(secret, unix_time/30)，动态截断取 6 位；容差窗 ±1 步（±30s）。", 0, DARK, False),
    (" 出厂种子 12345678901234567890（RFC6238 附录 B 测试向量），可用 Google Authenticator 验证。", 0, SUB, False),
    (" ★ 三者分工：CRC 保“数据没写坏”，HMAC 保“凭据不是明文”，TOTP 保“口令一次性”。", 0, ACCENT2, True),
], note="注意 CRC 不是安全机制（无密钥，可伪造）。真正的防篡改要靠 HMAC/签名；"
        "本工程 CRC 只用来挡“断电写一半”的物理损坏。", note_h=0.9, size=12.5)

bullet_slide("技术", "原理⑤：W25Q64 Flash 的擦写特性", [
    (" Flash 特性：只能 1→0，写之前必须先擦除（擦除是把整块变回全 1）。", 0, DARK, True),
    (" 最小擦除单位 = 4KiB 扇区；所以 config 一个槽就是 4KiB，一条日志也是一个扇区。", 0, DARK, False),
    (" 擦写寿命约 10 万次：本工程日志是环形 128 扇区 → 均摊后寿命足够，但要避免热点扇区。", 0, ORANGE, False),
    (" 写流程：写使能 → 扇区擦除（等待忙）→ 页编程（≤256B）→ 等待忙 → 读回校验。", 0, DARK, False),
    (" 掉电风险：擦完还没写、或写了一半 → 靠 magic + CRC 判定为无效记录并丢弃。", 0, WARN, False),
    (" A/B 双槽正是为此设计：永远写“非活动”槽，旧槽在写成功前保持可读可用。", 0, ACCENT2, True),
], note="本项目把“掉电安全”做成了存储层的默认属性，而不是让每个调用方自己处理 —— "
        "这是 config_store/event_log 存在的最大意义。", note_h=0.85, size=13)

bullet_slide("技术", "原理⑥：串口环形缓冲 / 看门狗 / 低功耗", [
    (" 串口接收用环形缓冲（SYSTEM/usart.c）：head/tail 双指针，ISR 只写入缓冲，", 0, DARK, False),
    (" 任务侧再取走解析 —— ★ ISR 绝不碰 RTOS 对象，也不做 printf 这类慢操作。", 0, ACCENT2, True),
    (" 取模实现环形：tail = (tail + 1) % size；满/空靠 head 与 tail 的相对位置判断。", 0, DARK, False),
    (" 独立看门狗 IWDG：main 启动失败死循环等它复位；正常运行时 board_watchdog_refresh() 喂狗。", 0, DARK, False),
    (" 栈溢出/断言失败会进 for(;;) 死循环 → 正是靠 IWDG 把设备拉回复位态。", 0, WARN, False),
    (" 低功耗 STOP：SMART_LOCK_ENABLE_STOP_MODE 默认 0，configUSE_TICKLESS_IDLE=0。", 0, DARK, False),
    (" 原因：STOP 会停时钟与外设，唤醒后需恢复时钟/DMA；先验证唤醒链路再开，避免“能进不能出”。", 0, ORANGE, False),
    (" 唤醒源：PA8 EXTI8；ISR 只置 volatile 标志，真正的唤醒动作在 input 任务里做。", 0, DARK, False),
], note="工程取舍：低功耗是“锦上添花”，正确性优先 —— 唤醒链路没验证前，宁可不进 STOP。",
   note_h=0.8, size=12.5)

# =====================================================================
# 第 4 部分：优缺点 / 边界 / Bug 风险
# =====================================================================
s = add_slide(); bg(s, DARK)
rect(s, 0, Inches(3.0), SW, Inches(1.4), color=WARN)
tb, tf = textbox(s, Inches(0.9), Inches(3.05), SW-Inches(1.8), Inches(1.3),
                 anchor=MSO_ANCHOR.MIDDLE)
add_para(tf, "第 4 部分", 16, RGBColor(0xFF, 0xD8, 0xD8), space_after=4)
add_para(tf, "优缺点、边界情况、Bug 风险", 32, HEAD_FG, bold=True, space_after=0)

bullet_slide("取舍", "优点：这套架构做对了什么", [
    (" 分层干净：app 层纯 C 可 PC 单测，认证/锁控/凭据这些“最该正确”的代码有断言覆盖。", 0, ACCENT2, False),
    (" 决策唯一：access_task 单点仲裁 + 队列串行化，从架构上消除了双判竞态。", 0, ACCENT2, True),
    (" 全静态分配：无堆碎片、无分配失败，20KB RAM 下确定性运行。", 0, ACCENT2, False),
    (" 安全内建：常数时间比较、虚拟前后缀胁迫、双因子、失败锁定、CRC 掉电保护。", 0, ACCENT2, False),
    (" 故障自愈：RC522 两级自愈、I2C 事务锁、看门狗，让“玄学问题”变成可诊断问题。", 0, ACCENT2, False),
    (" 可观测：启动日志 [OK] MFRC522 / [RTC] DS3231 online / [BOOT] ready 一眼定位。", 0, ACCENT2, False),
    (" 决策与执行分离：lock_controller 只改状态，set_physical_state 才动硬件，便于测试与审计。", 0, ACCENT2, False),
], note="最值得借鉴的一点：把“正确性”和“健壮性”分层处理，再用状态机与队列在接口处串起来。",
   note_h=0.8, size=13)

bullet_slide("取舍", "缺点与局限：坦诚面对", [
    (" ROM 余量 <4KB：再加功能（如更多汉字、TLS）会直接撑爆，必须先砍字库。", 0, WARN, True),
    (" 无 NTP 校时：时钟完全依赖 DS3231 + VBAT 电池；电池没电 → TOTP 失效。", 0, WARN, True),
    (" 无安全存储：device_secret 明文存在 W25Q64，能读 Flash 就能离线算标签（无加密芯片）。", 0, WARN, True),
    (" 指纹存模块：AS608 独立存储，擦 MCU Flash 清不掉；模块本身也是攻击面。", 0, ORANGE, False),
    (" MQTT 走明文（无 TLS）：局域网/公网可被窃听与重放（token 有时效但报文不加密）。", 0, ORANGE, False),
    (" 单核无 MMU/MPU：任一任务跑飞（栈溢出/野指针）都能拖垮整个系统。", 0, ORANGE, False),
    (" 队列满会丢事件：input_queue 满时丢弃并只上报诊断，不会背压。", 0, SUB, False),
], note="这些不是 bug，是“资源与成本约束下的取舍”。讲清楚取舍，比假装完美更有价值。",
   note_h=0.8, size=12.5)

bullet_slide("边界", "边界情况清单（必须想过的场景）", [
    (" 断电时机：擦 Flash 中 / 写一半 / 刚开门 → magic+CRC 判无效；A/B 槽保配置。", 0, DARK, False),
    (" 时钟：DS3231 掉电无电池 → 跳回编译时刻 → TOTP 全部失效（只能靠 VBAT）。", 0, WARN, True),
    (" 时钟跳变：菜单改时间后，已解锁的 deadline 与失败锁定期会跟着变（按绝对时间算）。", 0, ORANGE, False),
    (" 门未关：door_closed=false 时不自动落锁（避免夹人），但会一直等 → 需要门未关提醒。", 0, DARK, False),
    (" 刷卡不放：CARD_DUPLICATE_MS=3000 去重窗口内不重复开锁。", 0, DARK, False),
    (" 队列满/任务阻塞：input 丢事件 + 诊断；ui/日志延迟但不影响开锁（优先级低）。", 0, DARK, False),
    (" Flash 写失败：event_log_append 返回 false → 上报 BOARD_DIAG_LOG_STORAGE_FAILURE。", 0, DARK, False),
    (" 访客码消耗后未落盘成功：必须 save 成功才 grant，否则重启后一次性码可重用。", 0, ACCENT2, True),
], note="边界清单的价值：把“没想过”变成“想过并选择了处理方式”。", note_h=0.75, size=12.5)

bullet_slide("踩坑", "Bug 风险①：栈溢出 = 静默死机", [
    (" 现象：串口输出突然停在某行中间，xTickCount 冻结，看门狗才复位。", 0, WARN, True),
    (" 根因：vApplicationStackOverflowHook 就是 taskDISABLE_INTERRUPTS + for(;;)，**不打印**。", 0, DARK, False),
    (" 历史：network_task 栈曾只有 320 字（42%），esp8266_mqtt_connect 局部 392B → 必溢出。", 0, DARK, False),
    (" 现栈：input 256 / access 384 / network 768 / log 256 / ui 160 字。", 0, DARK, False),
    (" 定位法：map 查 *_task_stack 地址 → ST-Link 热插拔读栈底 16 字节；", 0, ACCENT2, False),
    (" FreeRTOS 用 0xA5A5A5A5 填充未用栈，被改写即溢出（内容常是返回地址 → map 反查函数）。", 0, ACCENT2, False),
    (" STM32_Programmer_CLI -c port=SWD mode=hotplug -r32 <栈底地址> 16", 0, SUB, False),
    (" 黄金法则：RAM 吃紧先关 configUSE_TIMERS，别去压别的任务栈。", 0, ACCENT2, True),
], note="栈溢出是本项目最难查的一类故障，因为它“看起来像死机、其实是在钩子里的死循环”。",
   note_h=0.8, size=12.5)

bullet_slide("踩坑", "Bug 风险②：I2C 总线并发 → 屏幕错乱", [
    (" 现象：OLED 文字错乱/花屏，但指纹与密码完全正常（因为它们不碰 I2C）。", 0, WARN, True),
    (" 根因：OLED(0x78) 与 DS3231(0xD0) 共用软 I2C PB8/PB9，被不同优先级任务访问。", 0, DARK, False),
    (" access(4) 读 DS3231、ui(1) 刷整屏、input(3) 键盘回显 → 高优先级在事务中途抢断低优先级。", 0, DARK, False),
    (" 结果：两条事务的 START/数据/STOP 交织，从机把半个时钟周期当数据收下。", 0, DARK, False),
    (" 老修法失效原因：只在 oled.c 加 s_oled_mtx，**挡不住 DS3231 的访问**。", 0, DARK, False),
    (" 正确修法：锁下沉到“总线事务层”（IIC_WriteBytesRaw/ReadRegs/WriteReg），用递归锁。", 0, ACCENT2, True),
    (" 通用结论：共享总线上挂多器件时，互斥必须加在总线层，而不是某个器件的驱动层。", 0, ACCENT2, True),
], note="这个坑的隐蔽性在于：切到真实 DS3231 之前（软时钟不碰总线）它根本不出现。",
   note_h=0.8, size=12.5)

bullet_slide("踩坑", "Bug 风险③：RC522 棕色复位 / ESP8266 供电", [
    (" 现象：卡刚开始能读，用一阵彻底没反应（指纹密码正常）。", 0, WARN, True),
    (" 错误判据：读 VersionReg(0x37)。它是固定硅片 ID（本板恒 0x92），", 0, DARK, False),
    (" 芯片哪怕掉电复位、配置全丢，它照样读得回 → 老逻辑永远走“还活着”分支。", 0, DARK, False),
    (" 真实诱因：ESP8266 发射峰值 300mA+ 与 RC522 共用 3V3 → RC522 棕色复位丢配置。", 0, DARK, False),
    (" 正确判据：MFRC522_Check() 回读 Init 亲手写下的 TModeReg=0x8D / TPrescalerReg=0x3E。", 0, ACCENT2, True),
    (" 两级自愈：配置丢了立刻整片 Init；即便正常也每 6 次（≈30s）无条件 Init 兜底。", 0, ACCENT2, False),
    (" 硬件侧：RC522 VCC-GND 就近并 100µF+0.1µF；最好给 RC522 与 ESP8266 分开供电路径。", 0, ORANGE, False),
    (" ESP8266 另案：连不上网多为模块固件崩溃死循环（非 MCU 责任），重刷 AT 或换模块。", 0, SUB, False),
], note="★ 通用方法论：永远不要用“只读的固定 ID 寄存器”当健康探针 —— 要回读你亲手写下去的配置。",
   note_h=0.8, size=12)

bullet_slide("边界", "Bug 风险④：其它值得警惕的点", [
    (" TOTP 依赖时钟：偏移 >30s 即失效；软时钟回退到编译时刻后必然全部失败。", 0, WARN, False),
    (" 中文源码必须 UTF-8 **带 BOM**：AC5 按 CP936 解析，缺 BOM 会报 #8: missing closing quote。", 0, WARN, True),
    (" 注释中文化陷阱：以 * 开头的代码行（*ptr = ...）会被误判成注释，切勿批量替换。", 0, WARN, True),
    (" 事件枚举持久化：event_type 按 uint8_t 写盘，新枚举只能追加末尾，不能重排。", 0, ORANGE, False),
    (" 菜单期间读头让路：此时刷卡不会开锁（设计如此），要讲清楚避免误判为 bug。", 0, SUB, False),
    (" 字库缺字：OLED_ShowText 无码点时画空心方框；字库由 tools/gen_oled_font.py 维护。", 0, SUB, False),
    (" 烧录后 ESP8266 不复位：-rst 只复位 MCU，模组要单独断电或 AT+RST。", 0, SUB, False),
], note="其中“中文 BOM”和“注释替换误伤代码行”是本项目真实踩过/差点踩到的坑，务必记住。",
   note_h=0.8, size=12.5)

# =====================================================================
# 第 5 部分：流程总结
# =====================================================================
s = add_slide(); bg(s, DARK)
rect(s, 0, Inches(3.0), SW, Inches(1.4), color=ORANGE)
tb, tf = textbox(s, Inches(0.9), Inches(3.05), SW-Inches(1.8), Inches(1.3),
                 anchor=MSO_ANCHOR.MIDDLE)
add_para(tf, "第 5 部分", 16, RGBColor(0xFF, 0xF0, 0xD8), space_after=4)
add_para(tf, "项目流程总结", 32, HEAD_FG, bold=True, space_after=0)

code_slide("流程", "一次“密码开锁”的端到端时序（完整链路）", r'''[用户] 键盘输入 123456
   │  keypad_scan_step() 消抖 → keypad_input_feed()
   ▼
[input_task] 产出 board_input_event{BOARD_AUTH_PIN, digits}
   │  xQueueSend(input_queue, 20ms 超时)
   ▼
[access_task] xQueueReceive(input_queue) → handle_board_input()
   │  ① keypad_is_locked?  ② 胁迫码匹配?  ③ 业主码匹配(常数时间比较 HMAC 标签)?
   │  ④ 双因子是否齐?      ⑤ 访客码?      ⑥ 否则 deny + 失败计数
   ▼ 命中
lock_controller_handle(AUTH_GRANTED) → state=UNLOCKED, deadline=now+N秒
   ▼
grant_access()
   ├─ set_physical_state()      → board_lock_set(false) → 舵机转 90° 解锁
   ├─ board_auth_feedback(true) → 蜂鸣 + 绿灯
   ├─ queue_ui("Access granted") ──ui_queue──▶ [ui_task] 刷 OLED
   └─ queue_audit(UNLOCK)       ──log_queue──▶ [log_task] 写 W25Q64(CRC32)
   ▼
[门磁] BOARD_DOOR_OPENED → door_closed=false
[门磁] BOARD_DOOR_CLOSED → 立即落锁 + 审计
[兜底] TICK 且 now≥deadline 且门已关 → 自动 LOCKED''',
    note="三路并行（物理/界面/审计）全部异步：access 任务不阻塞在 I2C(慢) 或 Flash(更慢) 上，"
         "这是“刷卡立刻有反应”的关键。",
    size=10, h=5.2, note_h=0.65)

steps_slide("流程", "把整条链路串起来：从需求到可运行", [
    ("需求：本地多方式开锁 + 远程 + 审计 + 胁迫告警（金融级）", ACCENT),
    ("约束：64KB Flash / 20KB RAM → 全静态分配 + 纯 C + 精简字库", WARN),
    ("架构：app(纯逻辑) / stm32(适配) / HARDWARE(驱动) / FreeRTOS(内核) 四层", ACCENT),
    ("启动：main → app_start(建 6 队列 + 5 任务) → board_init 9 步", ACCENT2),
    ("采集：input_task 10ms 扫键盘/卡/指纹 → 统一事件 → input_queue", ACCENT2),
    ("决策：access_task 单点仲裁（handle_board_input + lock_controller 状态机）", ACCENT),
    ("凭据：HMAC 标签 + 常数时间比较 + 虚拟前后缀胁迫 + TOTP + 失败锁定", WARN),
    ("执行：set_physical_state 动硬件；queue_ui / queue_audit 异步分发", ACCENT2),
    ("持久化：log_task 写 W25Q64 环形日志（magic+CRC 挡坏记录）", PURPLE),
    ("健壮：I2C 事务锁 / RC522 两级自愈 / 看门狗 / 栈溢出可定位", PURPLE),
], note="这条链上每一环都有“为什么”和“踩过什么坑”，讲解时按此顺序推进最顺。",
   note_h=0.75, top=1.35)

table_slide("讲解", "40–60 分钟讲解节奏分配（可直接照着讲）", [
    ("段落", "页码主题", "时长", "讲法重点"),
    ("开场 + 架构", "封面 / 路线图 / 约束 / 四层 / 分工 / 引脚 / 任务队列", "8 min", "先讲约束，再讲架构为何如此"),
    ("启动链", "main / app_start / board_init 9 步 / FreeRTOSConfig / hooks", "7 min", "逐行念签名，讲“为什么这个顺序”"),
    ("任务与队列", "input / access / network / log / ui 五个任务逐行", "8 min", "强调让出、优先级、唯一决策点"),
    ("输入采集", "board_poll_input / read_card_uid / heal / MFRC522_Init", "7 min", "三个真实 bug 与优化点"),
    ("认证与凭据", "handle_board_input / PIN 分支 / 状态机 / grant-deny", "8 min", "安全优先级顺序、常数时间"),
    ("动态口令", "HOTP 动态截断 / TOTP 容差窗与下溢防护", "5 min", "RFC4226/6238 原理 + 逐行"),
    ("持久化与原理", "config A/B 信封 / CRC32 环形日志 / 技术点合集", "7 min", "掉电安全、CRC 与 HMAC 分工"),
    ("风险与收尾", "优缺点 / 边界 / 栈溢出 / I2C / RC522 / 时序 / 总结", "8 min", "踩坑复盘最有说服力"),
], [Inches(2.2), Inches(6.2), Inches(1.2), Inches(4.4)],
    note="合计约 58 分钟；时间紧可压缩“原理”段到 4 min、合并任务段，仍能保 40+ 分钟。"
         "每页节奏：先念函数签名 → 再讲为什么这么写 → 最后点踩过的坑。",
    rh=0.44, note_h=0.95, size=11)

bullet_slide("总结", "本课要点回顾（10 句话）", [
    ("1. 约束决定架构：20KB RAM 逼出“全静态分配 + 纯 C + 4KB 堆”。", 0, DARK, False),
    ("2. 四层解耦：app 纯逻辑可单测，board 管适配，HARDWARE 管驱动，RTOS 管并发。", 0, DARK, False),
    ("3. 5 任务 6 队列：采集/决策/通信/持久/呈现各司其职，access 是唯一决策中心。", 0, DARK, False),
    ("4. 队列不是装饰：值拷贝 + 异步 + 保优先级 + 削峰 + 消除双判竞态。", 0, ACCENT2, True),
    ("5. 让出≠忙等：vTaskDelay 让出 CPU，所以 RC522 自愈 13ms 不卡系统。", 0, ACCENT2, True),
    ("6. 决策与执行分离：状态机只改状态，set_physical_state 才动硬件。", 0, DARK, False),
    ("7. 安全内建：常数时间比较、虚拟前后缀胁迫、双因子、失败锁定。", 0, WARN, False),
    ("8. 掉电安全：A/B 双槽 + 序号 + CRC32，写一半断电不丢配置。", 0, DARK, False),
    ("9. 健康探针要回读“自己写下的配置”，不是读固定 ID —— RC522 的教训。", 0, WARN, True),
    ("10. 互斥加在总线事务层，不是器件驱动层 —— I2C 花屏的教训。", 0, WARN, True),
], note="带走的方法论比记住 API 更重要：先定责任边界，再用寄存器回读 + 日志定位，别凭感觉改代码。",
   note_h=0.85, size=12.5)

bullet_slide("交互", "管理菜单：待机按 'D' 进入", [
    (" 入口：先验主密码；管理员指纹(48 页) 或管理卡开锁后 30s 内按 'D' 免验直达。", 0, DARK, False),
    (" 按键：菜单页 A/C 移动、D 进入、B 返回；输入页 数字/# 确认、B 删位、C 退出。", 0, DARK, False),
    (" 菜单树：密码管理(主/胁迫) · 指纹(录 1~47 / 删 1~49 / 管理员 48) · 卡片(角色)", 0, DARK, False),
    ("         胁迫(49 页) · 系统(改时间 YYMMDDHHMMSS / 查看记录 / 组合模式)。", 0, DARK, False),
    (" 状态机跑在 access 任务里（menu_poll ≈100ms），渲染经 ui 队列 → board_menu_show()。", 0, DARK, False),
    (" 中文文案集中在 board_port.c；改主密码是“先落盘再报成功”，避免断电后密码丢失。", 0, ACCENT2, True),
    (" 菜单激活期间：不喂键盘扫描给认证、不 queue_ui、不进 STOP —— 保证按键实时响应。", 0, DARK, False),
], note="菜单是本项目最大的一块交互状态机，也是 ROM 占用大户（中文文案 + 多页渲染）。",
   note_h=0.8, size=12.5)

bullet_slide("边界", "组合认证与特殊凭据（访客 / 周期 / 胁迫）", [
    (" 访客码（一次性）：visitor_credential_verify_and_consume() 校验并**立即消耗**，", 0, DARK, False),
    ("   ★ 必须 board_security_save() 落盘成功才 grant，否则重启后这码还能再用一次。", 0, WARN, True),
    (" 周期口令：periodic_credential 带“星期位图 + 本地分钟窗口”，只在允许时段内有效。", 0, DARK, False),
    ("   位 0 = 周一，位 6 = 周日；时间为本地零点后的分钟数（注意时区 +8）。", 0, DARK, False),
    (" 胁迫码 / 胁迫指纹：门照开，但审计记录为胁迫事件并后台告警（表面无异常）。", 0, WARN, False),
    (" 双因子（FINGERPRINT_PIN / RFID_PIN）：先记第一因子 + 超时窗口，再输 PIN 才放行。", 0, DARK, False),
    ("   命中第二因子后 clear_pending_factor()，防止凭据被复用。", 0, DARK, False),
], note="设计原则：所有特殊凭据都在同一套决策入口内实现，权限与锁定策略完全一致，没有旁路。",
   note_h=0.8, size=12.5)

table_slide("导航", "关键文件 → 职责 速查表（照着这张表读源码）", [
    ("文件", "职责", "优先级"),
    ("firmware/stm32/src/main.c", "入口：smart_lock_app_start → vTaskStartScheduler", "必读"),
    ("firmware/stm32/src/smart_lock_app.c", "任务/队列、handle_board_input、grant/deny、菜单", "★ 核心"),
    ("firmware/stm32/src/board_port.c", "board_init 9 步、读卡、键盘、菜单渲染、总线", "★ 核心"),
    ("firmware/app/src/lock_controller.c", "4 态锁控状态机（纯逻辑，可单测）", "★ 核心"),
    ("firmware/app/src/lock_auth.c", "常数时间比较、PIN 虚拟前后缀校验、失败锁定", "★ 核心"),
    ("firmware/app/src/totp.c", "HOTP 动态截断 + TOTP 容差窗校验", "必读"),
    ("firmware/app/src/config_store.c · event_log.c", "A/B 双槽 CRC32 掉电安全信封；CRC32 环形审计日志 + 序号回绕比较", "必读"),
    ("firmware/HARDWARE/i2c_soft.c", "软 I2C + 事务级递归锁（防花屏关键）", "★ 易错"),
    ("firmware/HARDWARE/mfrc522.c", "RC522 Init/Check/自愈（防失灵关键）", "★ 易错"),
    ("firmware/USER/app_wifi.c · tests/test_main.c", "ESP8266 联网状态机 + OneNET token；PC 端单元测试", "了解"),
], [Inches(4.4), Inches(6.4), Inches(1.6)],
    note="建议阅读顺序：main → smart_lock_app → lock_controller → lock_auth → 再回头看驱动。",
    rh=0.4, note_h=0.6, size=10.5)

code_slide("工程", "构建 / 烧录 / 调试（实测命令，可直接复用）", r'''# 无头编译（UV4 是 GUI 子系统，须 GUI 子 shell；一律用 -r 强制 Rebuild）
$p = Start-Process -FilePath 'D:\Keil_v5\UV4\UV4.exe' `
        -ArgumentList '-r','SmartLock.uvprojx','-j0','-o','build.log' -Wait -PassThru
$p.ExitCode        # 0=成功；日志里出现 "Program Size:" 才说明真重编了

# 烧录（SWD；-rst 烧完即跑。注意 ESP8266 不随 MCU 复位）
STM32_Programmer_CLI.exe -c port=SWD -w obj\SmartLock.hex -v -rst

# 调试串口 COM7 @115200 8N1（printf 走 USART3 PB10/PB11）
# 实测体积：Code=53388 RO=8104 RW=408 ZI=18208，0err/0warn

# ★ 热插拔读实时寄存器（不复位、不打断运行，读到的是真实值）
STM32_Programmer_CLI.exe -c port=SWD mode=hotplug -r32 <addr> <len>
#   常用地址：RCC 0x40021000(+0x18 APB2ENR)  GPIOA 0x40010800(CRL/CRH/IDR/ODR)
#             AFIO 0x40010000(+0x04 MAPR)    USART1 0x40013800  USART3 0x40004800

# ★ 中文源码必须 UTF-8 带 BOM（AC5 按 CP936 解析，缺 BOM 报 #8: missing closing quote）
python tools/fix_source_bom.py --check''',
    note="ROM 余量极小（<4KB）：省空间先砍字库，绝不砍安全逻辑。"
         "栈溢出表现为“静默死机”——输出停在某行中间、xTickCount 冻结。",
    size=10, h=5.0, note_h=0.75)

bullet_slide("实测", "上板自查清单（讲解完当场跑一遍最有说服力）", [
    (" [ ] 启动日志出现 [OK] MFRC522 RFID (Version 0x92) 与 [RTC] DS3231 online <UTC>。", 0, DARK, False),
    (" [ ] [CARD] N card(s) loaded 与 [BOOT] smart lock board ready。", 0, DARK, False),
    (" [ ] 密码 123456 开锁；连续输错 3 次后 60s 内被锁（Try again later）。", 0, DARK, False),
    (" [ ] 胁迫码 654321 能开门，但后台记录为胁迫事件（表面无异常提示）。", 0, WARN, False),
    (" [ ] TOTP（Google Authenticator，种子 12345678901234567890）一次性开锁成功。", 0, DARK, False),
    (" [ ] 卡长按 5s 以上屏幕无花屏（I2C 事务锁生效）。", 0, ACCENT2, True),
    (" [ ] 连续读卡 10 分钟仍响应（RC522 两级自愈生效；日志偶见 heal: ... re-init）。", 0, ACCENT2, True),
    (" [ ] 门关闭后自动落锁；开门不放时超时也自动落锁。", 0, DARK, False),
    (" [ ] 待机按 D → 验主密码 → 进菜单改时间，重启后时间仍正确（DS3231 + VBAT）。", 0, DARK, False),
], note="把这张清单当“验收用例”：讲完架构与代码，当场跑通一遍，说服力最强。",
   note_h=0.8, size=12.5)

# ---- Q&A ----
s = add_slide()
bg(s, DARK)
rect(s, 0, Inches(2.85), SW, Inches(1.7), color=ACCENT)
tb, tf = textbox(s, Inches(0.9), Inches(2.9), SW-Inches(1.8), Inches(1.6),
                 anchor=MSO_ANCHOR.MIDDLE)
add_para(tf, "Q & A · 谢谢观看", 30, HEAD_FG, bold=True, space_after=6)
add_para(tf, "深度代码讲解到此结束，欢迎就任一函数实现或踩坑细节提问。", 15,
         RGBColor(0xCF, 0xE6, 0xFF), space_after=0)
tb2, tf2 = textbox(s, Inches(0.9), Inches(5.0), SW-Inches(1.8), Inches(1.4))
add_para(tf2, "配套：docs/code_walkthrough.html（交互版图解）· docs/task_queue_architecture.png（任务-队列架构图）",
         12, RGBColor(0x9F, 0xB3, 0xC8), space_after=4)
add_para(tf2, "构建：UV4 -r SmartLock.uvprojx -j0    烧录：STM32_Programmer_CLI -c port=SWD -w SmartLock.hex -v -rst",
         11, RGBColor(0x9F, 0xB3, 0xC8), space_after=0)

# ---------------- 页脚 + 保存 ----------------
total = len(prs.slides._sldIdLst)
for i, sl in enumerate(CONTENT, start=2):
    footer(sl, i, total)

out = os.path.abspath(PPTX_PATH)
os.makedirs(os.path.dirname(out), exist_ok=True)
prs.save(out)
print("SAVED", out, "slides=", total)
