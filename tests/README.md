# Chirp Test Suite

Each chirp file in here is a conformance test in the form of a chirp script. 

## Running the suite

This test suite is meant to be *large*. We treat volume as a purely organizational/human sanity concern, not an engineering one. We strongly discourage using custom runners. Instead, we recommend using the officially supported [script](../scripts/test_conformance.py). The script will be always kept "sane" relative to the current size of the test suite.

If the script doesn't work for you as-is, feel free to open a ticket and we will work with you to accommodate your needs.

The script relies on the reference interpreter's `--run-report` mechanism. If you are writing an interpreter, we **strongly** encourage you to provide that facility verbatim so that it can be tested against this suite by the script.

Each test is an ordinary `.chirp` file with optional harness directives:

```chirp
`expect_stdout("Hello World\n");
`expect_exit(0);

`print("Hello World");
```

### Directory-Based Routing
To verify syntactical, lexical, or parse-phase failures, place the test script in a directory whose path contains `/syntax/` (e.g. `tests/syntax/`). 
* The test runner automatically expects these files to fail parsing (yielding a `syntax_failure` outcome and exiting with code `1`).

### Supported Directives
For successful compilations (VM runtime tests), you can specify expectations dynamically:

- `` `expect_stdout("..."); `` sets the exact expected stdout. Multiple directives are concatenated in file order. If omitted, stdout has no impact on the test passing/failing.

- `` `expect_exit(n); `` sets the expected process exit code of the interpreter (regardless of whether it exits via normal execution, a runtime error, or an explicit `` `exit(n) `` intrinsic). Defaults to `0` if omitted.

- `` `expect_test_failure; `` marks the test as an expected runtime failure (XFAIL). This is a TDD tool, not a way to unbreak builds. Please do not make future-me regret adding it.

Since the directives are proper chirp intrinsics, commenting them out works as expected.