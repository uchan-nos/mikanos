#!/bin/sh -eu

make ${MAKE_OPTS:-} -C kernel kernel.elf

for MK in $(ls apps/*/Makefile)
do
  APP_DIR=$(dirname $MK)
  APP=$(basename $APP_DIR)
  make ${MAKE_OPTS:-} -C $APP_DIR $APP
done

DISK_IMG=./disk.img MIKANOS_DIR=$PWD $HOME/osbook/devenv/make_mikanos_image.sh

if [ "${1:-}" = "run" ]
then
  $HOME/osbook/devenv/run_image.sh ./disk.img
fi
