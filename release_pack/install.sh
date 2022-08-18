#!/bin/sh -eu

if [ $# -lt 1 ]
then
  echo "Usage: $0 <usb flash device file>"
  exit 1
fi

USB_FLASH=$1

mkfs.fat -F 32 -n MIKANOS $USB_FLASH

mkdir -p mnt
mount $USB_FLASH mnt
mkdir -p mnt/EFI/BOOT
cp BOOTX64.EFI mnt/EFI/BOOT/BOOTX64.EFI
cp kernel.elf mnt/kernel.elf

cp -r apps mnt/
cp resource/* mnt/

sleep 0.5
umount mnt
