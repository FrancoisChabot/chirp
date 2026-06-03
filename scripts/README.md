# Developer Scripts

## Test Conformance Runner

`test_conformance.py` is the runner for the chirp conformance test suite.

### Running

From the repository root:

```bash
python3 scripts/test_conformance.py path/to/chirp
```

The bootstrap interpreter already has that hooked up as the `chirp_e2e_tests`, and will run it if you invoke ctest.
  
**N.B.** If/When the conformance suite starts being too big/heavy, that last bit will probably become an opt-in.
