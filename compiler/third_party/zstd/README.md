# zstd single-file decoder

`zstddeclib.c` is the single-file Zstandard decompressor from the
[facebook/zstd](https://github.com/facebook/zstd) project (BSD license).

Used by the plugin loader to decompress `.dll.zst` / `.so.zst` plugin files.

Only the **decoder** is included (~12K lines, ~30KB compiled). The full zstd
library with compression lives in `lib/std/runtime/` for use by TML programs.

To regenerate from the official repo:
```
cd facebook/zstd/build/single_file_libs
python combine.py -r ../../lib -x legacy/zstd_legacy.h -o zstddeclib.c zstddeclib-in.c
```
