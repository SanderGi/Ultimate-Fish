# Engine baseline

The upstream baseline was captured before Chess Ultimate rule changes.

- Upstream: Fairy-Stockfish `master`
- Commit: `c19b5f6c`
- Host: Apple Silicon macOS
- Build: `make -j2 build ARCH=apple-silicon COMP=clang`
- Benchmark: `./stockfish bench`
- Time: 4,175 ms
- Nodes: 6,180,480
- Nodes/second: 1,480,354

This benchmark verifies the inherited engine build; it is not a Chess Ultimate
strength score. Ultimate Fish versions will additionally use deterministic
perft/conformance suites and fixed-opening self-play matches. A version only
becomes the new playing-strength baseline after it passes every rule fixture
and beats the previous accepted version with a statistically meaningful result.
