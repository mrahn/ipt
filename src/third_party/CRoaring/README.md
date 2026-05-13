# CRoaring (vendored)

This directory contains the single-file amalgamation of
[CRoaring](https://github.com/RoaringBitmap/CRoaring), used as an
external baseline for the IPT benchmark
(`benchmark.cpp`, baseline name `roaring`).

- Upstream: <https://github.com/RoaringBitmap/CRoaring>
- Version:  v4.6.1
- Files:    `roaring.c`, `roaring.h`, `roaring.hh`
- License:  dual Apache-2.0 / MIT (see `LICENSE`)
- Vendored unmodified on 2026-04-29.

The amalgamation is dropped into the benchmark build as-is: the
benchmark translation unit `benchmark.cpp` includes `roaring.hh`, and
the build script links `roaring.c` from this directory. Apart from the
vendored CRoaring sources and this README, the IPT repository itself
remains BSD 3-Clause licensed (see the top-level `LICENSE`).

To refresh, replace the three amalgamation files with the latest
release-asset versions from the upstream releases page and update the
version line above.
