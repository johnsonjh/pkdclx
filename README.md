<!-- PKDCLX 1.1 -->
<!-- Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com> -->
<!-- SPDX-License-Identifier: MIT-0 -->
# PKDCLX

PKWARE DCL-compatible Extended DCL Implode and DCL Explode

## Overview

* PKWARE Data Compression Library Implode/Explode (method 10) compatibility
* Extended 8K/16K/32K ("ImplodeX") support
* `dclzip` command-line utility included

```
Options:
  -d, --decompress  decompress (default: compress)
  -c, --stdout      write result to standard output
  -v, --verbose     verbose status (report CRC, sizes, and ratio)
  -f, --force       overwrite existing output file
      --ascii       ASCII-literal compression mode
      --binary      binary-literal compression mode (default)
      --1k          1024-byte dictionary
      --2k          2048-byte dictionary
      --4k          4096-byte dictionary (default)
      --8k          8192-byte dictionary (extended)
      --16k         16384-byte dictionary (extended)
      --32k         32768-byte dictionary (extended)
      --extra       tighter (but slower) parsing
      --optimal     optimal (but slowest) parsing
  -h, --help        display this help
```

## Legal

* PKDCLX is distributed under the terms of the permissive
  [MIT No Attribution (MIT‑0) License](LICENSE).  While not legally required,
  giving me credit if you benefit from this code is highly appreciated.

* The PKWARE Data Compression Library Implode algorithm was protected by
  [U.S. Patent No. 5,051,745](https://patents.google.com/patent/US5051745A),
  which expired on August 21, 2010.
