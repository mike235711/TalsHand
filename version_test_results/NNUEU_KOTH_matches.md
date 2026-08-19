# NNUEU King-of-the-Hill — durable match record

Reconstructed 2026-07-27 from the project memory `binpack-sweep-results` after the raw
match artifacts (JSONs, logs, wrappers, binaries) were wiped by the OS `/tmp` cleanup.
Nets re-hashed from their surviving weight CSVs for **unambiguous, verifiable identity**.

**All matches share the SAME search** — engine tag **v0.4.4** (dual-act NNUEU eval +
forward futility + LMP). Engines differ ONLY in the NNUEU net + its head dims (a binary
per net, `NNUEU_NET` set via wrapper). `scripts/version_match.py`, 4 time controls
(bullet 1+1, bullet 1+3, blitz 3+2, blitz 5+2), 64 games/rung unless stopped early.

`weights sha256[:12]` = SHA-256 over the name-sorted concatenation of each net's weight
CSVs — recompute with the same rule to verify which NNUEU a result belongs to.

## Nets
| net | width/2nd/3rd | PB | dual | atk | input | val_cp50 | weights sha256[:12] | path |
|---|---|---|---|---|---|---|---|---|
| `n256_h16x16 (DEPLOYED champion)` | 256/16/16 | 1 | True | False | 640 | n/a (old d12 data) | `849499aee760` | `~/Documents/GitHub/TalsHandThreaded/models/n256_h16x16_sq` |
| `bp_256_w0.9_l2.0_sfs_n50M` | 256/16/64 | 8 | True | False | 640 | 39.6 | `6a2bd40b47d6` | `~/Documents/GitHub/NNUEU_sweep_results/models/bp_256_w0.9_l2.0_sfs_n50M` |
| `bp_768_w0.9_l2.0_sfs_n50M` | 768/16/64 | 8 | True | False | 640 | 39.8 | `0b40821e9e03` | `~/Documents/GitHub/NNUEU_sweep_results/models/bp_768_w0.9_l2.0_sfs_n50M` |
| `bp_256_w0.8_l2.0_sfs_n150M` | 256/16/64 | 8 | True | False | 640 | 36.1 | `3031c1ddd2a6` | `~/Documents/GitHub/NNUEU_sweep_results/models/bp_256_w0.8_l2.0_sfs_n150M` |
| `bp_512_w0.8_l2.0_sfs_n150M` | 512/16/64 | 8 | True | False | 640 | 34.1 | `e769a23d46c4` | `~/Documents/GitHub/NNUEU_sweep_results/models/bp_512_w0.8_l2.0_sfs_n150M` |
| `bp_768_w0.8_l2.0_sfs_n150M` | 768/16/64 | 8 | True | False | 640 | 34.4 | `db510ab48812` | `~/Documents/GitHub/NNUEU_sweep_results/models/bp_768_w0.8_l2.0_sfs_n150M` |

## Matches (NEW = challenger, score% is from the challenger's POV; OLD = defender)

### Round 1 — 50M binpack data (2026-07-18) — champion HELD, both rejected
| rung | NEW net | OLD net | games | score% (new) | per-TC b1+1 / b1+3 / bl3+2 / bl5+2 | outcome |
|---|---|---|---|---|---|---|
| R1 | bp_256_w0.9_l2.0_sfs_n50M `6a2bd40b47d6` | n256_h16x16 (DEPLOYED champion) `849499aee760` | 46/64 (stopped early) | **41.3%** (5-28-13) | 46.9 / 37.5 / 34.6 / — | REJECTED (champion holds) |
| R2 | bp_768_w0.9_l2.0_sfs_n50M `0b40821e9e03` | n256_h16x16 (DEPLOYED champion) `849499aee760` | 39/64 (stopped early) | **32.1%** (1-23-15) | 34.4 / 25.0 / ~50 / — | REJECTED (N768 speed penalty) |

### Round 2 — 150M binpack data (3× data) (2026-07-19) — champion HELD, closer
| rung | NEW net | OLD net | games | score% (new) | Elo (95% CI) | per-TC b1+1 / b1+3 / bl3+2 / bl5+2 | outcome |
|---|---|---|---|---|---|---|---|
| R1 | bp_256_w0.8_l2.0_sfs_n150M `3031c1ddd2a6` | n256_h16x16 (DEPLOYED champion) `849499aee760` | 64/64 (complete) | **47.7%** (5-51-8) | **−16.3 ± 38.5** (lb −54.8) | 40.6 / 53.1 / 43.8 / 53.1 | champion holds (not significant) |
| R2 | bp_512_w0.8_l2.0_sfs_n150M `e769a23d46c4` | — | NOT PLAYED | — | — | — | built+validated, match not run |
| R3 | bp_768_w0.8_l2.0_sfs_n150M `db510ab48812` | — | NOT PLAYED | — | — | — | not run (speed) |

## Findings
- The **deployed net `n256_h16x16` beats the entire binpack sweep** (50M and 150M).
- **Data-scaling helps the fast net: 50M 41.3% → 150M 47.7%** (~+22 Elo) — but not to parity.
- Round-2 R1 time check: bp_256@150M reached **~1.7 ply LESS depth** (17.8 vs 19.5).
  Diagnosed: **nps 0.92× (minor), nodes-to-depth 1.74× (bushier tree)**. NOT an eval-scale
  bug (measured scale ratio R=0.99); it's an **eval-shape difference** (correlation 0.76 with
  the deployed net) → the binpack net is a genuinely different, less-search-efficient evaluator.
  **cp50 (quiet-position accuracy) ≠ search usefulness.**

## Traceability note (the gap this file fixes)
The original `version_match.py --output` JSON recorded `new_binary`/`old_binary` = the
**wrapper script paths**, NOT the `NNUEU_NET`. A result was only tied to its net via the
wrapper contents + memory notes — fragile, and lost when `/tmp` was cleaned. **Going forward:
embed the net name + weights sha256 + config in the match output, and write to this durable
`version_test_results/` dir (not `/tmp`), with `--pgn` to keep the games.**
