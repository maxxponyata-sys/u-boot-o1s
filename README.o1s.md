# U-Boot for Samsung Galaxy S21 (o1s)

Codename **o1s** · model **SM-G991B** · SoC **Exynos 2100 (S5E9840)**

U-Boot (v2027.01-rc2 based) running on the Galaxy S21 as a second-stage
bootloader after Samsung's S-Boot, started from the `recovery` partition.

> **Status: early bring-up / experimental.** Tested on one SM-G991B
> (board rev 22, bootloader G991BXXSJHZC2, bootloader unlocked).

## What works

- S-Boot loads U-Boot from `recovery` (Android boot image header v2); U-Boot
  runs at **EL1** (Samsung's uH hypervisor owns EL2)
- MMU and caches, relocation, memory layout taken from S-Boot's device tree
- Display through the bootloader framebuffer (`0xFA000000`, 1080×2400,
  `simple-framebuffer`, DECON0 kept in HW-trigger mode)
- Volume keys (`gpio-keys` via the new Exynos 2100 pinctrl driver), `bootmenu`
- Pressing **Volume Down** moves through the menu (wraps around), **Volume Up**
  selects

## Not yet

- Power key (it is on the S2MPS23 PMIC, no driver yet)
- UFS storage, USB (DWC3), UART
- Using DRAM outside the bank U-Boot runs from (the other banks hold
  firmware/hypervisor memory that is only described in S-Boot's
  reserved-memory, which LMB/EFI do not honour yet)
- Booting Linux: there is a hook for a FIT appended at +4 MiB, untested

## Build

```sh
export CROSS_COMPILE=aarch64-linux-gnu-
scripts/o1s/mkrecovery-o1s.sh /path/to/your-recovery.img u-boot-o1s-recovery.img
```

The DTB table and recovery DTBO inside the image are Samsung's and are not
shipped here; the script copies them from a recovery image you provide (a
backup of your own `recovery` partition works).

## Flash and run

Requires an unlocked bootloader. **This replaces your recovery (e.g. TWRP).
Back up the `recovery` partition first.**

```sh
# from rooted Android
adb push u-boot-o1s-recovery.img /data/local/tmp/
adb shell su -c 'dd if=/data/local/tmp/u-boot-o1s-recovery.img of=/dev/block/by-name/recovery bs=4M conv=fsync'
adb reboot recovery
# or, from download mode
heimdall flash --RECOVERY u-boot-o1s-recovery.img
```

To get back: hold **Volume Down + Power** (boots Android), then restore your
recovery backup with `dd`, or use download mode and Heimdall/Odin.

## Bring-up aids (temporary, still in this tree)

- U-Boot clears the PMU reboot mode (`SYSIP_DAT0`) and sets the Samsung reset
  reason to "normal" at start, so any reset afterwards boots Android instead
  of looping into `recovery`.
- The cluster-0 watchdog is left running, so a hung U-Boot resets by itself.
- Everything U-Boot prints, plus every init step and device probe, is mirrored
  into the ramoops `pmsg` zone (`0x8F020000`). After the phone is back in
  Android, read it with
  `adb shell su -c 'cat /sys/fs/pstore/pmsg-ramoops-0'`.
- White bands on the framebuffer mark boot stages.

## Fixes made during bring-up

- Do not require a serial console (no UART node → `panic` in `serial_init`)
- Keep only the DRAM bank U-Boot runs from; map the framebuffer separately
- Opaque alpha for console colours: DECON honours per-pixel alpha even for
  XRGB8888, so alpha 0 is invisible
- `console_setfile()`: do not dereference a NULL `gd->jt` when a stdio device
  (button-kbd) registers before `jumptable_init()`
- PIE build (TEXT_BASE 0): find the appended payload relative to the real
  load address

## Credits / license

Based on upstream U-Boot (`board/samsung/exynos-mobile` by Kaustabh
Chakraborty). GPL-2.0+, like U-Boot. No Samsung binaries are included.

---

**По-русски кратко:** U-Boot для Galaxy S21 (o1s, SM-G991B, Exynos 2100).
Запускается S-Boot'ом из раздела `recovery`, работает на EL1, есть экран, меню
и кнопки громкости (вниз — по меню, вверх — выбор). Кнопки питания, UFS и USB
пока нет. Перед прошивкой сделайте бэкап `recovery`.
