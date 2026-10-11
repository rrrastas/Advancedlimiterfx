# Advanced Limiter FX

A small CLAP stereo peak-limiter prototype for macOS Intel, inspired by the purpose of foobar2000's Advanced Limiter DSP. This is an independent implementation, not an official foobar2000 component or a bit-for-bit port.

## Controls
- **Ceiling:** -12 to 0 dBFS (default -1 dBFS)
- **Release:** 5 to 500 ms (default 80 ms)
- **Lookahead:** 0 to 10 ms (default 5 ms)
- **Input Gain:** -24 to +24 dB (default 0 dB)

## Build on macOS Ventura (Intel)

Requirements: Xcode Command Line Tools, CMake 3.21+, Git, and internet access for the first build (CMake fetches the official CLAP SDK).

```sh
xcode-select --install
brew install cmake
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build --config Release
```

The resulting bundle should be `build/AdvancedLimiterFX.clap`. Install it for the current user:

```sh
mkdir -p "$HOME/Library/Audio/Plug-Ins/CLAP"
cp -R build/AdvancedLimiterFX.clap "$HOME/Library/Audio/Plug-Ins/CLAP/"
```

Restart or rescan plugins in a CLAP-compatible DAW such as Bitwig or REAPER.

## Important prototype notes
- This is source code, not a precompiled plugin binary. It must be built on macOS.
- This initial version is a peak limiter prototype. It does not claim true-peak compliance, oversampling, or exact sonic equivalence to foobar2000's DSP.
- A CLAP plugin only works in software that supports CLAP; it will not load directly in every Mac audio app.
- The host's generic parameter UI is used; no custom graphical editor is included.
- Before relying on it for mastering or distribution, validate it with the CLAP validator and test it against a range of signals. For release-quality limiting, add oversampled true-peak detection, parameter/state persistence, latency reporting, and a proper custom UI.

## License
MIT for this original project code. The CLAP SDK is fetched separately from the official free-audio/clap repository under its own license.
