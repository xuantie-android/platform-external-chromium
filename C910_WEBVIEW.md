# Native C910 Android WebView153

This branch inherits Chromium153.0.8010.52 at
78e5e45d4bb41035e17ea4da2cc257f496416ac9 and contains the applied C910 changes.
No patch-apply or source-overlay step is needed. DEPS pins the corresponding
Android/Bionic FFmpeg and CPUinfo forks. The Android product consumes the signed
APK through [prebuilts-c910](https://github.com/xuantie-android/prebuilts-c910).

## Rebuilding

Use a Linux x86-64 host and the pinned depot_tools from
[webview.xml](https://github.com/xuantie-android/local_manifests/blob/lpi4a-vendor-stack-20260920/webview.xml).
Use a separate workspace, not an AOSP source subdirectory: Chromium and AOSP
have different toolchain/build requirements.

After fetching `src` and `depot_tools`, configure `.gclient` in their parent:

```python
solutions = [{"name": "src", "url": "https://github.com/xuantie-android/platform-external-chromium.git", "managed": False}]
target_os = ["android"]
target_cpu = ["riscv64"]
```

Run `gclient sync` from that parent with the pinned source checkout in place.
Use Chromium's matching LLVM24/Rust1.99/Crubit tools supplied by its hooks;
the AOSP C910 LLVM22/Rust1.93 pair cannot build these newer Crubit bindings.
This does not replace the AOSP compiler.

Use the archived Android17 sysroot and native C910 compiler-rt builtins from
the matching WebView Release assets in prebuilts-c910. Verify their published
SHA256 values. In the private Chromium toolchain only, replace
`third_party/llvm-build/Release+Asserts/lib/clang/24/lib/linux/libclang_rt.builtins-riscv64-android.a`
with the qualified archive (SHA256
dfea2e1fc4796b6608e8d3604207424ca5c8ae641e6b7c820f28f52542728486).
The upstream archive contains standard-V/Zb objects unsuitable for C910.

Generate `out/c910` using GN with:

```gn
target_os = "android"
target_cpu = "riscv64"
android_riscv_c910 = true
is_debug = false
is_component_build = false
is_official_build = false
symbol_level = 1
use_remoteexec = false
target_sysroot = "/absolute/path/to/c910-android17-sysroot"
```

Build `system_webview_apk`, `lpi4a_frame_reporter_smoke`,
`lpi4a_frame_reporting_unittests`, `lpi4a_crashpad_sanitize_smoke` and
`lpi4a_seccomp_smoke` using autoninja. The unit-test target produces an Android
native-test APK; the smoke targets are executables. The build retains normal
release optimization, DCHECKs, GPU acceleration and the renderer sandbox.
This LLVM24 backend supports scalar T-Head extensions, not XTheadVector.
Runtime-dispatched RVV1.0 functions are not selected on C910.

## Qualified changes

- Correct C910 default C++/Rust ISA flags, archived Android17 sysroot/builtins,
  and explicit per-kernel ISA override ordering.
- Native RISC-V Linux signal/clone/stat and six-argument seccomp ABI; preserve
  the existing syscall/flag restrictions rather than disabling the sandbox.
- Android FFmpeg source/config parity, Bionic CPUinfo affinity handling,
  scoped legacy audio annotations and correct ICU linkage.
- Preserve provisional compositor frame reporters for late WebView synchronous
  draws, replacement and independent main-frame promotion. Do not suppress the
  frame-ID assertion. The old code reliably reproduced an input ANR.
- Sanitize indirectly referenced Crashpad thread memory with the stack policy,
  including32-/64-bit redaction tests, instead of assuming the memory is absent.

The tested APK is153.0.8010.52/801005204, SHA256
69a7a49339e846193506981036178265a4f8b3128394daa7f8cdc94af8c61a4f.
47 frame-reporting tests,750 offline touch-scroll cycles, native sandbox/ABI
checks, JS/Canvas/PowerVR WebGL/WebGL2 and HTTPS checks passed with SELinux
Enforcing. Exceptional renderer crash collection still took about5.5s. This
does not claim full WebView CTS, a clean AOSP rebuild or cold-boot acceptance of
the resulting system image. Chromium and all included third-party licenses
remain applicable; a development signature is not a production release key.
