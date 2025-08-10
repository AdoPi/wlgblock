#!/bin/env python3
import sys

col=int(sys.argv[1], base=16)

r = (col & 0xFF0000) >> 16
g = (col & 0x00FF00) >> 8
b = (col & 0x0000FF) >> 0

r = round(r / 0xFF * 0x1F)
g = round(g / 0xFF * 0x1F)
b = round(b / 0xFF * 0x1F)

print((f"  db {hex(((g & 0x07) << 5) | r)}, {hex((b << 2) | (g >> 3))}").replace("0x", "$"))
