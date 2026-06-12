# The Chirp Specification
## The Core

> [!IMPORTANT]
> The word "set" has a very specific meaning from this point on. Unless it is explicitly called out as a "mathematical set", the word always refers to the definition laid out in the **Sets** section below.

### Bindings:

A **Binding** is the fundamental slot in which all computation and state in Chirp is staged.

To achieve this, every Binding exists across **three distinct domains**:

- **The Structural**:
   * Represented by its **Fundamental Constraint** (`fc`), which is a **Set**.
   * Whatever `Value` is assigned to `cv`, it will always, at any point in time, belong to this set.

- **The Spatial**:
   * Represented by its **Local Constraint** (`lc`), which is also a **Set**.
   * The content of `lc` is contextual. Each and every reference to the binding in the program's control flow can have a different `lc`. Whatever `Value` is assigned to `cv` **at that place in the program**, it will always belong to this set. 

- **The Temporal**:
   * Represented by the **Current Value** (`cv`)
   * It is the value assigned to the bind. There is always one.

#### Rules

- **The Space-to-Structure Law**:
   `lc ⊆ fc`
   *At any coordinate in the program's space, the local constraint (`lc`) must be a subset of the binding's structural identity (`fc`).* The compiler can never assume or prove something about a binding that violates its fundamental contract.

- **The Time-to-Space Law**:
   `cv ∈ lc`
   *At any tick of execution time, the chronological value (`cv`) must belong to the local constraint (`lc`) inferred for that specific point in space.* Runtime execution can never violate compile-time spatial proofs.

#### Creating a binding

The syntax `identifier: set_expr` denotes a binding with `set_expr` as its `fc`.

Depending on the evaluation context, that may also come with a mandatory or initial `cv` in the form of an `= expr` postfix.
```chirp
let some_var : {1,2,3} = 3; 

let foo = (param: {1,2,3}) => {};
```

`set_expr` is not a special kind of expression, by the way. It's just any ordinary expression, evaluated the same way as in any other context, that yields a set. Speaking of which...


### Sets: The Constraints

Chirp sets are defined *computationally*:
- *Some*, but crucially not all, values can play the role of a set. Being a set is not part of the identity of a value. It is a *capability*. 
- Sets can be interrogated as to what the belonging of **any** value is against themselves via their belonging predicate (`bp`).
- The range of a set's `bp` against a set of candidate values is called its **belonging range** (`br`). 
- `br` must be computable or statically approximable without executing arbitrary `bp`.
- `br` are always subsets of `{true, false, undecided}`. 

Examples of sets: (don't overthink `int` at the moment. We'll cover how that works soon enough). 

```chirp
0..3                        : bp = (v) => v ∈ int && v >= 0 && v < 3;   br = {true, false} 
{true, false}               : bp = (v) => v == true || v == false;      br = {true, false}
{x | x ∈ int && x % 2 == 0} : bp = (v) => x ∈ int && x % 2 == 0;        br = {true, false}
`any                        : bp = (_) => true;                         br = {true}

// spicy:   
{x | !(x ∈ x)}              : bp = (v) => !(v ∈ v)                      br = {true, false, undecided} // evaluates to `undecided` in degenerate cases
```

In chirp, sets are not defined by the fact they are used as `fc` and `lc`. They are garden variety values with certain properties that bindings happen to use:

```chirp
// Both uses of the {1, 2, 3} expression are semantically identical 

let test_result = 2 ∈ {1, 2, 3};
let x : {1, 2, 3} = 2;


// This means constraints can be indirected:

let constraint = {1, 2, 3};

let test_result = 2 ∈ constraint;
let x : constraint = 2;
```

### Values

A **Value** is an immutable package of data that can occupy the temporal domain (`cv`) of a Binding. 

### Types

Notice how **types** weren't mentioned at all until the previous section? That's because while types are critical to Chirp, the fundamental logic of Bindings and Sets doesn't strictly depend on them. Mathematically speaking, the rules of `lc ⊆ fc` and `cv ∈ lc` would remain perfectly sound even if our universe of values was just an amorphous, untyped pile of data.

But Chirp isn't just a mathematical theorem; it's a systems programming language. We can't execute abstract math on a CPU. To make this constraint system real, we need two things:

1. **Physical Reality**: We need to know the physical memory layouts of our values so we can compile down to machine code.
2. **Capability Dispatch**: We need a mechanical dispatch system to evaluate *how* a specific value behaves when used as a set.

This is exactly the dual-purpose role of **Types**.

In Chirp, every **Value** has exactly one intrinsic **Type** tag associated with it, denoted by `typeof(v)`.

#### Types as Capability Dispatchers

Recall that in the Sets section, we said a set is just a capability that a value *can* have. The way a value gains that capability is through its Type.

When you write the set-belonging check `v ∈ S`, you are not invoking a magic property on `S`. The `∈` operator is actually syntactic sugar that dispatches the belonging check to the **Type** of `S`. 

Specifically, it desugars to:
```chirp
typeof(S).bp(S, v)
```

If `typeof(S)` does not define a `bp` (belonging predicate), then `S` simply cannot be used as a set, and attempting to do so is a an error.

This is where the elegance of the system crystallizes. Let's look at how the sets from our earlier examples actually work under the hood:

```chirp
// Example 1: The range value
let x : 0..3 = 2;

