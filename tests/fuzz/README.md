# Fuzzing

Two [libFuzzer](https://llvm.org/docs/LibFuzzer.html) targets, built with
AddressSanitizer and UndefinedBehaviorSanitizer:

| Target | What it checks |
|---|---|
| `fuzz_text` | `isValidUtf8Text` accepts exactly what `g_utf8_validate` accepts (the gate before text reaches GTK); `usesCrlfLineEndings` and `replaceAllInText` against simple reference implementations |
| `fuzz_config` | Arbitrary bytes as `config.json`: `Config::load` never throws, keeps only valid UTF-8 paths, and a save/reload round trip is lossless |

```bash
CC=clang CXX=clang++ cmake -B build-fuzz -DCMAKE_BUILD_TYPE=Debug -DPLUMAS_BUILD_FUZZERS=ON
cmake --build build-fuzz --target fuzz_text fuzz_config

mkdir -p corpus-text corpus-config
./build-fuzz/fuzz_text   -max_total_time=600 -max_len=4096 corpus-text
./build-fuzz/fuzz_config -max_total_time=600 -max_len=4096 corpus-config
```

A crash leaves a `crash-<hash>` file; reproduce it with
`./build-fuzz/fuzz_text crash-<hash>`. CI runs each target for 60 seconds on
every push (`fuzz` job in `.github/workflows/ci.yml`).
