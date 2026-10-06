# TinyUSB (vendored subset)

Vendored subset of [TinyUSB](https://github.com/hathach/tinyusb) release
**0.16.0**, MIT licensed (see `LICENSE`).

Only the files required for a USB device with one CDC-ACM interface on a
Synopsys DWC2 controller (STM32H7) are included:

- `src/tusb.c`, `src/device/usbd*.c` - device stack core
- `src/class/cdc/cdc_device.c` - CDC-ACM class driver
- `src/common/` - shared utilities (FIFO, types)
- `src/osal/osal_none.h` - polled (no-OS) operation
- `src/portable/synopsys/dwc2/` - STM32H7 DWC2 device controller driver

The sources are unmodified. They are compiled by the `bspUsbCdc` module in
`platforms/stm32/bsp/bspUsbCdc`, which also provides `tusb_config.h`.
