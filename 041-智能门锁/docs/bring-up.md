# Hardware bring-up sequence

Do not connect every module at once. Complete and record each gate before moving
to the next one.

1. Open `firmware/stm32/cubemx/SmartLock.ioc`, generate the CubeIDE project,
   then verify 3.3 V and 5 V rails, common ground, reset, SWD, and 72 MHz clock.
2. Run GPIO LED and buzzer tests; confirm active levels.
3. Verify I2C OLED, then DS3231 time read/write and oscillator-stop handling.
4. Verify RC522 SPI with a known card; do not grant access on UID read errors.
5. Verify W25Q64 JEDEC ID, sector erase, record write, reset, and recovery scan.
6. Verify fingerprint UART enrollment, match, timeout, and disconnect behavior.
7. Verify keypad ghosting/debounce and virtual-PIN input length limits.
8. Drive the servo from a separate suitable supply; test stall and reset cases.
9. Verify door and tamper EXTI callbacks only notify tasks and never block.
10. Verify ESP8266 UART DMA framing, reconnect backoff, MQTT/TLS, and offline use.
11. Run 100 unlock/lock cycles and power-cut tests during log and config writes.
12. Enable watchdog and readout protection only after recovery is documented.
