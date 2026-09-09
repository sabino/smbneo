#!/usr/bin/env python3
"""Compare owned-ROM per-frame state traces of reference and optimized C."""
import argparse
import re
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('reference')
    parser.add_argument('optimized')
    parser.add_argument('rom')
    args = parser.parse_args()
    traces = []
    for binary in (args.reference, args.optimized):
        result = subprocess.run([binary, args.rom], text=True, capture_output=True, timeout=60)
        print(result.stdout, end='')
        if result.returncode:
            raise SystemExit(result.stderr or f'{binary} failed: {result.returncode}')
        matches = re.findall(r'^VS frame trace: ([0-9a-f]{16})$', result.stdout, re.M)
        if len(matches) != 1:
            raise SystemExit(f'{binary}: missing or ambiguous trace')
        traces.append(matches[0])
    if traces[0] != traces[1]:
        raise SystemExit(f'VS state trace mismatch: {traces}')
    print('Reference/optimized per-frame state traces match:', traces[0])


if __name__ == '__main__': main()
