# Chirp Design Notes

## First-Class Control Flow and Matchers

In Chirp, "everything is a value". This philosophy extends to control flow constructs, specifically pattern matching.

The `match` keyword is not a traditional control flow statement that executes immediately. Instead, it acts as a constructor for an intrinsic `Matcher` object (which mathematically represents a piecewise function).

Evaluating a match block without an explicit subject creates this `Matcher` object:

```chirp
let to_string = match {
    it: int => std.int_to_string(it),
    it: float => std.float_to_string(it),
};
```

Because `to_string` is a standard first-class object, its mapping rules can be mutated (e.g., during module load time). This allows for open-world extensibility, where new mapping branches (match arms) can be injected into the `Matcher` dynamically across different modules:

```chirp
let my_struct = struct { a: int };
let my_struct_to_string(v: my_struct) : string = { ... };

// Injecting a new branch into the existing match block
to_string.add({my_struct}, my_struct_to_string);
```

### The `match expr {}` Syntactic Sugar

The standard syntax of evaluating a match block against an expression:

```chirp
match subject {
    it: int => 1,
    it: float => 2,
}
```

Is actually syntactic sugar. Under the hood, this constructs an anonymous `Matcher` object and immediately invokes it with the `subject` as the argument:

```chirp
(match {
    it: int => 1,
    it: float => 2,
})(subject)
```

### Traits as Matcher Domains

This paradigm elegantly formalizes the concept of "Traits". A trait in Chirp is not a complex, dedicated language construct. Instead, a trait is simply the mathematical **domain** (the set of valid inputs) of a `Matcher` object.

For example, a `convertible_to_string` trait is just `domain(to_string)`. 

"Implementing a trait" for a new type is simply the act of using `.add()` to inject a new mapping arm into that global `Matcher`. Doing so naturally expands the function's domain to include the new type, making it belong to the trait.

## Lambdas and Anonymous Functions

Because Chirp does not have an `fn` keyword (standard functions are declared via `let name(args) = body`), anonymous functions (lambdas) are constructed using the fat arrow `=>` binary operator. 

This maps an argument list on the left to an expression or block on the right, providing a lightweight, ergonomic syntax familiar to users of JS, C#, and other modern languages.

```chirp
// Using a lambda to inject logic into a Matcher
strings.to_string.add({my_struct}, (v: my_struct) => {
    // ... string conversion logic ...
});

// For simple inline expressions
let square = (x: int) => x * x;
```

## Core Ontology & Architecture

Chirp's foundation is built on a strict, self-referential ontology of four concepts: Types, Values, Sets, and Bindings.

1. **Values & Types:** Every value has exactly 1 intrinsic type. Types are themselves values. The type of every type is `Type`.
2. **Sets:** Any value whose type has set-ness can act as a Set. Any value can be checked for belonging in a Set. The universal set is `any`.
3. **Bindings:** A binding (variable) has a current value and is constrained by a set of possible values. This constraint can become narrower over time (Flow-sensitive typing).

## The Three-Shell Execution Model (Concept)

To solve the "predictable performance" problem that plagues many systems languages, Chirp's execution model can be conceptually viewed as three concentric shells. This model guides both the standard library design and the compiler's compilation targets:

1. **Outer Shell (Chirp-Interpreter):** The full execution environment. Code here has access to the compiler's AST, the filesystem, and unlimited memory. It is highly dynamic. You cannot compile this shell into a standalone binary without effectively bundling the interpreter itself.
2. **Middle Shell (Chirp-Dynamic):** Code that can be compiled to a standalone executable, but requires a runtime. Features here include non-elidable tagged unions (e.g., `int ∪ bool`), runtime-modifiable sets, dynamic allocations, and global matcher dispatch. The performance profile is akin to Go, Swift, or Julia.
3. **Inner Shell (Chirp-Static):** The holy grail: flat, zero-overhead C-equivalent code. Deterministic memory layouts, statically devirtualized traits, no RTTI, and no hidden allocations.

When invoking compilation (e.g., `std.compile(entrypoint, target="static")`), the target acts as a strict performance linter. If a developer targets the inner static shell but the compiler's flow analysis fails to devirtualize a trait or elide a type tag, the compiler throws a hard error rather than silently emitting slow dynamic code. This provides a guaranteed "static escape hatch" for performance-critical hot paths, ensuring true zero-cost abstractions.

## Keywords vs. Intrinsics (The Backtick Rule)

To keep the parser lightweight and prevent namespace collision, Chirp draws a strict formal line between keywords and intrinsics:

