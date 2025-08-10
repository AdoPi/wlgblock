#!/bin/env python3
import sys

txt = "0x" + "".join(sys.argv[1].replace("$","").split(",")[::-1])
col=int(txt, base=16)

r = (col & 0x1F)
g = (col & 0x3E0) >> 5
b = (col & 0x7C00) >> 10

r = round(r / 0x1F * 0xFF)
g = round(g / 0x1F * 0xFF)
b = round(b / 0x1F * 0xFF)

print(f"{hex(r)}{hex(g).replace('0x','')}{hex(b).replace('0x','')}")
