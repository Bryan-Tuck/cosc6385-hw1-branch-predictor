# COSC 6385 Assignment 1: Pentium M style hybrid branch predictor

Bryan E. Tuck, University of Houston, Fall 2026 (Prof. Weidong Shi).

`src/my_predictor.h` implements `pm_predictor`, a hybrid of a 4096-entry bimodal
table and a 4-way, 512-set, tagged global predictor indexed by the branch address
XOR a 15-bit global history, on top of the CBP2 branch prediction infrastructure.
A tag hit selects the global prediction, a miss selects the bimodal prediction and
allocates the LRU way. Only conditional branch direction is predicted; the loop
predictor, target prediction and PIR of the real Pentium M are not part of the
assignment and were not implemented. The extra credit `cpm_predictor` was not
attempted.

Average over the 20 CBP2 traces: 7.759 MPKI. Per-trace numbers and the design are
in `report/report.pdf`.

## What is included

| Path | Contents |
| --- | --- |
| `src/` | CBP2 driver (`predict.cc`, `trace.cc`, headers, `Makefile`) with the completed `my_predictor.h`. Only `my_predictor.h` was modified; the sample `gshare_predictor` in it is unchanged. |
| `run`, `run_extra` | The CBP2 csh scripts that run every trace. |
| `doc/` | The CBP2 README and documentation. |
| `results/hybrid.csv` | MPKI per trace and their mean, the numbers in Table 1 of the report. |
| `report/` | `report.pdf` and its LaTeX source (`report.tex`, `refs.bib`). |

The traces are not included. Put the 20 trace folders of the course's
`cbp2-infrastructure-v2` under `traces/` next to `src/` (for example
`traces/164.gzip/gzip.trace.bz2`).

## Building and running

The code builds with g++ on Linux (tested with g++ 11.4 under WSL Ubuntu 22.04).
From `src/`:

```bash
make
```

This produces `predict` (and `predict_extra_credit`, which uses the unimplemented
extra credit class). The build prints warnings from `trace.h` and `trace.cc` (string
constants passed as `char *`, an unused `fread` result); they come from the original
infrastructure and can be ignored. To run one trace, from the folder that holds `src/`
and `traces/`:

```bash
./src/predict traces/164.gzip/gzip.trace.bz2
```

The last line of output is the misprediction rate in mispredictions per 1000
instructions (MPKI); lower is better. To run every trace and print the average
(the script needs `csh` and `dc`; on Ubuntu, `sudo apt install csh dc`):

```bash
./run traces
```

The script truncates the average to three decimals and prints 7.758; the report
rounds the same mean, 7.75885, to 7.759.

## Rebuilding the report

`report/report.tex` compiles with `latexmk -pdf report.tex` (TeX Live or MiKTeX; it
uses natbib and bibtex).
