#!/bin/sh
# Per-step cost of the folded walk versus the plain walk.  Same key, same range,
# same jump table; only the mechanism differs.  Compare the ops/s column: the
# step-count win from folding is only real if it is not eaten by slower steps.
# The range must be wide enough that the walk, not thread start-up, dominates
# the wall clock; 2^48 gives runs of several seconds.
K=$1
BITS=${2:-48}
for s in 1 2 3; do
  for m in "--charges 3" "--fold"; do
    printf '%-12s seed %s  ' "$m" "$s"
    ./bin/kangaroo -k $K --bits $BITS $m --jumps 4096 --seed $s -t 8 \
      | grep stats | sed 's/.*  \([0-9,]*\) ops\/s/\1 ops\/s/'
  done
done
