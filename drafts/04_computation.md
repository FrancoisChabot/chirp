# The Chirp Specification: The Computation Model

With the static ontology of Chirp established -- Values, Types, Sets, and Bindings -- we can describe how dynamic calculation settles into values.

Computation in Chirp is not modeled as ordinary imperative AST traversal. It is modeled as the recursive resolution of active computations until the current evaluation context can treat the result as terminal.

---

## 1. Computation and Yielders

In Chirp, dynamic calculation is represented by a set relationship:

```text
computation := terminal union yielder
```

A **terminal** is a value that is already a completed result for the current evaluation context. Terminal-ness is contextual: a raw integer may be terminal for arithmetic, while a callable value may remain passive unless the surrounding operation asks it to yield.

A **yielder** is a value whose Type provides yield-ness: the capability to advance a computation by producing another computation.

Terminal-ness and yield-ness are not mutually exclusive. If a value is both terminal and yield-capable, terminal-ness wins unless the surrounding operation explicitly invokes yield behavior.

For a Type to provide yield-ness, it must supply:

1. **Parameter Domain:** the set of accepted parameters.
2. **Result Domain:** the set of computations produced for a subset of the parameter domain.
3. **Yield Function:** the operation that maps a value plus parameters to the next computation.

---

## 2. Conceptual Evaluation Flow

Evaluation is conceptually a recursive loop:

```text
evaluate(c, p):
  if c is terminal in context p:
    return c

  if c has yield-ness:
    return evaluate(yield(c, p), p)
```

This is a semantic sketch, not surface syntax. A yielder does not need to produce a final value in one step. It only needs to produce the next computation, and evaluation continues until the result is terminal for the active context.

---

## 3. Unified Computation Values

Several language-level concepts can be understood as values with yield-like behavior:

- Functions and closures map a parameter domain to a result domain.
- Expression computations depend on the current evaluation scope and yield an evaluated value.
- Block-like computations sequence work and yield according to their semantic rules.
- Matcher-like computations model piecewise mappings from accepted inputs to branch outputs.

The exact surface syntax for constructing these values belongs in the grammar, not in this semantic chapter.

---

## 4. Next Steps

This chapter only describes the computation model. Concrete lexical and grammar details are maintained separately in [05_lexical.md](05_lexical.md) and [grammar.md](grammar.md).
