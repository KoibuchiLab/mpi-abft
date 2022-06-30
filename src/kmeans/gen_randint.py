#!/usr/bin/env python3
#
# Synopsis:
#   ./gen_randint.py 128
#
# Author: huyao 220630

import sys
import random

n = sys.argv[1]
num = int(n)
with open('randint_'+n+'.txt', "w") as f:
    for i in range(num):
        r = random.randint(1, 1000)
        f.write(str(r)+'\n') 