1. **Keywords (`if`, `let`, `match`, `struct`):** These dictate grammar, control flow, and type declarations. They are standard reserved words and do not require any prefix.
2. **Intrinsics (`` `any ``, `` `import ``, `` `type ``):** If a compiler-provided symbol acts syntactically as an identifier (a primary expression that evaluates to a value), it must be prefixed with a backtick. This guarantees absolute forward compatibility, as the compiler can add new intrinsics in the future without ever shadowing user-defined variables.

**The Atom Exception:** 
Initially, it was thought that `true` and `false` would either need to be written as `` `true `` and `` `false `` (pedantic application of the rule) or explicitly hardcoded as reserved keywords to prevent shadowing (`let true = 5;`). However, the formal ontology of Atoms provides a much cleaner solution. The compiler treats scalar literals (`42`, `"hello"`, `true`) conceptually as identifiers pre-bound in the global environment with the `unshadowable` modifier. Because they are implicitly `unshadowable`, they cannot be shadowed. This eliminates the need for them to be special parser keywords, perfectly unifying the mathematical evaluation model.

## Bindings, Mutability, and Aliasing

Bindings in Chirp are governed by keywords that modify the behavior of the binding itself, not the underlying value.

- **`mut`**: Allows the current value of the binding to be swapped (mutated) over time.
- **`unshadowable`**: Prevents the identifier from being shadowed in any descendent scope.

**The `using` Statement:**
Because "everything is a value" in Chirp, standard assignment (`let y = x;`) implies a value copy. To avoid the performance and identity issues of copying large values (like Types or whole modules), Chirp uses a dedicated `using` statement.
```chirp
using config = giant_struct.sub_system.config;
```
A `using` declaration is strictly a compile-time syntactic redirect within a local lexical block. Crucially, `using` declarations **do not exist in the type system** (you cannot pass one to a function or return one). This provides the ergonomic benefits of C++ references for local variables, but mathematically eliminates the possibility of dangling references or lifetime chaos.

## Literals, Atoms, and Infinity

Chirp formally divides what are traditionally called "literals" into two categories:

1. **Atoms (Scalar Literals):** Integers, strings, and booleans (`42`, `"hello"`, `true`).
2. **Constructors (Compound Literals):** Sets and layouts (`{1, 2}`, `struct {a: int}`).

**Atoms as Lazy Identifiers:**
As mentioned above, Atoms are conceptually evaluated simply by looking them up in the global environment. The compiler behaves as if the root prelude lazily executes `let final 42 = ...;`. 

**The `std.enumerable` Function:**
Because Atoms are conceptually pre-bound, the set of all identifiers in a scope (`some_scope.identifiers`) is mathematically infinite. If a metaprogrammer wants to iterate over the variables defined in a scope, they use the `std.enumerable(S)` standard library function to extract only the finite, explicitly declared bindings, ignoring the infinite sea of lazy literal identifiers. 

For example:
```chirp
for (id in std.enumerable(some_scope.identifiers)) { ... }
```
Using a standard library function rather than a dedicated operator (like `~`) reserves single-character operators for high-frequency business logic, prevents confusion with standard C operators (like bitwise NOT), and keeps the core compiler namespace small.

## The Cost-Driven Constraint Solver & Ternary Set Belonging

To unify systems-level performance predictability with high-level constraint expressiveness, Chirp implements a cost-driven constraint solver that bounds undecidability.

### 1. Bounding the Solver: Computational Budgeting

Proving set belonging for arbitrary predicates is equivalent to general theorem proving, which is mathematically undecidable (the Halting Problem). Rather than treating non-termination as a compiler hang or a fatal logical contradiction, the compiler assigns a deterministic **computational budget** (e.g., a precise VM execution step count or recursion limit) to each proof attempt.

This budget acts as a developer-facing "Build Time vs. Elision" dial:
*   **High Budget:** The compiler spends more time executing complex or recursive compile-time proofs to maximize **Runtime Check Elision** (generating zero-overhead native code).
*   **Low Budget:** The compiler demotes complex proofs early to `"undetermined"`, prioritizing **Compilation Speed** at the cost of inserting more dynamic runtime checks.

### 2. Mandatory Narrowing vs. Best-Effort Fallbacks

To ensure systems programmers have iron-clad, compile-time performance guarantees where they matter, the set universe is divided into two distinct categories:

1.  **Decidable/Structural Sets (Category 1):** Types (e.g., `int`), enumerated sets (e.g., `{1, 2, 3}`), ranges (e.g., `0..9`), and basic compositions (e.g., `int ∪ bool`). Proving these is fast and finite. The compiler enforces **Mandatory Narrowing** here: if a constraint in this category cannot be proved at compile-time, it results in a **hard compilation error**.
2.  **Arbitrary Predicate Sets (Category 2):** Sets defined by general equations or recursive user functions (e.g., `{x | x ∈ int && x % 2 == 0}`). These opt into the computational budget and can fall back to `"undetermined"` under the **best-effort rule**.
