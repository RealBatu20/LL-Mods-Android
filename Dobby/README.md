# Dobby (prebuilt static libs)

The build links against a prebuilt Dobby static library per Android ABI:

```
Dobby/arm64-v8a/libdobby.a
Dobby/armeabi-v7a/libdobby.a   (optional)
```

These binary artifacts are **not** committed to the repository. The CI workflow
(`.github/workflows/build.yml`) downloads a release build of Dobby and places
`libdobby.a` into `Dobby/${ANDROID_ABI}/` before invoking CMake.

To build locally, either:

1. Download a prebuilt `libdobby.a` for `arm64-v8a` and drop it at
   `Dobby/arm64-v8a/libdobby.a`, or
2. Build Dobby from source for the Android target:

   ```sh
   git clone https://github.com/jmpews/Dobby.git
   cd Dobby
   cmake -B build \
     -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
     -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 \
     -DDOBBY_GENERATE_SHARED=OFF
   cmake --build build
   cp build/libdobby.a /path/to/repo/Dobby/arm64-v8a/libdobby.a
   ```

`CMakeLists.txt` imports it as the `dobby` IMPORTED target.
