# Dynamic Allocation, RAII, and Moves

This note captures the current design consensus for handling dynamic allocations (`Box`, `unique_ptr`) and RAII within Chirp's core semantics.

## The Core Conflict

Chirp's ontology relies on flow-sensitive bindings (`fc`/`lc`) and immutable chronological values (`cv`). In this system:
- **Values are immutable.** A `Box` is simply an immutable package of data containing an address in some address space.
- **Assignment is trivial.** `let b = a` simply takes the `cv` of `a` and makes it the `cv` of `b`.
- **Bindings always have a value.** A binding must always hold a valid `cv` belonging to its `lc`. 

If we allow trivial assignment of a `Box`, we duplicate the address, breaking the uniqueness invariant and causing double-frees. However, if we allow "destructive reads" mid-scope (where `a` is moved to `b`), `a`'s `lc` would have to dynamically widen to accommodate a `void` or "moved-from" tombstone state. This severely complicates static analysis and calcification.

## The Solution: Terminal Move Semantics

To preserve the elegance of Chirp's semantic physics while providing strict, zero-abstraction unique ownership, Chirp adopts **Terminal Move Semantics**. 

The model avoids mid-scope tombstones entirely by severely restricting *when* a non-copyable value can be moved.

### 1. Copying is Opt-Out
Because all values in Chirp are immutable, trivial copying is the **default** behavior for the vast majority of types (`int`, `bool`, flat structs, etc.). 

A type must explicitly **opt-out** of copyability. 
The most natural way this happens is by implementing the `` `drop `` capability. Because trivial assignment cannot execute code (it simply duplicates the value), trivially copying a droppable type would guarantee a double-free. Therefore, the compiler structurally forbids trivial copying for any type that implements `` `drop `` (like `Box`). Types can also manually opt-out by implementing a `` `unique `` trait for things like hardware capability tokens.

Attempting to evaluate an identifier bound to an uncopyable type in a standard expression (e.g., `let a = b;` or `foo(b)`) is an automatic compiler error.

### 2. The Terminal Evaluation Exception
The *only* time a non-copyable value can be evaluated and transferred is when that evaluation is the **terminal action of its lexical scope** (an RVO or expiring value scenario).

```chirp
let a = do {
    let b = Box.new();
    // ... use b ...
    break b; // OK: b's lexical scope is closing.
};
```

Because the compiler knows `b` is dying at the exact moment of evaluation, it intercepts `b.cv`, routes it out of the block, and skips the drop sequence for `b`.

### 3. Trivial, Tombstone-Free Destruction
Because non-copyable values cannot be moved mid-scope, a binding's Local Constraint (`lc`) never has to dynamically track a `void` or `uninit` state. The binding safely holds its type until the closing brace.

This makes automatic cleanup extremely deterministic:
1. We introduce a `` `drop `` capability.
2. At the end of any lexical block, the compiler evaluates all bindings going out of scope.
3. If a binding's type implements `` `drop `` AND it was not the specific value yielded by the block, the compiler unconditionally inserts `typeof(cv).drop(cv)`.
4. No dynamic "drop flags" or runtime state-tracking are required.

## Ergonomics of Ownership Transfer

This system forces a strict, tree-like ownership model. You cannot transfer ownership of a local variable halfway through its scope.

If a function `consume_box(b: Box)` takes ownership, passing a mid-scope variable is illegal:
```chirp
let b = Box.new();
do_something();
consume_box(b); // ERROR: b is not going out of scope here.
do_something_else();
```

Instead, you must organically align the variable's scope with the transfer of ownership:
```chirp
consume_box(do {
    let b = Box.new();
    do_something();
    break b;
});
do_something_else();
```

While restrictive, this aligns perfectly with Chirp's "TypeScript-annotated Zig" philosophy. It eliminates hidden control flow, makes the exact lifetime of heap allocations aggressively visible in the code structure, and completely removes the need for a `std::move()` intrinsic.

## Reassignment and Overwrites

If a binding holding a droppable type is reassigned mid-scope, the old value is dropped immediately before the overwrite:

```chirp
let mut p = Box.new(1);
p = Box.new(2); // typeof(cv).drop(cv) is implicitly called on Box(1) here.
```

Because the old value cannot be "moved out" mid-scope, dropping it is the only semantically sound outcome.

## Interpreter vs. Calcification

During the dynamic execution phase, the Chirp interpreter might use host-level garbage collection to manage its own representations of `Box`. However, the interpreter strictly enforces the `` `copy ``, Terminal Move, and `` `drop `` semantics flow-sensitively.

By enforcing these constraints during interpretation, the compiler guarantees that the program's memory lifecycle is statically provable. When the program reaches the **Calcification** phase for machine lowering, the compiler can confidently emit raw `malloc` and `free` instructions with zero runtime overhead, knowing that aliases and double-frees are structurally impossible.
