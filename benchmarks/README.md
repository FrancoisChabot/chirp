# Benchmarks

Run one benchmark:

```bash
python3 benchmarks/run_bench.py fibonacci --root-dir lib/chirp_vm
```

Run the current suite:

```bash
python3 benchmarks/run_bench.py all --root-dir lib/chirp_vm
```

List available benchmarks:

```bash
python3 benchmarks/run_bench.py all --list
```

Current coverage:

- `startup`: process startup and minimal boot cost
- `fibonacci`: recursive calls, branching, and integer math
- `block_scope`: nested block scopes and local bindings
- `closures`: closure creation, capture, and invocation

For now, `lib/chirp_vm` is the practical root for suite runs because it keeps the VM on the subset it currently supports.
