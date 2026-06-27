# third_party/glosshook

bedrocklua's inline-hooking backend is [GlossHook](https://github.com/XMDS/GlossHook)
(MIT), vendored as a prebuilt ARM64 static library:

```
third_party/glosshook/include/Gloss.h
third_party/glosshook/lib/arm64/libGlossHook.a
```

These artifacts are **not committed**. Run `scripts/fetch_deps.sh` to download
them (the CI workflow does this automatically before configuring CMake):

```sh
bash scripts/fetch_deps.sh
```

`CMakeLists.txt` links `libGlossHook.a` statically, so `libbedrocklua.so` is
self-contained and does not depend on a separately built `libpreloader.so`.
