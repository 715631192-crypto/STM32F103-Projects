#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""把自研代码中的**英文散文注释**翻译成中文。

原则（重要）：
  1. 只改"真正的注释散文"。绝对不碰：
     - 以 * 开头的代码行（指针解引用赋值，如 `*value_out = cursor;`）
     - 代码示例 / JSON 模板 / 协议公式 / 文件名 / 引脚表格 / 标识符清单
  2. 每条改动用 (文件, 行号, 原串, 新串) 断言式替换，
     原串不存在则立即报错中止 —— 防止改错位置。
  3. 改完统一写回 UTF-8 **带 BOM**（AC5 按 CP936 解析，无 BOM 会报
     `#8: missing closing quote`），并保留原有换行符（CRLF/LF）。

范围：app / stm32 / HARDWARE / drivers / USER / SYSTEM / tests
排除：FreeRTOS / MIDDLEWARE（第三方，保留英文原文）
"""
import io
import os
import sys

ROOT = r'C:\Users\71563\Desktop\block\firmware'

# (相对路径, 行号, 原串, 新串)
EDITS = [
    # ---------------- app/include/config_store.h：信封二进制布局 ----------------
    ('app/include/config_store.h', 28, 'offset 0  : magic', 'offset 0  : 魔数 magic'),
    ('app/include/config_store.h', 29, 'offset 4  : version', 'offset 4  : 版本号 version'),
    ('app/include/config_store.h', 30, 'offset 6  : payload_length', 'offset 6  : 有效载荷长度 payload_length'),
    ('app/include/config_store.h', 31, 'offset 8  : sequence', 'offset 8  : 序号 sequence'),
    ('app/include/config_store.h', 32, 'offset 12 : payload[payload_length]', 'offset 12 : 有效载荷 payload[payload_length]'),

    # ---------------- app/include/onenet_token.h：OneNET 令牌算法 ----------------
    ('app/include/onenet_token.h', 18, '1. key', '1. 密钥 key'),
    ('app/include/onenet_token.h', 20, '3. sign', '3. 签名 sign'),
    ('app/include/onenet_token.h', 21, '4. password', '4. 口令 password'),

    # ---------------- app/include/periodic_credential.h ----------------
    ('app/include/periodic_credential.h', 10,
     '/* Bit 0 is Monday and bit 6 is Sunday. Times are local minutes after midnight. */',
     '/* 位 0 表示周一，位 6 表示周日。时间为本地时间零点之后的分钟数。 */'),

    # ---------------- app/src/onenet_token.c ----------------
    ('app/src/onenet_token.c', 138, '/* 3. sign', '/* 3. 签名 sign'),

    # ---------------- drivers/include/esp8266_at.h：MQTT CONNECT 报文字段 ----------------
    ('drivers/include/esp8266_at.h', 29, 'Protocol Name', '协议名 Protocol Name'),
    ('drivers/include/esp8266_at.h', 30, 'Protocol Level', '协议级别 Protocol Level'),
    ('drivers/include/esp8266_at.h', 31, 'Connect Flags', '连接标志 Connect Flags'),
    ('drivers/include/esp8266_at.h', 32, 'Keep Alive', '保活时间 Keep Alive'),
    ('drivers/include/esp8266_at.h', 33, 'Client Id', '客户端 ID Client Id'),

    # ---------------- drivers/src/json_codec.c ----------------
    ('drivers/src/json_codec.c', 154, '/* reverse(start..pos) */', '/* 反转区间 reverse(start..pos) */'),

    # ---------------- HARDWARE/esp8266.c：模组版本信息 ----------------
    ('HARDWARE/esp8266.c', 17, 'AT version', 'AT 固件版本'),
    ('HARDWARE/esp8266.c', 18, 'SDK version', 'SDK 版本'),

    # ---------------- HARDWARE/esp8266.h ----------------
    ('HARDWARE/esp8266.h', 38, '/* ③ DeviceSecret (base64) */', '/* ③ 设备密钥 DeviceSecret (base64) */'),
    ('HARDWARE/esp8266.h', 70, 'door_event', '开门事件 door_event'),
    ('HARDWARE/esp8266.h', 72, 'duress_scene', '胁迫场景 duress_scene'),

    # ---------------- HARDWARE/i2c_soft.h：器件地址 ----------------
    ('HARDWARE/i2c_soft.h', 12, 'SSD1306 OLED :', 'SSD1306 OLED 地址：'),
    ('HARDWARE/i2c_soft.h', 13, 'DS3231 RTC   :', 'DS3231 RTC 地址：'),

    # ---------------- HARDWARE/mfrc522.c：引脚 + 错误寄存器位 ----------------
    ('HARDWARE/mfrc522.c', 8,
     'PA4 = NSS(CS)  PA5 = SCK  PA6 = MISO  PA7 = MOSI',
     'PA4 = 片选 NSS(CS)  PA5 = 时钟 SCK  PA6 = 主入从出 MISO  PA7 = 主出从入 MOSI'),
    ('HARDWARE/mfrc522.c', 334,
     'err   ErrorReg:  0x01=ProtocolErr 0x02=ParityErr 0x04=CRCErr',
     'err  错误寄存器 ErrorReg：0x01=协议错 ProtocolErr  0x02=奇偶错 ParityErr  0x04=CRC 错 CRCErr'),
    ('HARDWARE/mfrc522.c', 335,
     '0x08=CollErr     0x10=BufferOvfl',
     '0x08=冲突错 CollErr      0x10=缓冲区溢出 BufferOvfl'),

    # ---------------- stm32/FreeRTOSConfig.required.h ----------------
    ('stm32/FreeRTOSConfig.required.h', 4,
     '/* Merge these settings into the CubeMX-generated FreeRTOSConfig.h. */',
     '/* 将下面这些配置项合并进 CubeMX 生成的 FreeRTOSConfig.h。 */'),

    # ---------------- stm32/include/board_port.h：对外接口契约 ----------------
    ('stm32/include/board_port.h', 182,
     '/* Enters STOP and returns after wake with clocks and DMA restored. */',
     '/* 进入 STOP 低功耗模式，唤醒后恢复时钟与 DMA 再返回。 */'),
    ('stm32/include/board_port.h', 196,
     'Non-blocking input poll. Fingerprint/RFID events mean the board driver has',
     '非阻塞输入轮询。指纹 / RFID 事件表示板级驱动已经'),
    ('stm32/include/board_port.h', 197,
     'already completed a successful match against an enabled, registered user.',
     '完成了一次针对已启用且已登记用户的成功匹配。'),
    ('stm32/include/board_port.h', 201,
     '/* Non-blocking network poll. The ESP8266 transport parses JSON into this type. */',
     '/* 非阻塞网络轮询。ESP8266 传输层把 JSON 解析成该类型。 */'),
    ('stm32/include/board_port.h', 205,
     '/* Load/save must use a versioned, CRC-protected, power-loss-safe envelope. */',
     '/* 加载 / 保存必须使用带版本号、CRC 校验、掉电安全的信封结构。 */'),
    ('stm32/include/board_port.h', 280,
     '/* W25Q64 MVP mapping: one 4 KiB erase sector per log slot. */',
     '/* W25Q64 最小可行映射：每条日志占用一个 4 KiB 擦除扇区。 */'),

    # ---------------- stm32/src/board_port.c：method 枚举映射 ----------------
    ('stm32/src/board_port.c', 764,
     '0 SYSTEM  1 PIN  2 RFID  3 FINGERPRINT  4 TOTP  5 REMOTE',
     '0 系统 SYSTEM  1 密码 PIN  2 刷卡 RFID  3 指纹 FINGERPRINT  4 动态口令 TOTP  5 远程 REMOTE'),

    # ---------------- stm32/src/freertos_hooks.c：故障钩子 ----------------
    ('stm32/src/freertos_hooks.c', 36,
     '/* A debugger can inspect task_name. The watchdog resets release units. */',
     '/* 调试器可在此查看 task_name。量产机由看门狗复位恢复。 */'),
    ('stm32/src/freertos_hooks.c', 44,
     '/* Dynamic allocation is not used by the smart-lock application layer. */',
     '/* 智能门锁应用层不使用动态内存分配。 */'),

    # ---------------- stm32/src/smart_lock_app.c：安全不变量 ----------------
    ('stm32/src/smart_lock_app.c', 1703,
     '/* Never operate the actuator unless replay state is durable first. */',
     '/* 重放状态必须先持久化落盘，之后才允许驱动执行机构。 */'),

    # ---------------- tests/test_main.c ----------------
    ('tests/test_main.c', 182,
     '/* 1970-01-05 was Monday: 10:30 is inside the configured window. */',
     '/* 1970-01-05 是周一：10:30 落在已配置的时间窗内。 */'),
    ('tests/test_main.c', 345, 'base64', 'base64 编解码'),
    ('tests/test_main.c', 382, 'OneNET token', 'OneNET 鉴权令牌'),
    ('tests/test_main.c', 421, 'config_store', '配置存储 config_store'),

    # ---------------- USER/app_wifi.c：联网状态机 ----------------
    ('USER/app_wifi.c', 17,
     'STEP_AT → STEP_CHECK → STEP_JOIN → STEP_MQTT → STEP_SUB → STEP_RUN',
     'STEP_AT(初始化 AT) → STEP_CHECK(检查模块) → STEP_JOIN(连接 WiFi) → '
     'STEP_MQTT(连接 MQTT) → STEP_SUB(订阅主题) → STEP_RUN(正常运行)'),
    ('USER/app_wifi.c', 542, '@param type   "unlock" / "lock"',
     '@param type   命令类型，"unlock"（开锁）/ "lock"（上锁）'),

    # ---------------- USER/stm32f10x_it.h：中断向量映射 ----------------
    ('USER/stm32f10x_it.h', 8, 'SVC_Handler   - vPortSVCHandler',
     'SVC_Handler   - vPortSVCHandler（FreeRTOS 系统调用入口）'),
    ('USER/stm32f10x_it.h', 9, 'PendSV_Handler - xPortPendSVHandler',
     'PendSV_Handler - xPortPendSVHandler（任务上下文切换）'),
    ('USER/stm32f10x_it.h', 10, 'SysTick_Handler - xPortSysTickHandler',
     'SysTick_Handler - xPortSysTickHandler（系统节拍）'),
]


def apply_edits(path, edits):
    """在单个文件上应用若干 (行号, 原串, 新串)；返回改动行数。"""
    with io.open(path, 'r', encoding='utf-8-sig', newline='') as f:
        had_bom = f.read(1) == '\ufeff'
        f.seek(0)
        lines = f.readlines()

    changed = 0
    for lineno, old, new in edits:
        if lineno < 1 or lineno > len(lines):
            raise AssertionError('%s: 行号 %d 越界（文件共 %d 行）' % (path, lineno, len(lines)))
        idx = lineno - 1
        if old not in lines[idx]:
            raise AssertionError(
                '%s:%d 未找到预期原文：\n  期望包含: %r\n  实际内容: %r'
                % (path, lineno, old, lines[idx].rstrip('\r\n')))
        lines[idx] = lines[idx].replace(old, new, 1)
        changed += 1

    # 统一写回 UTF-8 带 BOM，保留原换行符
    with io.open(path, 'w', encoding='utf-8-sig', newline='') as f:
        f.writelines(lines)
    return changed, had_bom


def main():
    by_file = {}
    for rel, lineno, old, new in EDITS:
        by_file.setdefault(rel, []).append((lineno, old, new))

    total = 0
    for rel in sorted(by_file):
        path = os.path.join(ROOT, rel.replace('/', os.sep))
        if not os.path.isfile(path):
            print('[跳过] 文件不存在: %s' % rel)
            continue
        n, had_bom = apply_edits(path, by_file[rel])
        total += n
        print('[OK] %-42s 改 %2d 行  (原BOM=%s → 已写为带BOM)' % (rel, n, had_bom))

    print()
    print('=== 共修改 %d 处，涉及 %d 个文件 ===' % (total, len(by_file)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
