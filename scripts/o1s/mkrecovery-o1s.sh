#!/bin/sh
# SPDX-License-Identifier: GPL-2.0+
#
# Build U-Boot for the Samsung Galaxy S21 (o1s, SM-G991B) and pack it as an
# Android boot image (header v2) that S-Boot loads from the "recovery"
# partition.
#
# S-Boot expects the DTB table and recovery DTBO that come with a recovery
# image for this phone. They are Samsung's, so they are not shipped here:
# pass any working recovery.img for your phone (e.g. a backup of your own
# recovery partition) and they are copied from it.
#
# Usage: scripts/o1s/mkrecovery-o1s.sh <your-recovery.img> [output.img]
# Needs: aarch64-linux-gnu- toolchain, mkbootimg/unpack_bootimg (Android).

set -eu

src=${1:?usage: $0 <your-recovery.img> [output.img]}
out=${2:-u-boot-o1s-recovery.img}
top=$(cd "$(dirname "$0")/../.." && pwd)
build=${BUILD_DIR:-$top/.output-o1s}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

export CROSS_COMPILE=${CROSS_COMPILE:-aarch64-linux-gnu-}
make -C "$top" O="$build" exynos2100-o1s_defconfig
make -C "$top" O="$build" DEVICE_TREE=exynos/exynos2100-o1s -j"$(nproc)"

unpack_bootimg --boot_img "$src" --out "$work/src" >/dev/null
[ -s "$work/src/dtb" ] || { echo "no DTB in $src" >&2; exit 1; }
[ -s "$work/src/recovery_dtbo" ] || { echo "no recovery DTBO in $src" >&2; exit 1; }

# S-Boot wants a ramdisk; an empty gzip'ed cpio is enough.
mkdir "$work/rd"
(cd "$work/rd" && echo . | cpio -o -H newc 2>/dev/null | gzip -9 > "$work/ramdisk.gz")

# Load addresses are ignored by S-Boot; these match common TWRP builds.
mkbootimg --header_version 2 \
	--kernel "$build/u-boot.bin" \
	--ramdisk "$work/ramdisk.gz" \
	--dtb "$work/src/dtb" \
	--recovery_dtbo "$work/src/recovery_dtbo" \
	--base 0x10000000 --kernel_offset 0x00008000 \
	--ramdisk_offset 0x04000000 --tags_offset 0 \
	--dtb_offset 0x100000000 --pagesize 4096 \
	--os_version 14.0.0 --os_patch_level 2099-12 \
	--cmdline "androidboot.hardware=exynos2100" \
	-o "$out"

echo "Wrote $out"
