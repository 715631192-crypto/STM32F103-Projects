# -*- coding: utf-8 -*-
"""简化固件：删除开发/调试脚手架（不改变任何业务逻辑）。

删除内容：
  1. servo.h / servo.c          —— 上电舵机自检 SERVO_SELFTEST（开机舵机转动）
  2. board_port.c               —— esp8266_link_probe() 及其调用（排查 WiFi）
  3. board_port.c               —— 一次性恢复出厂块（开发用）
  4. board_port.c / mfrc522.*   —— RC522 MISO 探针（排查读卡线缆）
  5. smart_lock_board_config.h  —— 上述开关的宏定义与说明
保留：开机串口自检日志（[OK] MFRC522 / [RTC] / [BOOT]）与 [DIAG] 运行时诊断。
"""
import os

root = "C:/Users/71563/Desktop/block/"
removed = []


def load(rel):
    with open(root + rel, encoding="utf-8") as f:
        return f.read()


def save(rel, s):
    with open(root + rel, "w", encoding="utf-8", newline="") as f:
        f.write(s)


# ---------------- smart_lock_board_config.h ----------------
rel = "firmware/stm32/include/smart_lock_board_config.h"
s = load(rel)
n0 = s.count("\n")

old_a = """/* MFRC522 读卡作为便捷因子，不作为高安全因子 */
#define SMART_LOCK_RC522_POLL_DEBUG             0
/* SG90 自检：上电扫 0°→90°→0°，正式部署必须置 0 */
#define SMART_LOCK_SERVO_SELFTEST               0
/* ⚠ 一次性恢复出厂开关（2026-09-14）：置 1 时每次上电都擦 W25Q64 的
 * 配置 A/B 槽 + 卡片表，开机即回出厂（主密码 123456 / 胁迫 654321 /
 * 开锁组合=单因子）。用法：烧擦除版 → 上电一次看串口 [FACTORY] →
 * 立刻改回 0 重新编译烧回正常版。平时必须为 0。
 * ★ 2026-09-14 18:xx 已执行过一次恢复出厂（串口确认 [FACTORY] +
 *   [CFG] provisioned factory defaults），现改回 0。 */
#define SMART_LOCK_FACTORY_RESET_ON_BOOT        0

"""
assert s.count(old_a) == 1, "config: factory/debug macro block not found uniquely"
s = s.replace(old_a, "", 1)
removed.append("config: RC522_POLL_DEBUG / SERVO_SELFTEST / FACTORY_RESET_ON_BOOT 宏")

a = s.index("/* ESP8266 链路自检：")
m = "#define SMART_LOCK_ESP8266_LINK_PROBE           0\n"
b = s.index(m, a) + len(m)
if s[b:b + 1] == "\n":
    b += 1
s = s[:a] + s[b:]
removed.append("config: SMART_LOCK_ESP8266_LINK_PROBE 宏 + 大段说明")

save(rel, s)
print(f"{rel}: lines {n0} -> {s.count(chr(10))}")

# ---------------- board_port.c ----------------
rel = "firmware/stm32/src/board_port.c"
s = load(rel)
n0 = s.count("\n")

# 1) 探针函数体
a = s.index("#if SMART_LOCK_ESP8266_LINK_PROBE")
end1 = "#endif /* SMART_LOCK_ESP8266_LINK_PROBE */\n"
b = s.index(end1, a) + len(end1)
s = s[:a] + s[b:]
removed.append("board_port.c: esp8266_link_probe() 函数体(~310 行)")

# 2) 探针调用点
a = s.index("#if SMART_LOCK_ESP8266_LINK_PROBE")
b = s.index("#endif\n", a) + len("#endif\n")
s = s[:a] + s[b:]
removed.append("board_port.c: esp8266_link_probe() 调用点")

# 3) 一次性恢复出厂块
a = s.index("#if SMART_LOCK_FACTORY_RESET_ON_BOOT")
b = s.index("#endif\n", a) + len("#endif\n")
s = s[:a] + s[b:]
removed.append("board_port.c: 一次性恢复出厂块")

# 4) RC522 MISO 探针打印 -> 只保留初始化
old4 = """    /* 5) 射频读卡：ProbeMiso 必须在 Init 之前（它会临时把 PA4/PA6 配成 GPIO） */
    {
        const uint8_t miso = MFRC522_ProbeMiso();
        if (miso != MFRC522_MISO_OK) {
            /* 非 OK 说明 MISO 这条线可能没接好/没供电。仅打印诊断后照常初始化，
             * 不在这里 return —— 有些情况（模块刚上电）下一步初始化就能拉回来。 */
            printf("[RC522] MISO probe=%u (0=OK 1=LOW 2=HIGH 3=NODRIVE 4=DRIVEN)\\r\\n",
                   (unsigned)miso);
        }

        MFRC522_Init();"""
new4 = """    /* 5) 射频读卡：初始化 */
    {
        MFRC522_Init();"""
assert old4 in s, "board_port.c: RC522 chunk not found"
s = s.replace(old4, new4, 1)
removed.append("board_port.c: RC522 MISO 探针打印")

save(rel, s)
print(f"{rel}: lines {n0} -> {s.count(chr(10))}")

# ---------------- mfrc522.c ----------------
rel = "firmware/HARDWARE/mfrc522.c"
s = load(rel)
n0 = s.count("\n")
a = s.index("/**\n * @brief 探测 PA6 当前被外部拉到了什么电平")
endm = "    return MFRC522_MISO_NODRIVE;\n}\n"
b = s.index(endm, a) + len(endm)
s = s[:a] + s[b:]
save(rel, s)
removed.append("mfrc522.c: rc522_probe_pin() + MFRC522_ProbeMiso()")
print(f"{rel}: lines {n0} -> {s.count(chr(10))}")

# ---------------- mfrc522.h ----------------
rel = "firmware/HARDWARE/mfrc522.h"
s = load(rel)
n0 = s.count("\n")
a = s.index("/* ---- MISO 线缆自检（须在 MFRC522_Init() 之前调用）----")
m = "uint8_t MFRC522_ProbeMiso(void);\n"
b = s.index(m, a) + len(m)
if s[b:b + 1] == "\n":
    b += 1
s = s[:a] + s[b:]
save(rel, s)
removed.append("mfrc522.h: MFRC522_MISO_* 宏 + MFRC522_ProbeMiso() 声明")
print(f"{rel}: lines {n0} -> {s.count(chr(10))}")

print("\n== removed ==")
for r in removed:
    print("  -", r)

print("\n== leftover scan (firmware/) ==")
syms = ["SERVO_SELFTEST", "MFRC522_ProbeMiso", "rc522_probe_pin", "MFRC522_MISO",
        "SMART_LOCK_ESP8266_LINK_PROBE", "SMART_LOCK_FACTORY_RESET_ON_BOOT",
        "SMART_LOCK_RC522_POLL_DEBUG", "SMART_LOCK_SERVO_SELFTEST",
        "esp8266_link_probe"]
hits = 0
for dp, _, fns in os.walk(root + "firmware"):
    for fn in fns:
        if fn.endswith((".c", ".h")):
            fp = os.path.join(dp, fn)
            t = open(fp, encoding="utf-8", errors="ignore").read()
            for sym in syms:
                if sym in t:
                    print("  LEFTOVER", sym, "in", os.path.relpath(fp, root))
                    hits += 1
print("leftover hits:", hits)
