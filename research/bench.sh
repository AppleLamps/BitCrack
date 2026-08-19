#!/bin/sh
# Reproduces the benchmark table in research/REPORT.md.
# Run from the repository root after `make dir_kangaroo`.

echo '### ./bin/kangaroo --bits 30 --benchmark 200 --arms 3:fold --jumps 4096 --seed 101 -t 8'
./bin/kangaroo --bits 30 --benchmark 200 --arms 3:fold --jumps 4096 --seed 101 -t 8

echo '### ./bin/kangaroo --bits 30 --benchmark 200 --arms 2:fold --jumps 4096 --seed 103 -t 8'
./bin/kangaroo --bits 30 --benchmark 200 --arms 2:fold --jumps 4096 --seed 103 -t 8

echo '### ./bin/kangaroo --bits 32 --benchmark 100 --arms 3:fold --jumps 4096 --seed 109 -t 8'
./bin/kangaroo --bits 32 --benchmark 100 --arms 3:fold --jumps 4096 --seed 109 -t 8

echo '### ./bin/kangaroo --bits 34 --benchmark 60 --arms 3:fold --jumps 4096 --seed 107 -t 8'
./bin/kangaroo --bits 34 --benchmark 60 --arms 3:fold --jumps 4096 --seed 107 -t 8

echo '### ./bin/kangaroo --bits 30 --benchmark 60 --arms 3:fold --jumps 1024 --seed 113 -t 8'
./bin/kangaroo --bits 30 --benchmark 60 --arms 3:fold --jumps 1024 --seed 113 -t 8

echo '### ./bin/kangaroo --bits 30 --benchmark 60 --arms 3:fold --jumps 16384 --seed 113 -t 8'
./bin/kangaroo --bits 30 --benchmark 60 --arms 3:fold --jumps 16384 --seed 113 -t 8

echo '### ./bin/kangaroo --bits 30 --benchmark 60 --arms 3:fold --jumps 4096 --cycle-hist 2 --seed 127 -t 8'
./bin/kangaroo --bits 30 --benchmark 60 --arms 3:fold --jumps 4096 --cycle-hist 2 --seed 127 -t 8

echo '### ./bin/kangaroo --bits 30 --benchmark 60 --arms 3:fold --jumps 4096 --cycle-hist 8 --seed 127 -t 8'
./bin/kangaroo --bits 30 --benchmark 60 --arms 3:fold --jumps 4096 --cycle-hist 8 --seed 127 -t 8

echo '### ./bin/kangaroo --bits 30 --benchmark 60 --arms fold:fold3 --jumps 4096 --seed 131 -t 8'
./bin/kangaroo --bits 30 --benchmark 60 --arms fold:fold3 --jumps 4096 --seed 131 -t 8
