# The Chirp Specification: The Computation Model

With the static ontology of Chirp established—Values, Types, Sets, and Bindings—we can now introduce the execution engine that brings these elements to life. 

Computation in Chirp is not modeled as standard imperative AST traversal. Instead, it is modeled as the recursive, coinductive resolution of active computations until they settle into stable values.

---

## 1. Computation and the Yielder

In Chirp, all dynamic calculation is represented by a simple, elegant set relationship:

```text
computation ≜ terminal ∪ yielder
```

*   A **terminal** is a value that is already a completed result in the current evaluation context. The set of terminal values is therefore contextual (for instance, inside a standard math expression, a raw integer is terminal).
*   A **yielder** is a value whose Type has **yield-ness**: the capability to advance a computation by producing another computation.

### Coinductive Evaluation

Terminal-ness and yield-ness are not mutually exclusive. A value may be both terminal and yielder. In that case, terminal-ness wins unless the surrounding operation explicitly asks for the value to be yielded. This is what allows a function, matcher, or other executable value to be handled as an ordinary, passive value in one context, and as an active computation in another.

For a Type to have **yield-ness**, it must fulfill three criteria:

1.  **Parameter Domain:** There has to be an operation that provides the parameter domain (as a set) of the value.
2.  **Result Domain:** There has to be an operation that provides the result domain (as a set of computations) of the value *as a function of a subset of the parameter domain*.
3.  **Yield Function:** There has to be an operation that, given a value of a subset of its parameter domain, yields a computation of the corresponding result domain.

---

## 2. Conceptual Evaluation Flow

Evaluation is conceptually a recursive loop that continuously resolves yielders:

```text
evaluate(c, p):
  if c ∈ terminal(p):
    return c

  if c ∈ yielder:
    return evaluate(yield(c, p), p)
```

This is only a semantic sketch, not surface syntax. The crucial point is the exit condition: a yielder does not need to produce a final, terminal value in one step. It only needs to produce *another computation*, and the process continues recursively until that computation evaluates to a terminal value.

---

## 3. Unifying Expressions, Functions, and Blocks

By modeling all execution as a generalized yielder-resolution loop, Chirp unifies language constructs that are normally treated as entirely separate concepts in other compilers:

*   **Functions & Closures:** Traditional callables that map a parameter domain to a result domain.
*   **Expressions:** Lazy calculations whose parameter domain is simply the current evaluation scope, yielding their evaluated result.
*   **Code Blocks:** Ordered sequences of computations that yield the result of their final statement.
*   **Matchers (Pattern Matching):** Piecewise functions that map matching subjects (parameter domain) to their corresponding branch outputs.

All of these share the same fundamental mechanism: they map a parameter domain to a result domain, step-by-step, until a terminal result is produced.

---

## 4. Next Steps

Now that we have covered the static core and the dynamic computation model, we are ready to cross the boundary into physical memory layouts and hardware compilation.

Next up: [Lexical](05_lexical.md), Where we begin to start expressing this architecture as concrete code.
