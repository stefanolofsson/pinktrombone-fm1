#!/usr/bin/env python3
"""Grid-search the tongue / lip settings that put F1 and F2 on a vowel's targets (host/render must be built)."""
import itertools, subprocess, sys
import numpy as np
sys.path.insert(0, 'host')
from formants import formants

TARGETS = {'AH': (730, 1090), 'AE': (660, 1720), 'EH': (530, 1840), 'IH': (390, 1990), 'EE': (270, 2290),
           'UH': (520, 1190), 'AW': (570, 840), 'OH': (450, 880), 'OO': (300, 870)}

def measure(idx, dia, lip, f0=120):
    subprocess.run(['host/render', '/tmp/pt_t.raw', '0.6', str(f0), str(idx), str(dia), str(lip), '0.6', '0.01'], check=True)
    x = np.fromfile('/tmp/pt_t.raw', dtype=np.float32)
    return formants(x[len(x) // 2: len(x) // 2 + 4096])

for name in sys.argv[1:]:
    t1, t2 = TARGETS[name]
    best = None
    for idx, dia, lip in itertools.product([12, 13, 14, 15, 16, 18, 20, 22, 24, 26, 28], [2.05, 2.3, 2.6, 2.9, 3.2, 3.5],
                                           [0.6, 0.8, 1.0, 1.3, 1.7, 3.0]):
        f = measure(idx, dia, lip)
        if len(f) < 2:
            continue
        sc = abs(f[0] - t1) / t1 + abs(f[1] - t2) / t2
        if best is None or sc < best[0]:
            best = (sc, idx, dia, lip, f[:3])
    print(name, 'target', (t1, t2), 'best', best)
