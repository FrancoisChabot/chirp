# 03: Capabilities and Reification

*(Preamble for the author: This chapter fits immediately after `02_core.md`. The reader now understands the 4 core primitives (Values, Types, Bindings, Sets), the `lc ⊆ fc` funnel, and how `bp`/`br` evaluate constraints. The goal of this chapter is to explain *how* those `bp`/`br` functions are attached to types, transitioning the reader from structural sets into nominal capability dispatch (Traits). After this chapter, the reader will be fully prepared for `04_machine.md`, as they will understand exactly how dynamic capability registries calcify into static dispatch tables.)*

---

At this point, we have a pristine, closed loop. Values have Types, Bindings hold Values, and Sets are values whose Types can answer belonging queries via `bp` and `br`. 

But this begs the question: **Where do `bp` and `br` actually come from?** 

When we evaluate `v ∈ S`, we know it conceptually desugars to `typeof(S).bp(S, v)`. But `typeof(S)` is just a Type value. Where does it store this `bp` function? 

We could simply just declare "every type has two optional properties", and that'd probably be fine. However, "attaching functionality to a type" is *such* a common and useful pattern that it'd be silly not to just make use of it here. Pointer-ness, yield-ness, operator overloads, etc... all operate around the same principle.

## Reifying Capabilities

To make the framework sing, we need the interpreter to manage this registry for us, in a way that guarantees it can be statically calcified by the compiler. 

To do this, we introduce the concept of **Capabilities**. 

A Capability is a named semantic contract that a type can implement. To bring Capabilities to life, the interpreter provides two core primitives: `` `mint() `` and `` `implement() ``.

### 1. `mint()`: Nominal Identity
In a structural language, it is hard to create distinct identities. If all symbols belong to the `Symbol` type, they aren't fully distinct at the type level.

`` `mint() `` is a parameter-less primitive that returns a fresh value, and simultaneously creates a **fresh, globally unique intrinsic type** for that value. 
```chirp
let final std.setness = `mint();
let final std.display_name = `mint();

`typeof(std.setness) == `typeof(std.display_name); // FALSE
```

A Capability is simply a minted identity used as a unique key for behavior.

### 2. `implement()`: Attaching Behavior
The `` `implement(Type, Capability, Contract) `` intrinsic attaches a capability to a type. 

Because the interpreter manages this attachment natively, it guarantees that when the `compile()` function takes a snapshot of the execution environment, the capability mapping is frozen and calcified.

When you write `v ∈ S` in Chirp, the compiler explicitly lowers it to:
```chirp
let impl = `capability_impl(`typeof(S), std.setness);
impl.bp(S, v);
```
If `typeof(S)` has not implemented `std.setness`, this is an error.

## Bootstrapping the Universe

By formalizing Capabilities via `mint` and `implement`, something incredible happens: the C++ interpreter's core footprint shrinks dramatically. 

Because we can attach core language behaviors to types in user-space, the C++ interpreter doesn't even need to know what `any` or `empty` are. We can bootstrap the fundamental axioms of the language inside the standard library!

```chirp
// lib/chirp/boot/01_core.chirp

// 1. Mint the unique identities for `any` and `empty`
let final `any = `mint();
let final `empty = `mint();

// 2. Imbue `any` with the Setness capability
`implement(`typeof(`any), std.setness, SetnessContract(
    bp = (this, value) => true,
    br = (this, lc) => {true},
));

// 3. Imbue `empty` with the Setness capability
`implement(`typeof(`empty), std.setness, SetnessContract(
    bp = (this, value) => false,
    br = (this, lc) => {false},
));
```

The C++ interpreter only needs to know how to execute basic AST nodes, `mint` unique identities, and dispatch `implement` lookups. The language defines its own bounds.

## Core Capabilities vs. User Traits

Capabilities and Traits are the exact same mechanism. The only difference is whether the compiler knows about them.

A **Core Capability** is an implementation hook recognized by the evaluator or compiler to desugar syntax. Examples include:
- `std.setness` (used for `∈`, constraints, and pattern matching)
- `std.yieldness` (used for `f(args)` callability on non-function values)
- `std.layout` (used during Calcification to determine machine memory representation)

A **User Trait** is built using the exact same `mint()` and `implement()` functions, but it does not invoke compiler magic. It is just a capability whose consumers are ordinary libraries. 

This keeps the ontology small while giving the language a uniform, safe extension mechanism that natively calcifies into zero-cost dispatch tables.

## Stability And Calcification

Capability implementations are part of a type's semantic behavior. During the dynamic interpretation phase, you can attach capabilities freely. 

However, Calcification enforces a strict boundary:
- Interpreted code can attach and replace capabilities dynamically.
- Calcified code sees a closed-world snapshot.
- Mutations to capabilities after Calcification do not retroactively change already-emitted machine code. 

When generating C or LLVM IR, if the compiler knows the exact `Type` entering a function, the dynamic capability dispatch (`capability_impl(T, ...).bp()`) is entirely devirtualized and inlined as a direct function call. This is how the boundless flexibility of the Interpreter maps cleanly to blistering-fast Runtime execution.