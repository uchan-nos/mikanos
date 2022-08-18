#!/bin/sh -eux

if [ $# -ne 1 ]
then
  echo "Usage: $0 GIT_TAG"
  exit 1
fi

if [ "$(basename $PWD)" != "release_pack" ]
then
  echo "Please run this script in 'release_pack' directory."
  exit 1
fi

mikanos_root=".."
disk="$mikanos_root/disk.img"
kernel="$mikanos_root/kernel/kernel.elf"

if [ ! -f $disk ] || [ ! -f $kernel ]
then
  echo "Please build MikanOS in advance."
  exit 1
fi

edk2_dir="$HOME/edk2"
bootloader="$edk2_dir/Build/MikanLoaderX64/DEBUG_CLANG38/X64/Loader.efi"

if [ ! -f $bootloader ]
then
  echo "Please build MikanLoaderPkg in advance."
  exit 1
fi

git_tag=$1
dist_dir="mikanos_distrib-$git_tag"

if [ -e $dist_dir ]
then
  echo "Please delete the distrib directory: $dist_dir"
  exit 1
fi

mkdir $dist_dir
cp $mikanos_root/LICENSE $mikanos_root/NOTICE $dist_dir
cp README.md install.sh $dist_dir

cp $disk $kernel $dist_dir
cp $bootloader $dist_dir/BOOTX64.EFI
cp -r $mikanos_root/resource $dist_dir

apps="$mikanos_root/apps"
mkdir $dist_dir/apps

for app in $(ls $apps)
do
  app_dir=$apps/$app
  app_bin=$app_dir/$app
  if [ -d $app_dir ] && [ -f $app_bin ]
  then
    cp $app_bin $dist_dir/apps
  fi
done

tar czf $dist_dir.tar.gz $dist_dir
