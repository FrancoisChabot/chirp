# Chirp Test Suite

Each chirp file in here is a conformance test in the form of a chirp script. 

## Scope of the suite

This test suite tests assumes a compliant interpreter that's using the standard `lib/chirp/boot` bootstrap. We may carve out a bootstrap-agnostic subset eventually, but we won't until the need arises, so speak up if you need it.

## Running the suite

This test suite is meant to be *large*. We treat volume as a purely organizational/human sanity concern, not an engineering one. We strongly discourage using custom runners. Instead, we recommend using the officially supported [script](../scripts/test_conformance.py). The script will be always kept "sane" relative to the current size of the test suite.

If the script doesn't work for you as-is, feel free to open a ticket and we will work with you to accommodate your needs.

The script relies on the reference interpreter's `--run-report` mechanism. If you are writing an interpreter, we **strongly** encourage you to provide that facility verbatim so that it can be tested against this suite by the script.

Each test is an ordinary `.chirp` file with optional harness directives:

```chirp
`expect_exit(0);

let value = 1 + 2;
`expect(value == 3);
```

### Directory-Based Routing
To verify syntactical, lexical, or parse-phase failures, place the test script in a directory whose path contains `/syntax/` (e.g. `tests/syntax/`). 
* The test runner automatically expects these files to fail parsing (yielding a `syntax_failure` outcome and exiting with code `1`).

### Supported Directives
For successful compilations (VM runtime tests), you can specify expectations dynamically:

- `` `expect_stdout("..."); `` appends to the exact expected stdout. Prefer placing stdout expectations next to the statements or control-flow blocks that produce them. If omitted, stdout has no impact on the test passing/failing.

- `` `expect(expr); `` evaluates `expr`, requires it to be `true`, and reports a runtime failure otherwise. Prefer this for semantic checks that do not need to verify printed output.

- `` `expect_exit(n); `` sets the expected process exit code of the interpreter (regardless of whether it exits via normal execution, a runtime error, or an explicit `` `exit(n) `` intrinsic). Defaults to `0` if omitted.

- `` `expect_test_failure(); `` marks the test as an expected runtime failure (XFAIL). This is a TDD tool, not a way to unbreak builds. Please do not make future-me regret adding it.

Since the directives are ordinary Chirp calls, commenting them out works as expected.
