#!/usr/bin/env python3
"""GATE 8 -- the engine's NNUEU_FT_PHASE, against invariants rather than a replica."""
import numpy as np, sys, os
os.chdir(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
L = lambda f: np.loadtxt("gate_ftphase/" + f, dtype=int)
ref, uni, up, dn = L("e_ref.txt"), L("e_uniform.txt"), L("e_b3up.txt"), L("e_b3dn.txt")
fens = [l.strip() for l in open("gate_ftphase/fens.txt") if l.strip()]
npieces = np.array([sum(1 for c in f.split()[0] if c.isalpha()) for f in fens])
buckets = np.minimum(7, np.maximum(0, (npieces - 1) // 4))

ok = True
print("8a  8 buckets IDENTICOS  ==  build sin FT_PHASE (bit a bit)")
a = np.array_equal(ref, uni); ok &= a
print(f"    [{'ok' if a else 'FAIL'}] {int((ref==uni).sum())}/{len(ref)} evals identicas")
if not a:
    for i in np.where(ref != uni)[0]:
        print(f"      fen {i} (bucket {buckets[i]}): sin_ftphase {ref[i]}  ftphase {uni[i]}")

print("\n8b  perturbar SOLO el bucket 3 solo puede mover posiciones del bucket 3")
moved = (up != uni) | (dn != uni)
outside = moved & (buckets != 3)
b = not outside.any(); ok &= b
print(f"    [{'ok' if b else 'FAIL'}] 0 posiciones fuera del bucket 3 se mueven"
      f"{'' if b else '  -> ' + str(np.where(outside)[0].tolist())}")
inb = buckets == 3
c = bool(moved[inb].all()) and int(inb.sum()) > 0; ok &= c
print(f"    [{'ok' if c else 'FAIL'}] las {int(inb.sum())} del bucket 3 se mueven "
      f"(en al menos una de las dos direcciones)")

print(f"\n    {'fen':>4} {'piezas':>7} {'bucket':>7} {'ref':>7} {'unif':>7} {'x1.5':>7} {'x0.5':>7}")
for i in range(len(fens)):
    tag = "  <== bucket perturbado" if buckets[i] == 3 else ""
    print(f"    {i:>4} {npieces[i]:>7} {buckets[i]:>7} {ref[i]:>7} {uni[i]:>7} {up[i]:>7} {dn[i]:>7}{tag}")
print("\nGATE 8:", "PASS" if ok else "FAIL")
sys.exit(0 if ok else 1)
