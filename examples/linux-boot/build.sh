#!/usr/bin/env bash
set -euo pipefail

recipe=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
output=${1:-"$recipe/../../build/linux-boot"}
cross_compile=${CROSS_COMPILE:-riscv64-linux-gnu-}
jobs=${JOBS:-$(nproc)}

mkdir -p -- "$output"
output=$(cd -- "$output" && pwd)
cache=${LINUX_BOOT_SOURCE_CACHE:-"$output/downloads"}
mkdir -p -- "$cache" "$output/sources"
cache=$(cd -- "$cache" && pwd)
sources="$output/sources"

fetch() (
  local name=$1 url=$2 checksum=$3 download
  if [[ -f "$cache/$name" ]]; then
    printf '%s  %s\n' "$checksum" "$cache/$name" | sha256sum --check
    return
  fi

  download=$(mktemp "$cache/$name.XXXXXX")
  trap 'rm -f -- "$download"' EXIT
  curl --fail --silent --show-error --location --retry 3 --output "$download" "$url"
  printf '%s  %s\n' "$checksum" "$download" | sha256sum --check
  mv -- "$download" "$cache/$name"
)

opensbi_commit=bd613dd92113f683052acfb23d9dc8ba60029e0a
opensbi="opensbi-$opensbi_commit"
linux='linux-6.12'
busybox='busybox-1.37.0'

fetch opensbi-1.6.tar.gz \
  "https://codeload.github.com/riscv-software-src/opensbi/tar.gz/$opensbi_commit" \
  e2463916c9c0e646ec93f342dde2d24f55e3277b828254b5d6ffea1f7c5ab89c
fetch "$linux.tar.xz" "https://cdn.kernel.org/pub/linux/kernel/v6.x/$linux.tar.xz" \
  b1a2562be56e42afb3f8489d4c2a7ac472ac23098f1ef1c1e40da601f54625eb
fetch "$busybox.tar.bz2" "https://www.busybox.net/downloads/$busybox.tar.bz2" \
  3311dff32e746499f4df0d5df04d7eb396382d7e108bb9250e7b519b837043a4

unpack() {
  local archive=$1 source=$2 staging
  if [[ ! -d "$sources/$source" ]]; then
    staging=$(mktemp -d "$sources/.unpack.XXXXXX")
    tar -xf "$cache/$archive" -C "$staging"
    mv -- "$staging/$source" "$sources/$source"
    rmdir -- "$staging"
  fi
}

unpack opensbi-1.6.tar.gz "$opensbi"
unpack "$linux.tar.xz" "$linux"
unpack "$busybox.tar.bz2" "$busybox"

make -C "$sources/$opensbi" -j "$jobs" CROSS_COMPILE="$cross_compile" \
  PLATFORM=generic FW_TEXT_START=0x80000000 FW_JUMP_OFFSET=0x200000 \
  FW_JUMP_FDT_OFFSET=0x07e00000

(
  cd -- "$sources/$linux"
  make ARCH=riscv CROSS_COMPILE="$cross_compile" defconfig
  scripts/kconfig/merge_config.sh -m .config "$recipe/linux.config"
  make ARCH=riscv CROSS_COMPILE="$cross_compile" olddefconfig
  make -j "$jobs" ARCH=riscv CROSS_COMPILE="$cross_compile" Image
)

(
  cd -- "$sources/$busybox"
  make CROSS_COMPILE="$cross_compile" defconfig
  "$sources/$linux/scripts/kconfig/merge_config.sh" -m .config "$recipe/busybox.config"
  make CROSS_COMPILE="$cross_compile" oldconfig </dev/null
  make -j "$jobs" CROSS_COMPILE="$cross_compile"
)

"${HOSTCC:-cc}" -O2 "$sources/$linux/usr/gen_init_cpio.c" -o "$output/gen_init_cpio"
BUSYBOX_BINARY="$sources/$busybox/busybox" LINUX_BOOT_INIT="$recipe/init" \
  "$output/gen_init_cpio" -t 0 "$recipe/initramfs.list" > "$output/initramfs.cpio.part"
cp -- "$sources/$opensbi/build/platform/generic/firmware/fw_jump.bin" "$output/"
cp -- "$sources/$linux/arch/riscv/boot/Image" "$output/"
mv -- "$output/initramfs.cpio.part" "$output/initramfs.cpio"
printf 'Linux boot images are in %s\n' "$output"
