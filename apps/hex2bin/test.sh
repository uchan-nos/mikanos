#!/bin/bash -u

clang++ -o hex2bin-linux hex2bin.cpp

function hex2bin() {
  src="$1"
  want="$2"
  opt="${3:-}"
  got=$(echo $src | ./hex2bin-linux $opt | hexdump -v -e '/1 "%02x "')

  if [ "$want" = "$got" ]
  then
    echo "[  OK  ]: ($opt) $src -> '$got'"
  else
    echo "[FAILED]: ($opt) $src -> '$got', want '$want'"
  fi
}

function bin2hex() {
  src="$1"
  want="$2"
  opt="${3:-}"
  got=$(echo $src | xxd -r -p | ./hex2bin-linux -r $opt)

  if [ "$want" = "$got" ]
  then
    echo "[  OK  ]: ($opt) $src -> '$got'"
  else
    echo "[FAILED]: ($opt) $src -> '$got', want '$want'"
  fi
}

hex2bin "42 61 64" "42 61 64 "
hex2bin "42 61 64" "42 61 64 " "-l"
hex2bin "42 61 64" "42 00 61 00 64 00 " "-l -s 2"
hex2bin "42 61 64" "00 42 00 61 00 64 " "-s 2"
hex2bin "4261 64" "42 61 00 64 " "-s 2"
hex2bin "f0420123" "f0 42 01 23 " "-s 4"
hex2bin "f0420123" "23 01 " "-l -s 2"

bin2hex "61 42 00 f0" "61 42 00 f0"
bin2hex "61 42 00 f0" "6142 00f0" "-s 2"
bin2hex "61 42 00 f0" "4261 f000" "-l -s 2"
bin2hex "01 02 03 04" "0102 0304" "-s 2"
bin2hex "01 02 03" "0102 0300" "-s 2"
