#!/usr/bin/env python3

import pp
import pptool

p = pptool.pptool()
p.init()

elements = pp.Sequence()
c = 12345
v1 = 5
v2 = 15
for v in range(v1,v2):
    elements.push_back(pp.el(c, v))
elements.push_back(pp.el())

p.send_and_check(elements)
