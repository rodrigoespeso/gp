#!/bin/bash
make
echo ""
scp -r /home/rodrigo/N-Body-Phi-KST-CRIT mic0:/home/rodrigo
scp cd-kst-crit.sh mic0:
echo ""
echo "	All ok!"
echo ""