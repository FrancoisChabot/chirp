# Traits, Interfaces, and Implementations

This draft replaces the older "capabilities" framing. The mechanism is still
about attaching behavior to values through their dispatch targets, but the public
ontology is now expressed in Chirp terms:

- an **interface** is a set of valid implementation values;
- a **trait** is a first-class value created from an interface;
- an **implementation** attaches one interface-conforming value to one dispatch
  target for one trait.

The word "capability" should be treated as deprecated terminology in this area.
The mechanism is traits all the way down.

---

## The Core Idea

Chirp already describes semantic validity through belonging. Trait
implementations should follow the same rule.

Given a trait `T`, its interface is the set of values that are valid
implementations for `T`:

```chirp
impl_value ∈ `interface(T)
```

Registering an implementation is therefore not a separate kind of type-system
judgment. It is a normal set-membership check followed by a registry update:

```chirp
`implement(
    trait=T,
    on=dispatch_target,
    impl=impl_value
);
```

The implementation is accepted only if:

```chirp
impl_value ∈ `interface(T)
```

Once accepted, the implementation is registered under the pair
`(T, dispatch_target)`.

---

## The Primitives

This model needs three primitive operations.

```chirp
`trait(interface)
```

Creates a fresh trait value whose implementation interface is `interface`.
The interface is itself a set. A struct type can be used as an interface because
types are sets of their instances, but the mechanism is intentionally more
general than "schema object".

```chirp
`interface(trait)
```

Returns the implementation interface carried by `trait`.

This is needed for compiler-seeded traits. User code must be able to recover the
interface for a trait such as `` `set `` even though that trait is created by the
interpreter to close the bootstrap loop.

```chirp
`implement(trait=T, on=O, impl=I)
```

Checks `` I ∈ `interface(T) ``, then attaches `I` to dispatch target `O` for trait
`T`.

The spelling `on` is intentional. It avoids implying that implementations always
attach directly to values or always attach directly to types. For ordinary value
dispatch, `on` will usually be `typeof(value)`, but making the target explicit
keeps the API honest.

---

## Traits as Sets

A trait serves as the set of values whose dispatch target implements that trait.
Conceptually:

```chirp
x ∈ SomeTrait
```

means:

