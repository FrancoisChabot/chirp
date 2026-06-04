# Chirp Test Suite

Each chirp file in here is a conformance test in the form of a chirp script. 

For instructions on how to run the test suite, check [`scripts/README.md`](../scripts/README.md).

Tests are grouped by behavior in subdirectories such as `binding/`, `expr/`,
`function/`, `set/`, `smoke/`, and `string/`. The runner discovers `.chirp`
files recursively and reports failures by relative path.

## Test File Format

Each test is an ordinary `.chirp` file with optional harness directives:

```chirp
`expect_stdout("Hello World\n");
`expect_exit(0);

`print("Hello World");
```

Supported directives:

- `` `expect_stdout("..."); `` sets the exact expected stdout. Multiple
  directives are concatenated in file order. If omitted, stdout has no impact on the test passing/failing.

- `` `expect_exit(n); `` sets the expected process exit code. If omitted, the
  expected exit code is `0`.

- `` `expect_test_failure; `` marks the test as an expected failure. This is a TDD tool, not a way to unbreak builds. Please do not make future-me regret adding it.

Since the directives are proper chirp intrinsics, the python script will respect commenting them out.
