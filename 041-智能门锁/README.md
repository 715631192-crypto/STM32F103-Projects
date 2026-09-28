# STM32 Secure IoT Smart Lock MVP

This repository implements the approved MVP architecture for an STM32F103C8T6
smart lock. The security-sensitive logic is isolated from the hardware so it
can be tested on a desktop before it is integrated with the firmware layer in
`firmware/stm32` (StdPeriph + FreeRTOS, at `arm-none-eabi-gcc`).

## Repository layout

- `firmware/app`: portable C11 authentication, TOTP, signed-command, event-log,
  periodic credential, and lock-state modules.
- `firmware/stm32`: 板级适配层与应用任务层 —— StdPeriph + 参考工程驱动 +
  FreeRTOS，接口定义在 `board_port.h`，引脚见 `docs/pinout.md`
  （`cubemx/SmartLock.ioc` 是早期 HAL 方案遗留，仅作引脚历史记录）。
- `firmware/FreeRTOS`: FreeRTOS V10.5.1 内核，取自参考工程；另附官方同版本
  GCC 移植层（详见 `THIRD_PARTY.md`）。
- `firmware/drivers`: portable module drivers, beginning with trusted DS3231
  time decoding for offline TOTP.
- `firmware/tests`: host-side tests, including RFC 6238 TOTP vectors.
- `cloud`: Mosquitto and Node-RED MVP deployment.
- `protocol`: MQTT topics and payload contracts.
- `docs`: hardware mapping, architecture, security boundaries, and bring-up.
- `tools`: 工程生成与源码规范工具 —— `gen_keil_project.py` 生成 Keil 工程，
  `fix_source_bom.py` 检查/修补源码 UTF-8 BOM，
  `gen_oled_font.py` 体检/补齐 OLED 中文字库缺字。
- `SmartLock.uvprojx`（连同 `STM32F103C8.sct`、`startup_stm32f103xb.s`）：
  可直接编译出可烧录固件的 **Keil MDK 工程**。

The agreed MVP boundary and remaining on-board work are listed in
`docs/mvp-scope.md`; the measured F103 memory headroom is in
`docs/memory-budget.md`.

## Verify the portable firmware core

```powershell
cmake --preset host-debug
cmake --build --preset host-debug
ctest --preset host-debug
```

The portable core can also be cross-compiled for Cortex-M3 with the STM32 GNU
toolchain:

```powershell
cmake --preset cortex-m3-release
cmake --build --preset cortex-m3-release
```

## Build the flashable firmware with Keil MDK

Open `SmartLock.uvprojx` in Keil MDK (ARM Compiler 5) and hit Build; the project
registers the startup file, the scatter file, the FreeRTOS RVDS port, the
StdPeriph sources and every driver, so it links a real image
(`obj/SmartLock.hex`). A headless rebuild verifies it end to end:

```powershell
D:\Keil_v5\UV4\UV4.exe -r SmartLock.uvprojx -j0 -o build_keil.log
```

Measured with ARM Compiler 5 (`V5.06 update 7 build 960`):

```
Program Size: Code=55776  RO-data=5896  RW-data=400  ZI-data=17672
0 Error(s), 0 Warning(s)
```

**Important:** every source file containing non-ASCII text must be saved as
UTF-8 **with BOM**. ARM Compiler 5 only recognises UTF-8 through the BOM;
without it the compiler falls back to the system code page and mangles Chinese
string literals into `missing closing quote` errors. Run
`python tools/fix_source_bom.py --check` before committing. Details and the
controlled experiment are in `firmware/stm32/README.md`.

The Chinese font table (`firmware/HARDWARE/oledfont_cn.h`) is maintained by
`tools/gen_oled_font.py`, which scans the UI source for the characters it
actually uses. Missing code points render as hollow boxes, so run
`python tools/gen_oled_font.py --check` after adding any on-screen Chinese.
`--trim` filters the table down to the characters the UI really uses, keeping
the existing bitmaps byte for byte — that is how the table went from 151 to
73 glyphs and paid for the device-side password-change wizard.

The generated `.hex` is **not** reproducible: `HARDWARE/ds3231.c` bakes
`__DATE__` / `__TIME__` into the RTC baseline, so two identical builds produce
different hex content. Never use the hex MD5 to decide whether a rebuild
actually happened; check the build log for the `compiling ...` lines and the
`Program Size:` line instead.

The host tests do not replace on-target testing. Fingerprint, RFID, OLED, RTC,
Flash, servo, tamper input, and ESP8266 behavior must be verified on the final
board.

## Current status

The portable security core, protocol, and local cloud assets are implemented
and tested. FreeRTOS (V10.5.1) is vendored from the reference project. The board
layer (`firmware/stm32/src/board_port.c`) implements every function declared in
`board_port.h` on top of the reference project's StdPeriph drivers, and the
Keil MDK project links a complete firmware image (62072 B Flash / 18072 B SRAM
of 64 KB / 20 KB) with zero errors and zero warnings.

The CMake cross-build (`-DSMART_LOCK_BUILD_FIRMWARE_SYNTAX=ON`) performs
compile-only checking with `arm-none-eabi-gcc`; producing a flashable image is
the Keil project's job. See `firmware/stm32/README.md` for the memory budget,
the UTF-8 BOM rule, and the on-target measurements still outstanding.

## License

New code in this repository is available under the MIT License. Third-party
components added later retain their own licenses and must be recorded in the
release bill of materials.
