cmake_minimum_required(VERSION 3.20)
project(stm32_secure_smart_lock VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(SMART_LOCK_BUILD_TESTS "Build portable core tests" ON)
option(SMART_LOCK_BUILD_STM32_SYNTAX "Compile the FreeRTOS integration with API stubs" ON)

add_library(smart_lock_core STATIC
    firmware/app/src/sha1.c
    firmware/app/src/totp.c
    firmware/app/src/lock_auth.c
    firmware/app/src/pin_credential.c
    firmware/app/src/periodic_credential.c
    firmware/app/src/visitor_credential.c
    firmware/app/src/remote_command.c
    firmware/app/src/event_log.c
    firmware/app/src/lock_controller.c
    firmware/app/src/base64.c
    firmware/app/src/onenet_token.c
    firmware/app/src/config_store.c
    firmware/app/src/keypad_input.c
    firmware/drivers/src/ds3231.c
)

target_include_directories(smart_lock_core PUBLIC
    firmware/app/include
    firmware/drivers/include
    # board_port.h（可移植，只依赖 app 头文件）与板级常量
    firmware/stm32/include
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(smart_lock_core PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
    )
endif()

if(SMART_LOCK_BUILD_STM32_SYNTAX)
    add_library(smart_lock_freertos_syntax OBJECT
        firmware/stm32/src/smart_lock_app.c
        firmware/stm32/src/freertos_hooks.c
    )
    target_include_directories(smart_lock_freertos_syntax PRIVATE
        firmware/tests/stubs
        firmware/stm32/include
        firmware/app/include
    )
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(smart_lock_freertos_syntax PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
        )
    endif()
endif()

if(SMART_LOCK_BUILD_TESTS)
    include(CTest)
    add_executable(smart_lock_tests firmware/tests/test_main.c)
    target_link_libraries(smart_lock_tests PRIVATE smart_lock_core)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(smart_lock_tests PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
        )
    endif()
    add_test(NAME smart_lock_tests COMMAND smart_lock_tests)
endif()

# ---------------------------------------------------------------------------
# 固件层编译校验（不链接）
#
# FreeRTOS 直接取自参考工程（firmware/FreeRTOS/，内核 V10.5.1 原样搬入），
# 唯一的一处替换是**移植层**：
#
#   参考工程冻结的是 Keil AC5(RVDS) 移植层
#     FreeRTOS/portable/RVDS/ARM_CM3/portmacro.h
#   里面有 __forceinline 和 RVDS 语法的 __asm { } 块，GCC 无法解析。
#   本仓库的目标工具链是 arm-none-eabi-gcc（见 CMakePresets.json 与 README），
#   所以编译器相关的移植层改用**同一内核版本**的官方移植层：
#     FreeRTOS/portable/GCC/ARM_CM3/{portmacro.h, port.c}
#   RVDS 那份保留在仓库里，Keil 用户可以直接继续用。
#
# 只做"编译"、不链接：链接还需要启动文件、链接脚本、StdPeriph 的 .c 与
# 参考工程驱动实现，等接入 CubeIDE/CMake 完整工程时再补。
# 这一步的价值：把 FreeRTOS 内核 + 移植层 + board_port.c / main.c /
# stm32f10x_it.c 放到真实的 StdPeriph + 参考工程驱动头文件下过一遍类型与
# 函数签名检查。
# ---------------------------------------------------------------------------
set(SMART_LOCK_REF_ROOT "C:/Users/71563/Desktop/智能门锁/STM32_SmartLock"
    CACHE PATH "参考工程（提供 StdPeriph 驱动、HARDWARE、MIDDLEWARE、SYSTEM、USER）")
set(SMART_LOCK_KEIL_DFP "D:/Keil_v5/Packs/Keil/STM32F1xx_DFP/2.3.0"
    CACHE PATH "STM32F1 标准外设库所在目录")
set(SMART_LOCK_CMSIS_CORE "D:/Keil_v5/Packs/ARM/CMSIS/5.8.0/CMSIS/Core/Include"
    CACHE PATH "CMSIS Core 头文件目录")
option(SMART_LOCK_BUILD_FIRMWARE_SYNTAX
       "为 Cortex-M3 编译固件层（仅编译，不链接）" OFF)

if(SMART_LOCK_BUILD_FIRMWARE_SYNTAX)
    add_library(smart_lock_firmware_objects OBJECT
        # ---- FreeRTOS 内核（与参考工程同版本 V10.5.1，原样搬入） ----
        firmware/FreeRTOS/tasks.c
        firmware/FreeRTOS/queue.c
        firmware/FreeRTOS/list.c
        firmware/FreeRTOS/timers.c
        firmware/FreeRTOS/event_groups.c
        firmware/FreeRTOS/stream_buffer.c
        # 内存管理：cJSON / MqttKit 需要 pvPortMalloc（heap_4 = 最佳适配+合并）
        firmware/FreeRTOS/portable/MemMang/heap_4.c
        # Cortex-M3 移植层：GCC 版（参考工程只带 Keil RVDS 版）
        firmware/FreeRTOS/portable/GCC/ARM_CM3/port.c
        # ---- 本工程可移植核心（不含 drivers/ds3231.c：固件侧用参考工程的
        #      HARDWARE/ds3231.c，它带软件 RTC 兜底，二者同名不同实现） ----
        firmware/app/src/sha1.c
        firmware/app/src/totp.c
        firmware/app/src/lock_auth.c
        firmware/app/src/pin_credential.c
        firmware/app/src/periodic_credential.c
        firmware/app/src/visitor_credential.c
        firmware/app/src/remote_command.c
        firmware/app/src/event_log.c
        firmware/app/src/lock_controller.c
        firmware/app/src/base64.c
        firmware/app/src/onenet_token.c
        firmware/app/src/config_store.c
        firmware/app/src/keypad_input.c
        # ---- 应用层与板级适配层 ----
        firmware/stm32/src/smart_lock_app.c
        firmware/stm32/src/freertos_hooks.c
        firmware/stm32/src/board_port.c
        firmware/stm32/src/main.c
        firmware/stm32/src/stm32f10x_it.c
    )

    # 包含路径顺序很关键：
    #   - GCC 移植层目录在最前，让 FreeRTOS 的 "portmacro.h" 解析到这里
    #   - 本工程头文件优先于参考工程，避免 sha1.h / totp.h / base64.h /
    #     onenet_token.h / ds3231.h / FreeRTOSConfig.h 被参考工程的同名
    #     文件抢走
    target_include_directories(smart_lock_firmware_objects PRIVATE
        firmware/FreeRTOS/portable/GCC/ARM_CM3
        firmware/app/include
        firmware/stm32/include
        ${SMART_LOCK_REF_ROOT}/HARDWARE
        ${SMART_LOCK_REF_ROOT}/MIDDLEWARE
        ${SMART_LOCK_REF_ROOT}/SYSTEM
        ${SMART_LOCK_REF_ROOT}/USER
        firmware/FreeRTOS/include
        ${SMART_LOCK_KEIL_DFP}/Device/Include
        ${SMART_LOCK_KEIL_DFP}/Device/StdPeriph_Driver/inc
        ${SMART_LOCK_CMSIS_CORE}
    )
    target_compile_definitions(smart_lock_firmware_objects PRIVATE
        USE_STDPERIPH_DRIVER
        STM32F10X_MD
    )
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(smart_lock_firmware_objects PRIVATE
            -Wall -Wextra -Wno-unused-parameter
        )
    endif()
endif()
