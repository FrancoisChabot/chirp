# Reifying Capabilities

This draft explores treating capabilities as an explicit core concept rather
than leaving them as informal "trait-like" behavior attached to types.

The immediate pressure comes from set-ness. The language already relies on
set-ness to explain constraints, `any`, `empty`, ranges, types-as-sets, match
patterns, and the meaning of `∈`. As minting moves nominal identity into guest
code, it becomes increasingly awkward for set-ness to remain an implicit
interpreter detail.

## The Fifth Concept

The current core ontology can be read as:

- **Value**: immutable data with one intrinsic type.
- **Type**: the intrinsic classifier for values, itself a value.
- **Binding**: a slot with a current value and constraints.
- **Set**: a value that can answer belonging.

Capabilities add one more explicit concept:

- **Capability**: a named semantic contract that a type can implement.

Under this model, "being a set" is not a primitive identity category. It is the
result of a type implementing the setness capability.

```chirp
v ∈ S
```

conceptually lowers to:

```chirp
let impl = `capability_impl(`typeof(S), std.setness);
impl.bp(S, v);
```

This is close to the current mental model, but it makes the indirection itself
first-class and consistent.

## Capabilities Are Values

A capability should be a value with nominal identity. Two capabilities with the
same display name are not the same capability unless they are the same value.

Sketch:

```chirp
let final std.setness = `capability();
let final std.display_name = `capability();
let final std.layout = `capability();
```

The spelling is intentionally provisional. The important point is that
capability identity is not stringly-typed. Display names, documentation, and
debug labels can be metadata attached to the capability value, just like they
can be metadata attached to ordinary minted types.

## Implementations

A type implements a capability by associating that capability with an
implementation value.

Sketch:

```chirp
`implement(T, std.setness, Setness(
    bp = (this, value) => ...,
    br = (this, lc) => ...,
));
```

For simple metadata capabilities, the implementation can be plain data:

```chirp
`implement(T, std.display_name, "MyType");
```

For behavioral capabilities, the implementation is a structured value whose
shape is defined by the capability contract. Setness, for example, needs both
`bp` and `br`.

## Setness As The First Core Capability

Setness is the capability that lets a value act as a set:

```chirp
let Setness = struct {
    bp: (this: any, value: any) => bool,
    br: (this: any, lc: `set) => `set,
};

let final std.setness = `capability(Setness);
```

The exact contract syntax is open. What matters semantically:

- `bp` answers whether a value belongs to this set value.
- `br` predicts the set of possible belonging answers for a candidate local
  constraint.
- `br` must return a subset of `{true, false, undecided}`.

Then:

```chirp
`set = { v | `has_capability(`typeof(v), std.setness) };
```

That makes `set` a derived concept: the set of values whose type implements
setness.

## Guest-Side `any` And `empty`

This pairs cleanly with minting.

```chirp
let final `any = `mint();
let final `empty = `mint();

`implement(`typeof(`any), std.setness, Setness(
    bp = (this, value) => true,
    br = (this, lc) => {true},
));

`implement(`typeof(`empty), std.setness, Setness(
    bp = (this, value) => false,
    br = (this, lc) => {false},
));
```

With minting plus capability implementation, `any` and `empty` no longer need to
be interpreter-created values. They become ordinary boot definitions with
ordinary guest-defined behavior.

The interpreter still needs enough primitive machinery to run this boot code,
but the semantic surface gets smaller:

- create fresh nominal values/types (`mint`)
- create or expose capability identities
- attach capability implementations to types
- invoke the small set of core capabilities that language operators depend on

## Capabilities Vs Traits

Capabilities and traits are closely related, but they should not be casually
collapsed.

A **core capability** is an implementation hook recognized by the evaluator,
compiler, or standard lowering rules. Examples:

- `std.setness` for `∈`, constraints, set operators, and match patterns.
- `std.yieldness` or callability for `value(args...)`.
- `std.layout` or representability for calcification and code emission.
- `std.display_name` for diagnostics and debugging.

A **user trait** can be built on top of the same machinery, but it does not
automatically become language magic. It is just a capability whose consumers are
ordinary libraries unless an operator or compiler rule explicitly depends on it.

This keeps the ontology small while still giving the language a uniform
extension mechanism.

## Operator Lowering

Once capabilities are explicit, operator semantics can be written in a uniform
style.

Belonging:

```chirp
a ∈ b
```

lowers through `std.setness` on `typeof(b)`.

Calling:

```chirp
f(x)
```

can lower through `std.yieldness` on `typeof(f)` when `f` is not a direct
function value.

Field access, indexing, iteration, display, serialization, and layout can be
evaluated the same way if those operations need open-world extensibility.

The design constraint is that every operator using a capability must specify:

- which capability it requires
- which type receives the implementation lookup
- what implementation contract is expected
- whether failed lookup is a runtime error, compile-time error, or ordinary
  false/empty behavior

## Stability And Mutation

Capability implementations are part of a type's semantic behavior. That raises
the same open-world versus compiled-world tension as traits.

During interpretation, capability implementation may be dynamic:

```chirp
`implement(T, std.setness, impl);
```

During calcification, the compiler must snapshot the relevant capability tables.
Code emission can then lower capability dispatch to direct calls where the
receiver type is known.

This suggests a practical rule:

- interpreted code can attach and replace capabilities according to normal
  binding/module rules;
- calcified code sees a closed-world snapshot;
- mutations after calcification do not retroactively change already-emitted
  code.

## Open Questions

- Is `` `capability() `` itself needed, or can capabilities be ordinary minted
  values with a required metadata contract?
- Should every capability declare an implementation shape, or can some
  capabilities accept arbitrary values?
- Are capability implementations mutable, additive, or final once attached?
- Can a type implement multiple variants of a capability, or is capability
  identity enough namespacing?
- Are capabilities attached only to types, or can individual values also carry
  capabilities?
- Which capabilities are core enough for the evaluator/compiler to recognize
  directly?

The current leaning is that capabilities should be core as a mechanism, while
specific capabilities should be introduced sparingly. Setness is the obvious
first one because it is already central to the language's semantics.
