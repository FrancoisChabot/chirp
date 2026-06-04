# Chirp Test Suite

Each chirp file in here is a conformance test in the form of a chirp script. 

## Running the suite

This test suite is meant to be *large*. We treat volume as a purely organizational/human sanity concern, not an engineering one. We strongly discourage using custom runners. Instead, we recommend using the officially supported [script](../scripts/test_conformance.py). The script will be always kept "sane" relative to the current size of the test suite.

If the script doesn't work for you as-is, feel free to open a ticket and we will work with you to accommodate your needs.

The script relies on the reference interpreter's `--run-report` mechanism. If you are writing an interpreter, we **strongly** encourage you to provide that facility verbatim so that it can be tested against this suite by the script.

## Test File Format

Each test is an ordinary `.chirp` file with optional harness directives:

```chirp
`expect_stdout("Hello World\n");
`expect_interpreter_exit(0);

`print("Hello World");
```

Supported directives:

- `` `expect_stdout("..."); `` sets the exact expected stdout. Multiple
  directives are concatenated in file order. If omitted, stdout has no impact on the test passing/failing.

- `` `expect_interpreter_exit(n); `` sets the expected interpreter process
  exit code. If omitted, the expected exit code is `0`, unless
  `` `expect_script_exit(n); `` is present.

- `` `expect_script_exit(n); `` expects the script to exit by calling
  `` `exit(n); ``. If `` `expect_interpreter_exit(n); `` is omitted, the
  interpreter process is expected to forward the script exit code.

- `` `expect_test_failure; `` marks the test as an expected failure. This is a TDD tool, not a way to unbreak builds. Please do not make future-me regret adding it.

Since the directives are proper chirp intrinsics, the python script will respect commenting them out.

The distinction between `expect_interpreter_exit()` and `expect_script_exit()` exists to make sure that a test that's expecting a syntactical error doesn't accidentally pass because the script mistakenly parsed well, but exited with a 1.