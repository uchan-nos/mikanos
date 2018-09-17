#!/usr/bin/python3

import argparse
import collections
import functools
import re
import sys


BITMAP_PATTERN = re.compile(r'([.*@]+)')


def compile(src: str) -> bytes:
    src = src.lstrip()
    result = []

    for line in src.splitlines():
        m = BITMAP_PATTERN.match(line)
        if not m:
            continue

        bits = [(0 if x == '.' else 1) for x in m.group(1)]
        bits_int = functools.reduce(lambda a, b: 2*a + b, bits)
        result.append(bits_int.to_bytes(1, byteorder='little'))

    return b''.join(result)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('font', help='path to a font file')
    parser.add_argument('-o', help='path to an output file', default='font.out')
    ns = parser.parse_args()

    with open(ns.o, 'wb') as out, open(ns.font) as font:
        src = font.read()
        out.write(compile(src))


if __name__ == '__main__':
    main()