// `0..3` is a value of type `IntRange`. 
// `IntRange` implements `bp(range, v)` as `v ∈ int && v >= range.start && v < range.end`.
// So `2 ∈ 0..3` desugars to:
typeof(0..3).bp(0..3, 2)
```

#### Types as Sets

If types are values, then types themselves must have a type! In Chirp, the type of every type tag is the special value `Type`. 

This unlocks one of Chirp's most powerful emergent behaviors: **Types can be used as sets themselves.**

How? Because `Type` (the type of all types) defines its own `bp`. 

When you use a type like `int` as a constraint, the exact same desugaring rules apply:
```chirp
let x : int = 5;

// The belonging check `5 ∈ int` desugars to:
typeof(int).bp(int, 5)

// Since typeof(int) is `Type`, this evaluates to:
Type.bp(int, 5)
```

The `bp` implementation for `Type` is trivial: it simply checks if the value's intrinsic type tag matches the instance. 
```chirp
// Conceptual implementation of Type's bp:
Type.bp = (type_instance, v) => typeof(v) == type_instance;
```

This means "types as sets" requires absolutely zero special casing in the compiler. A type acts as a set of its own instances strictly as a natural consequence of the universal `v ∈ S` dispatch rule.

### Binding Semantics 

With the foundations laid out, we can return to Bindings to explore their more advanced properties and behaviors within the language.

#### Mutability

Bindings are (unless constrained via a singleton set) mutable. Both the current value (`cv`) and local constraint (`lc`) can change as long as the invariants are respected. In general, modification of the local constraint is the domain of the compiler, which will reduce it along the program flow according to what it can manage to prove.

```chirp
let mut x : {1, 2, 3} = foo();
if (x != 1) do {
  // The local constraint (`lc`) of `x` can be inferred as {2, 3} here.
  x = 1; // Legal, because 1 satisfies the binding's invariant structural constraint `fc` {1, 2, 3}.
  // The local constraint (`lc`) of `x` is now exactly {1}.
}
// The local constraint (`lc`) of x is back to {1, 2, 3} regardless of which branch gets taken
```

While domain refinement is always semantically sound when applied, domain narrowing is a best-effort process performed by the compiler.

In practice, mutability needs to be opted-in by the contextual `mut` binding modifier. However, that is purely a syntactical construct.

Changing the value of a binding is assigning it a new value. This may seem evident when considering booleans and integers, but this also applies to larger and more complicated categories of values.

**The Performance Elision Rule:** Conceptually, modifying a struct field (e.g., `a.f = 2;`) involves evaluating the old value of `a`, constructing a brand-new immutable struct value where `f = 2`, and assigning this new value back to the Binding `a`. Since it is guaranteed that the old value of the binding is about to be thrown out, the compiler is obligated to elide this into an in-place mutation of the binding's physical storage.

#### Implicit Dereferencing 

Evaluating an identifier `x` normally evaluates to the *current value* (`cv`) held by its underlying Binding. However, there are a few exceptions to this rule:

- `` `binding(identifier) `` is a *special intrinsic* that yields a value representing the current state of the binding associated with that identifier.

### Behold, Chirp, revisited

The [introduction](00_introduction.md) opened with this equation:

```
b ∈ S ≜ τ(S).bp(S, b.cv : b.lc ⊆ b.fc) : τ(S).br(S, b.lc) ⊆ {true, false, undecided}
```

We can now finally unpack it.

- `b.cv : b.lc ⊆ b.fc` : That's the binding funnel. A binding has a current value which belongs to the spatial constraint, which in turn is a subset of the fundamental constraint.
- `b ∈ S ≜ τ(S).bp(...)` : This is set-ness formalized. Checking if a value belongs to a set is indirected via the set's type. 
- `: τ(S).br(S, b.lc)` : This says that the result of `bp` must belong to the set obtained when evaluating the `br` preflight check.
- `⊆ {true, false, undecided}` : This says that the `br` preflight check must return a subset of `{true, false, undecided}`.

And at the end of the day, that *IS* what chirp is all about.

## Next steps

Make sure you've got a good grasp of all this before proceeding, because we are going to be leveraging each of these concepts *extensively* from here on in.

Next up: [The Machine](03_machine.md), where we will see this all maps down to actual hardware.
