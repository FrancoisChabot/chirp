# The Chirp Specification: The Machine Model

Chirp is a semantic constraint language, meaning its execution model fundamentally splits into two domains: the boundless, mathematically pure **Interpreter Environment**, and the rigidly structured, physically bounded **Runtime Environment** (machine code).

This chapter outlines how the core ontology—Values, Types, Sets, and Bindings—maps to physical CPU architecture and memory when generating systems code.

---

## 1. Representable Types

During standard evaluation (the "Interpreter" phase), Chirp has no limitations on what a binding can hold. A binding's value can be a `Type`, a `Matcher`, a module, or even an infinite mathematical `set`. 

However, CPUs do not understand abstract mathematics. To emit machine code (like C or LLVM IR), Chirp relies on the concept of **Representable Types**. 

A Representable Type is any Type whose instances have a known, finite, and deterministic physical memory layout. 

The core representable types are:
* **Scalars:** Primitive machine words like `int`, `char`, `bool`.
* **Structs:** Contiguous blocks of memory composing other representable types.
* **Tagged Unions:** A layout consisting of a type tag followed by a union of component representable type layouts.
* **Memory Locations (Pointers):** Machine addresses.
* **Ranges:** Slices or spans of contiguous memory locations.

Non-representable types include concepts like `Type`, `Matcher`, and `Trait`. These exist purely at the interpreter level to orchestrate the construction of your program.

---

## 2. The Golden Rule of Code Generation

The bridge between the Interpreter Environment and the Runtime Environment is governed by a single, unbreakable rule:

> **Any binding that participates in dynamic runtime behavior MUST have its constraint reduced to a set containing only values with representable types.**

When you invoke compilation (e.g., `std.compile()`), the compiler initiates a **Calcification** phase. It takes a closed-world snapshot of the execution environment and analyzes the flow of constraints through the AST of the target entrypoint. 

If a binding used in the compiled execution path can only be proven down to a non-representable type (or an open set like `any` that *could* contain a non-representable type), calcification fails and compilation halts. The compiler will not—and mathematically cannot—emit code for abstract mathematical concepts.

## 3. Memory Locations and Mutability

N.B. [pointers.md](../drafts/pointers.md) will probably replace this once it's been finalized.

Because Chirp's core ontology dictates that "Values are immutable," imperative programming and state mutation are introduced via physical **Memory Locations**.

A Memory Location is a runtime physical container that holds a Value. Mutating state does not alter the Value itself; rather, it replaces the Value residing within the Memory Location.

### The Pointer Set Operator (`->`)

To express memory locations, Chirp introduces the pointer-set operator `->`. 

If `S` is a set of representable values, then `->S` is the set of all Memory Locations that contain a value belonging to `S`.

```chirp
let ptr : ->int = &x; // ptr is a memory location pointing to an integer
``` 

### Pointer Dereferencing and Ranges

* **Dereferencing (`*ptr`):** Accesses the Value currently residing at the memory location. The compiler guarantees at calcification time that the fetched Value is a member of the location's constraint set.
* **Ranges:** A contiguous slice of Memory Locations. Ranges allow arrays and buffers to be represented safely without decaying to raw, unconstrained addresses.

### Mutability Across Function Boundaries

Because bindings are normally passed by value, passing a large struct to a function will copy its value. To perform zero-copy mutations across function boundaries, you must pass a Memory Location (`->my_struct`). When the called function assigns a new value to the dereferenced location, the caller observes the change.

### Pointer Aliasing and Refinement Locking

Because pointer aliasing allows a binding's underlying value to be mutated through an indirect memory location, allowing static flow-sensitive refinement on aliased bindings would create a type-system soundness hole. 

To guarantee soundness, Chirp enforces the **Refinement Locking Rule:**

> If a mutable address of binding `x` is taken as `->S`, then for the lifetime of that alias, flow-sensitive refinements of `x` are bounded by the alias’s write capability. Before any possible write through the alias, `x.lc` may remain as narrow as `x.lc_at_escape ∩ S;` after any possible write through the alias, `x.lc` must be widened to at least `S ∩ x.fc`. Multiple active aliases combine by union for possible writes, not intersection.


Consider the following example:

```chirp
let mut x : {1, 2, 3} = 2;
let x_ptr : ->{1, 2, 3} = &mut x; // x's address is taken; refinement is locked!

let mutate_under_the_hood(x_ptr : ->{1, 2, 3}) = {
    *x_ptr = 1; // mutates x to 1 via the pointer
}

if x != 1 {
    // Under standard occurrence typing, x's lc would be refined to {2, 3}.
    // However, because x_ptr is an active alias, x's lc is locked to {1, 2, 3}.
    
    mutate_under_the_hood(x_ptr); 
    
    // This assignment is a compile-time error! 
    // x's lc is {1, 2, 3}, which is not a subset of the target constraint {2, 3}.
    let y : {2, 3} = x; 
}
```

This conservative approach ensures absolute soundness and compiler implementation simplicity while leaving the future design space open to select optimizations (e.g., affine/borrow checks) in a fully backward-compatible manner.

---

## 5. Summary

By strictly separating the boundless Interpreter environment from the bounded Runtime environment through the **Golden Rule of Code Generation**, Chirp achieves an elegant balance. It allows maximum dynamic flexibility for meta-programming and trait injection, while guaranteeing that the emitted machine code (often lowered to C) is deterministic, aligned, and blistering-fast.

---

## Next Steps

With the core mechanics, the evaluation engine, and the physical machine representation fully specified, we have a complete picture of the semantics of Chirp.
