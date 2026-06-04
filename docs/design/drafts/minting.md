# Minting Nominal Singletons

This draft captures a narrow primitive for creating fresh nominal singleton
values, and explains how that primitive can eventually reduce the interpreter's
bootstrap surface.

## The Primitive

`` `mint() `` creates one fresh value and one fresh intrinsic type for that
value:

```chirp
let final None = `mint();

None ∈ `typeof(None);      // true
`typeof(None) ∈ `type;     // true
```

The minted value is the only value that belongs to its type. In other words,
`` `typeof(None) `` is a singleton set by construction.

The primitive is deliberately parameterless:

```chirp
let a = `mint();
let b = `mint();

a == a; // true
a == b; // false
```

No string, symbol, or source name participates in identity. If a type should
have a display name, documentation string, serialization policy, debugger
label, or other metadata, that information should be attached through the same
general capability or metadata channel used for type behavior. Naming is an
observability concern, not part of the minted value's semantic identity.

## Why This Is Not `symbol`

Symbols are first-class constants, but all symbols share the same intrinsic
type:

```chirp
`typeof(#pending) == `typeof(#done); // true
```

That is exactly right for lightweight tags and match arms. Minting serves a
different purpose: it creates a value whose identity is reflected in the type
system.

```chirp
let final Pending = `mint();
let final Done = `mint();

`typeof(Pending) == `typeof(Done); // false
```

This makes minted values useful as nominal sentinels, module-private capability
tokens, option/result variants before richer algebraic forms exist, and other
places where "this exact distinguished value" should be usable as a constraint.

## Guest-Side `any` and `empty`

The important bootstrap consequence is that `any` and `empty` do not need to be
interpreter-created values forever. Once guest code can attach set-ness to a
type, the interpreter only needs to provide minting and the behavior-injection
mechanism.

Sketch:

```chirp
let final `any = `mint();
let final `empty = `mint();

`typeof(`any).implement(std.setness, Setness(
    bp = (this, value) => true,
    br = (this, lc) => {true},
));

`typeof(`empty).implement(std.setness, Setness(
    bp = (this, value) => false,
    br = (this, lc) => {false},
));
```

The exact spelling of `implement`, `std.setness`, and `Setness` is intentionally
left open. The design point is that `mint()` provides only nominal identity.
Universal and empty-set behavior belongs to guest-defined set-ness.

This keeps the interpreter surface small:

- `import` or equivalent boot escape hatch
- `mint()`
- a way to attach capabilities/metadata to types
- enough primitive evaluation machinery to run the boot prelude

With those pieces, `any`, `empty`, and their specialized belonging behavior can
be ordinary boot definitions rather than hardcoded interpreter values.

## Metadata And Display

Because `mint()` is parameterless, minted values may initially print with an
implementation-generated identity:

```text
<mint 42>
```

Readable names can be layered on later:

```chirp
let final None = `mint();

`typeof(None).implement(std.display_name, "None");
```

The display name is metadata. It must not affect equality, belonging, hashing,
or type identity.

## Open Questions

- Should repeated evaluation of the same top-level boot declaration produce a
  stable identity for caching and compiled artifacts, or should identity be
  purely evaluation-generative?
- How should minted identities be represented in run reports, debugging output,
  and serialized metadata?
- Should `mint()` be available to all user code, or only to boot and trusted
  metaprogramming scopes?

The current leaning is that `mint()` should be user-facing but advanced. It is
too useful for nominal sentinels and capability tokens to hide completely, but
it should remain a small intrinsic rather than grow constructor-like naming or
metadata parameters.
