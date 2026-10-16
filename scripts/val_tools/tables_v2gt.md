
### New GT (export_v2) - HELD-OUT 100 imgs

| config | GT plates w/ text | recall | exact e2e | exact (detected) | province | per-char | colour | single/double |
|---|---|---|---|---|---|---|---|---|
| x86 v2.3 (dual_hyp off) | 106 | 96.2 | 87.7 | 91.2 | 95.1 | 95.3 | 98.0 | 97.1 |
| x86 v2.4 (dual_hyp) | 106 | 96.2 | 90.6 | 94.1 | 98.0 | 98.9 | 98.0 | 99.0 |
| board v2.3 (orig det, no dual) | 106 | 94.3 | 80.2 | 85.0 | 95.0 | 94.3 | 97.0 | 98.0 |
| board v2.4 dual_hyp, INT8 det | 106 | 94.3 | 83.0 | 88.0 | 98.0 | 97.6 | 97.0 | 99.0 |
| board v2.4 dual_hyp + det U16 | 106 | 96.2 | 89.6 | 93.1 | 99.0 | 98.9 | 98.0 | 99.0 |

### New GT (export_v2) - ALL 300 imgs (150 used for det calibration)

| config | GT plates w/ text | recall | exact e2e | exact (detected) | province | per-char | colour | single/double |
|---|---|---|---|---|---|---|---|---|
| x86 v2.3 (dual_hyp off) | 317 | 96.5 | 88.6 | 91.8 | 97.1 | 96.6 | 97.7 | 96.7 |
| x86 v2.4 (dual_hyp) | 317 | 96.5 | 91.5 | 94.8 | 98.4 | 98.9 | 97.7 | 99.0 |
| board v2.3 (orig det, no dual) | 317 | 94.3 | 81.7 | 86.6 | 95.3 | 95.4 | 94.6 | 97.0 |
| board v2.4 dual_hyp, INT8 det | 317 | 94.3 | 84.2 | 89.3 | 97.0 | 97.9 | 95.0 | 99.3 |
| board v2.4 dual_hyp + det U16 | 317 | 96.5 | 90.5 | 93.8 | 98.7 | 98.8 | 97.0 | 99.0 |
