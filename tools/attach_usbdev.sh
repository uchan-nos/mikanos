#!/bin/sh

attach_dev() {
  powershell.exe Start-Process cmd.exe -Verb runas -ArgumentList cmd.exe,/C,usbipd,wsl,attach,--busid,$1
}

if [ $# -ne 1 ]
then
  echo Please specify a bus id to be attached
  (cd $(wslpath -u "C:\Windows"); cmd.exe /C "usbipd wsl list")
  exit 1
fi

echo Attaching a specified device: BUS_ID = $1
attach_dev $1
