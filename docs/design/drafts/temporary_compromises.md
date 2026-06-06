# Temporary Compromises

This document tracks implementation shortcuts that are intentionally narrower than
the intended language semantics. These notes are not final specification text.
They exist so temporary interpreter behavior does not quietly harden into the
wrong model.

## Anonymous Struct Literals

Anonymous struct literals are planned as context-sensitive sugar, not as
first-class anonymous records.

The temporary v1 rule is:

```chirp
let Point = struct { x: int, y: int };
let p : Point = { x=1, y=2 };
```

For now, `{x=1, y=2}` is legal only when the interpreter can directly recover
one expected nominal struct type from the surrounding context. The literal is
then evaluated as if the programmer had written the corresponding named struct
constructor.

Supported v1 contexts should include:

- constrained bindings, such as `let p : Point = {x=1, y=2}`;
- assignment to a binding whose fundamental constraint is a direct struct type;
- function and lambda arguments whose parameter bound is a direct struct type,
  such as `foo({x=1, y=2})`;
- struct constructor fields whose field bound is a direct struct type;
- lambda returns whose return bound is a direct struct type;
- `` `implement ``'s `impl` argument when the selected trait interface is a
  direct struct type, such as `` `Dropness ``.

This v1 behavior has two deliberate limits:

- `{}` remains the empty enumerated set. It is not a struct default literal.
- Anonymous struct literals are not first-class values and do not introduce
  anonymous record types.

### Intended Rule

The final rule should be expressed in terms of the compiler operation needed
for code emission:

```text
possible_types(binding_or_constraint) -> finite set of type values
```

Anonymous struct literal resolution should eventually:

1. Compute the possible represented types for the contextual binding or
   constraint.
2. Filter that result to struct types.
3. If exactly one struct type remains, desugar the literal to that struct
   constructor.
4. If zero struct types remain, reject the literal because there is no struct
   target.
5. If more than one struct type remains, reject the literal as ambiguous.

The direct-`StructType` rule is only a small interpreter approximation of this
future `possible_types` rule.