```chirp
`has_implementation(trait=SomeTrait, on=`typeof(x))
```

The helper above is conceptual, not necessarily a public intrinsic. The important
point is that trait belonging is still ordinary set belonging. Trait values have
set-ness, and their belonging predicate consults the implementation registry.

This keeps user traits and compiler-recognized traits in the same ontology.
The difference is only that the compiler assigns special lowering behavior to
some trait values.

---

## Dispatch Targets

The default dispatch target for a value is its intrinsic type:

```chirp
default_dispatch_target(x) = `typeof(x)
```

That is why ordinary implementations normally look like this:

```chirp
`implement(
    trait=Display,
    on=Point,
    impl=DisplayImpl(
        display = (this) => "..."
    )
);
```

If `point ∈ Display` is evaluated, the trait's belonging predicate checks
whether `` (Display, `typeof(point)) `` has an implementation. For a `Point`
value, that target is `Point`.

The explicit `on` parameter also handles singleton-like boot values cleanly:

```chirp
let final `any = `mint();

`implement(
    trait=`set,
    on=`typeof(`any),
    impl=`interface(`set)(
        bp = (this, v) => true,
        br = (this, lc) => {true}
    )
);
```

Because `mint()` creates a fresh value with a fresh singleton type, implementing
set-ness on `` `typeof(`any) `` gives set behavior to exactly that minted value.

---

## The `set` Trait

`` `set `` is the canonical compiler-seeded trait. It is the trait whose
implementors can be used as sets in constraints, belonging tests, pattern arms,
and related set operators.

It must be seeded by the interpreter because the language needs set-ness before
guest code can fully define set-ness. However, it should be exposed as if it had
been created with `` `trait ``:

```chirp
// Conceptual shape, not final surface syntax.
let final SetnessInterface = struct {
    bp: (this, value) => {true, false, undecided},
    br: (this, lc) => `set
};

let final `set = `trait(SetnessInterface);
```

In actual boot code, user-space recovers the interface from the seeded trait:

```chirp
let final Setness = `interface(`set);
```

Then it can implement the lattice edges without importing hardcoded `any` or
`empty` values:

```chirp
let final `any = `mint();
let final `empty = `mint();

`implement(
    trait=`set,
    on=`typeof(`any),
    impl=Setness(
        bp = (this, v) => true,
        br = (this, lc) => {true}
    )
);

`implement(
    trait=`set,
    on=`typeof(`empty),
    impl=Setness(
        bp = (this, v) => false,
        br = (this, lc) => {false}
    )
);
```

This is the main bootstrap win. The interpreter no longer needs special
`AnyType` and `EmptyType` behavior forever. It needs `mint`, the seeded `` `set ``
trait, and the generic implementation registry.

---

## How Belonging Lowers

Set belonging remains the central equation:

```chirp
v ∈ S
```

For a set value `S`, the compiler/evaluator resolves the setness implementation
for `typeof(S)`:

```chirp
let impl = implementation_for(trait=`set, on=`typeof(S));
impl.bp(S, v)
```

The result is constrained by the implementation's `br` method:

```chirp
impl.bp(S, v) ∈ impl.br(S, lc_of_v)
```

The exact lookup helper does not have to be public API. The semantic requirement
is that `v ∈ S` dispatches through the implementation of the `` `set `` trait on
`typeof(S)`.

Types-as-sets are the same mechanism. If `bool` is used as a set, then `bool` is
a value whose dispatch target is `` `type ``. The setness implementation on
`` `type `` can define:

```chirp
bp = (this, v) => `typeof(v) == this
```

No separate "types are sets" rule is needed.

---

## User Traits

User traits are created with the same primitive.

```chirp
let final DisplayInterface = struct {
    display: (this) => string
};

let final Display = `trait(DisplayInterface);
```

An implementation is an ordinary value that belongs to the interface:

```chirp
`implement(
    trait=Display,
    on=Point,
    impl=DisplayInterface(
        display = (this) => "(" + this.x + ", " + this.y + ")"
    )
);
```

Consumers can use the trait as a constraint:

```chirp
let print_displayable(x: Display) = do {
    let impl = implementation_for(trait=Display, on=`typeof(x));
    `print(impl.display(x));
};
```

Again, `implementation_for` is conceptual. The public design question is whether
ordinary user code should have a reflection API for retrieving implementations,
or whether traits are primarily consumed through higher-level library functions.

---

## Compiler-Known Traits

Compiler-known traits are ordinary traits with special consumers.

Examples:

- `` `set `` powers `∈`, constraints, pattern matching, set operators, and
  `fc`/`lc` reasoning.
- A future yield trait can power call syntax on non-function values.
- A future layout trait can participate in calcification and machine
  representation.

The distinction is not ontological. It is just whether the evaluator/compiler has
hardcoded syntax or lowering rules that consult a particular trait value.

This matters for extensibility. User traits and compiler-known traits share:

- the same interface rule;
- the same implementation registry;
- the same calcification behavior;
- the same dispatch-target model.

---

## Coherence and Replacement

The default coherence rule should be strict:

```text
At most one implementation may exist for a given (trait, on) pair.
```

Calling `` `implement `` twice for the same pair should be an error unless the
language later grows an explicit override or scoped replacement operation.

This keeps trait behavior stable and makes calcification easier to reason about.
Implementations are semantic facts about dispatch targets, not casual mutable
fields.

---

## Calcification

During interpretation, the implementation registry can be built by ordinary code.
During calcification, the compiler snapshots that registry along with the rest of
the closed-world environment.

If a dispatch target is statically known, trait dispatch can lower to a direct
call to the registered implementation:

```chirp
v ∈ S
```

can lower through:

```chirp
implementation_for(trait=`set, on=`typeof(S)).bp(S, v)
```

and then to a statically selected function or inlined body when the implementation
value is known.

If a dispatch target is not statically known but is still representable, the
compiler may lower through a finite dispatch table produced from the calcified
registry.

Mutations to the interpreted registry after calcification do not change already
emitted machine code.

---

## Minimal Bootstrap Surface

This design keeps the interpreter's permanent core small:

- `` `import `` or another boot escape hatch;
- `` `typeof ``;
- `` `mint ``;
- `` `trait ``;
- `` `interface ``;
- `` `implement ``;
- compiler-seeded `` `set ``;
- the primitive values needed before they can be derived, such as `true`,
  `false`, and `undecided`.

With those pieces, `any`, `empty`, display metadata, singleton sentinels, and
most semantic hooks can move into boot code.

---

## Open Questions

- Should ordinary user code be able to retrieve implementation values directly,
  or should implementation lookup remain an evaluator/compiler operation exposed
  through libraries?
- Should `on` always be a type value, or should advanced traits be able to define
  different dispatch-target strategies?
- Should duplicate implementations be permanently forbidden, or should scoped
  override/replacement exist for metaprogramming?
- How should trait and implementation visibility interact with modules?
- Should `` `trait(interface) `` require `` interface ∈ `set ``, or should invalid
  interfaces fail later when `` `implement `` is called?
