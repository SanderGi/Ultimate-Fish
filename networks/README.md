# Ultimate neural networks

`ultimate-2026-08-06.ufnn` is the first reproducible Ultimate-specific neural
residual checkpoint. It has 7,164 sparse inputs, 32 accumulator neurons, and a
strict version-1 binary header. Its adjacent JSON records the corpus split,
held-out error, and quantization parity. SHA-256:
`45077291352e4c620f08e4fa7f97c13e7d04c77e760ac62b994b1b44e717c557`.

The network is intentionally **not enabled by default**. It was neutral in the
fixed-node and wall-clock matches documented in
[`docs/benchmarks/ultimate-search.md`](../docs/benchmarks/ultimate-search.md)
and currently costs about 16% node throughput. Use it for continued training
and experiments with:

```bash
ULTIMATE_NNUE_FILE=networks/ultimate-2026-08-06.ufnn src/ultimatefish
```

Only promote a replacement after running Python/C++ parity, incremental/full
refresh parity, fixed-node matches, and wall-clock matches.
