# RVA23 Linux Boot Payload Build Instructions

This document outlines the procedure to generate the `vmlinux_placeholder.elf` pre-compiled artifact required by the `mpact-riscv` RVA23 boot tests. As per `SPECS.md` constraints, the payload cannot be built natively during CI to avoid OOM limits and excessive build times.

## Artifact Overview

The generated `fw_payload.elf` is a monolithic RISC-V 64-bit ELF binary containing:
1. **OpenSBI Firmware:** Serves as the Machine-mode (M-mode) bootloader.
2. **Linux Kernel (v6.8):** Compiled with strict `RVA23` extension support.
3. **Minimal Initramfs (BusyBox):** An embedded filesystem containing a basic `/init` script that mounts `/proc` and `/sys` before dropping to `/bin/sh`.

## Prerequisites

You must perform this build out-of-band (e.g., in `~/.os_build` or `/tmp/rva23_os_build`).

```bash
export WORKDIR=/tmp/rva23_os_build
export RVA23_MARCH="rv64imafdcv_zicsr_zifencei_zba_zbb_zbs_zicond_zihintpause_zcb_zfa_zicbom_zicboz_zawrs"
export RVA23_MABI="lp64d"

mkdir -p $WORKDIR
cd $WORKDIR

sudo apt-get update
sudo apt-get install -y gcc-riscv64-linux-gnu build-essential flex bison bc libelf-dev libssl-dev cpio curl wget libc6-dev-riscv64-cross
```

## Step 1: Build the Minimal Initramfs (BusyBox)

```bash
wget -q https://busybox.net/downloads/busybox-1.36.1.tar.bz2
tar -xf busybox-1.36.1.tar.bz2
cd busybox-1.36.1

make ARCH=riscv CROSS_COMPILE=riscv64-linux-gnu- defconfig
sed -i 's/# CONFIG_STATIC is not set/CONFIG_STATIC=y/' .config
sed -i 's/CONFIG_TC=y/# CONFIG_TC is not set/' .config

make ARCH=riscv CROSS_COMPILE=riscv64-linux-gnu- EXTRA_CFLAGS="-march=$RVA23_MARCH -mabi=$RVA23_MABI" -j$(nproc)
make ARCH=riscv CROSS_COMPILE=riscv64-linux-gnu- install CONFIG_PREFIX=../initramfs
cd ..

cd initramfs
mkdir -p proc sys dev etc tmp
cat << 'INIT' > init
#!/bin/sh
mount -t proc none /proc
mount -t sysfs none /sys
echo "======================================"
echo " Welcome to RVA23 Minimal Linux Boot! "
echo "======================================"
exec /bin/sh
INIT
chmod +x init
find . -print0 | cpio --null -H newc -o | gzip -9 > ../initramfs.cpio.gz
cd ..
```

## Step 2: Build the RVA23-Aware Linux Kernel

```bash
wget -qO- https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.8.tar.xz | tar xJ
cd linux-6.8
make ARCH=riscv CROSS_COMPILE=riscv64-linux-gnu- defconfig

# Embed Initramfs
./scripts/config --enable CONFIG_BLK_DEV_INITRD
./scripts/config --set-str CONFIG_INITRAMFS_SOURCE "../initramfs.cpio.gz"

# Enable Mandatory RVA23 Extensions (Macro Toggles)
./scripts/config --enable CONFIG_RISCV_ISA_V
./scripts/config --enable CONFIG_RISCV_ISA_ZFA
./scripts/config --enable CONFIG_RISCV_ISA_ZBA
./scripts/config --enable CONFIG_RISCV_ISA_ZBB
./scripts/config --enable CONFIG_RISCV_ISA_ZBS
./scripts/config --enable CONFIG_RISCV_ISA_ZICOND
./scripts/config --enable CONFIG_RISCV_ISA_ZIHINTPAUSE
./scripts/config --enable CONFIG_RISCV_ISA_ZICBOM
./scripts/config --enable CONFIG_RISCV_ISA_ZICBOZ
./scripts/config --enable CONFIG_RISCV_ISA_SVPBMT

make ARCH=riscv CROSS_COMPILE=riscv64-linux-gnu- olddefconfig

# Force the compiler to use the exact RVA23 string for compilation
sed -i "s/^KBUILD_CFLAGS += -march=.*/KBUILD_CFLAGS += -march=$RVA23_MARCH/" arch/riscv/Makefile
sed -i "s/^KBUILD_AFLAGS += -march=.*/KBUILD_AFLAGS += -march=$RVA23_MARCH/" arch/riscv/Makefile

make ARCH=riscv CROSS_COMPILE=riscv64-linux-gnu- -j$(nproc) vmlinux Image
cd ..
```

## Step 3: Build OpenSBI (The Payload Wrapper)

```bash
git clone https://github.com/riscv-software-src/opensbi.git
cd opensbi

make PLATFORM=generic CROSS_COMPILE=riscv64-linux-gnu- \
     PLATFORM_RISCV_ISA=$RVA23_MARCH PLATFORM_RISCV_ABI=$RVA23_MABI \
     FW_PAYLOAD_PATH=../linux-6.8/arch/riscv/boot/Image -j$(nproc)
cd ..
```

## Step 4: Deploy

Copy the final monolithic payload to the exact path expected by the CI boot tests:

```bash
cp opensbi/build/platform/generic/firmware/fw_payload.elf /workspace/louhi_ws/vmlinux_placeholder.elf
chmod 755 /workspace/louhi_ws/vmlinux_placeholder.elf
```
