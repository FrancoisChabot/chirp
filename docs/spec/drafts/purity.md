# Purity (Draft)

This chapter outlines the draft specification for function purity in Chirp, its static and dynamic analysis rules, the available user-facing constructs, and mechanisms to explicitly circumvent purity checking.

## Overview

In Chirp, purity analysis helps ensure that functions do not produce observable side-effects. A function is considered **pure** if evaluating it does not mutate any state outside of its local scope and does not perform unpure I/O operations.

Purity is currently evaluated recursively as a property of a `LambdaExpr` AST node.

## Purity Rules

A function is classified as **unpure** if its body contains any of the following:

1. **Non-Local Mutations**: Assigning a new value to a binding that was defined outside the function's local scope (including modifying global or captured variables).
2. **Unpure Intrinsic Calls**: Invoking intrinsics that explicitly perform I/O or observable side-effects. For example, `` `print `` is flagged as an unpure intrinsic. (Note: Specific testing intrinsics like `` `expect `` are considered pure to facilitate test suites without polluting purity analysis).
3. **Unpure Function Calls**: Invoking any function or lambda that is not statically or dynamically verifiable as pure. If the purity of a called function is unknown or unverifiable, it is conservatively treated as unpure.

If none of the above are present, the function is classified as **pure**. 
*Note: The current definition of purity focuses on side effects and may be further refined once formal lambda capturing semantics are finalized.*

## User-Facing Bootstrap Identifiers

Chirp's standard bootstrap (`lib/chirp/boot/00_fundamental.chirp`) exposes functionality to interact with the interpreter's purity analysis.

### `` `is_pure ``
A function that takes a single callable argument and returns a boolean indicating whether the given function is pure. It wraps the internal `` `is_pure_func `` intrinsic, which recursively evaluates the lambda's AST for side-effects and memoizes the result.

```chirp
let pure_identity = (x) => x;
let unpure_print = (x) => `print(x);

`is_pure(pure_identity); // true
`is_pure(unpure_print);  // false
```

### `` `pure_fn ``
A constraint provided by the bootstrap, defined mathematically as `{ v: `yielder | `is_pure(v) }`. It can be used as a binding constraint to guarantee that a function parameter or variable is strictly pure.

```chirp
let call_pure = (f: `pure_fn, x) => f(x);

call_pure((x) => x + 1, 5); // OK
call_pure((x) => `print(x), 5); // Runtime constraint failure
```

## `debug` Blocks

Chirp provides D-style `debug` blocks as an explicit "escape hatch" to circumvent purity checks. Operations placed inside a `debug` block are completely ignored by the interpreter's purity analysis, allowing side-effects (like logging or debugging metrics) inside an otherwise pure function.

```chirp
let pure_with_debug = (x) => do {
    let y = x * 2;
    debug {
        `print("Debugging y: ");
        `print(y);
    }
    y
};

`is_pure(pure_with_debug); // Returns true!
```

### Rules for `debug` Blocks:
1. **Statement Only**: A `debug` block is a statement (`DebugStmt`), not an expression. It does not yield a value and cannot be assigned to bindings or used within expression contexts.
2. **No Top-Level Declarations**: You cannot use `let` statements directly inside a `debug` block. Doing so produces a syntax error. This constraint clarifies the ambiguity around whether a `debug` block introduces a distinct lexical scope (it does not create a new scope for bindings).
    * *Note:* While top-level declarations are forbidden, you can still mutate variables from the enclosing scope.
3. **Purity Exemption**: The compiler/evaluator strictly treats the `debug` block as pure, shifting the responsibility of "User-Beware" to the developer.
