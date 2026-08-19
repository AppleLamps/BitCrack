#!/usr/bin/env bash
# Solved-puzzle regression ladder: run the solver against real Bitcoin puzzle
# targets whose keys are public, and check the recovered key exactly.
#
# Any new mechanism must clear this ladder before it is pointed at an unsolved
# range: a mechanism that is fast but wrong is worse than the baseline.
#
# Usage: research/ladder.sh [mechanism-args...]      (default: --fold)
#   research/ladder.sh --fold
#   research/ladder.sh --charges 3
#   PUZZLES="40 45" research/ladder.sh --fold -t 8
set -u

BIN=./bin/kangaroo
ARGS=${*:-"--fold"}
PUZZLES=${PUZZLES:-"40 45 50"}

# puzzle -> pubkey:privkey (from the public solved list; range is [2^(n-1), 2^n-1])
key_40=03a2efa402fd5268400c77c20e574ba86409ededee7c4020e4b9f0edbee53de0d4:E9AE4933D6
key_45=026ecabd2d22fdb737be21975ce9a694e108eb94f3649c586cc7461c8abf5da71a:122FCA143C05
key_50=03f46f41027bbf44fafd6b059091b900dad41e6845b2241dc3254c7cdd3c5a16c6:22BD43C2E9354

fail=0
for n in $PUZZLES; do
    eval "entry=\${key_$n:-}"
    if [ -z "$entry" ]; then echo "puzzle $n: no key on file, skipping"; continue; fi
    pub=${entry%%:*}
    want=${entry##*:}

    echo "=== puzzle $n  (2^$((n-1)) .. 2^$n-1)  args: $ARGS"
    start=$(date +%s.%N)
    out=$("$BIN" -k "$pub" --bits "$n" $ARGS 2>&1)
    end=$(date +%s.%N)
    secs=$(echo "$end - $start" | bc)

    echo "$out" | grep -Ei 'steps|found|folds|cycles' | sed 's/^/    /'
    if echo "$out" | grep -qi "$want"; then
        printf "    PASS  key %s in %.1fs\n" "$want" "$secs"
    else
        printf "    FAIL  expected %s, not in output (%.1fs)\n" "$want" "$secs"
        echo "$out" | tail -5 | sed 's/^/    | /'
        fail=1
    fi
done
exit $fail
