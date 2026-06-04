# Chirp Specification: Traits and Implementations

This document outlines the syntax, semantics, and compilation mechanics for endowing user-defined types (such as structs) with custom behaviors like **set-ness**, **yield-ness**, or other user-defined traits.

---

## 1. Core Philosophy

In Chirp, we avoid introducing complex, dedicated keywords or type-system machinery for traits. Instead, we lean directly on the core ontology:

1. **Everything is a Value:** User-defined types (e.g. `struct` definitions) are first-class values of type `Type`.
2. **Traits are Matcher Domains:** Mathematically, a trait is simply the **domain** (the set of valid inputs) of a first-class `Matcher` object (piecewise function). Implementing a trait means adding a new mapping arm to that global matcher.
3. **Set-ness and Yield-ness as Primitives:** Special operators like `∈` (belonging) and `()` (callable invocation) on non-function values are simply syntactic sugar for invoking built-in matchers.

To make implementing traits cohesive and readable, we define an `implement` method on the `Type` type value itself. This avoids introducing new syntax or keywords, maintaining parser purity while providing a high-level, elegant interface.

---

## 2. The `Trait` Ontology

A trait is represented in the standard library as an instance of the `Trait` struct:

```chirp
let Trait = struct {
    name: string,
    register: (Type, any) => void
};

// Implement is a method on the `Type` value, which delegates registration to the Trait
let Type.implement(self: Type, trait: Trait, impl: any) : void = do {
    trait.register(self, impl);
    yield;
};
```

---

## 3. Endowing Set-ness (Single-Method Trait)

**Set-ness** allows instances of a user-defined type to act as sets, making them compatible with the belonging operator `∈`. 

Because set-ness is defined by a single belonging predicate, we implement it by passing a lambda or a function.

### Standard Library Definition (`std.setness`)

```chirp
let std.setness = Trait {
    name: "setness",
    register: (t: Type, predicate: (any, any) => bool) => do {
        // Add a new branch to the global contains Matcher
        std.contains.add({ (any, t) }, predicate);
        yield;
    }
};
```

### Usage Example

```chirp
let my_cool_thing = struct { x: int, y: int };

// Endow my_cool_thing with set-ness
my_cool_thing.implement(std.setness, (v: any, s: my_cool_thing) => do {
    yield match v {
        pt: Point => pt.x >= 0 && pt.x <= s.x && pt.y >= 0 && pt.y <= s.y,
        `any  => false
    };
});

// Using the set-ness!
let boundary = my_cool_thing(x=10, y=10);
let pt = Point(x=5, y=5);

let inside = pt ∈ boundary; // Evaluates to true
```

---

## 4. Endowing Yield-ness (Multi-Method Trait)

**Yield-ness** allows instances of a user-defined type to act as callable functions or generators. 

Because yield-ness requires three distinct operations (`parameter_domain`, `result_domain`, and `yield`), we bundle the implementation functions into a struct instance.

### Standard Library Definition (`std.yieldness`)

```chirp
// The interface struct for yieldness
let YieldnessImpl = struct {
    parameter_domain: (any) -> set,
    result_domain: (any, any) -> set,
    yield: (any, any) -> any
};

let std.yieldness = Trait {
    name: "yieldness",
    register: (t: Type, impl: YieldnessImpl) => do {
        // Register each capability to its respective global Matcher
        std.parameter_domain.add({ t }, impl.parameter_domain);
        std.result_domain.add({ (t, any) }, impl.result_domain);
        std.yield.add({ (t, any) }, impl.yield);
        yield;
    }
};
```

### Usage Example

```chirp
let my_cool_thing = struct { x: int, y: int };

// Endow my_cool_thing with yield-ness
my_cool_thing.implement(std.yieldness, YieldnessImpl {
    parameter_domain: (s: my_cool_thing) => int,
    
    result_domain: (s: my_cool_thing, param: int) => int,
    
    yield: (s: my_cool_thing, param: int) => do {
        yield s.x * param + s.y; // Evaluates linear equation
    }
});

// Using the yield-ness!
let f = my_cool_thing(x=2, y=3);
let result = f(5); // Evaluates to 13
```

---

## 5. Lowering & Compiler Mechanics

Chirp is designed to compile to C-equivalent, highly efficient code. Although the `Type.implement` API is dynamic and reflective at runtime, it compiles with **zero abstraction cost** due to static lowering.

### Static Devirtualization

During compilation, the compiler parses all top-level statements. When it encounters `T.implement(trait, impl)`:

1. It extracts the functions provided in `impl`.
2. It generates standard static C functions for each implementation (e.g. `my_cool_thing_yield(my_cool_thing s, int param)`).
3. If the trait registration affects built-in operations (like `∈` or calling `s(param)`), the compiler registers this type-association in its static lookup tables.

### Operator Lowering

When compiling operations on values, the compiler devirtualizes them directly:

* **Lowering `v ∈ s`:**
  1. The compiler infers the static type `T` of `s`.
  2. If `T` has registered a `std.setness` predicate, it replaces the belonging check with a direct, static function call to that predicate:
     ```c
     // v ∈ s  ==> Lowered directly to:
     my_cool_thing_contains(s, v);
     ```
  3. If `T` is not statically known, it falls back to a dynamic dispatch through the global `std.contains` Matcher.

* **Lowering `s(param)`:**
  1. The compiler infers the static type `T` of `s`.
  2. If `T` has registered `std.yieldness` handlers, it replaces the function call with a direct call to the static yield function:
     ```c
     // s(param) ==> Lowered directly to:
     my_cool_thing_yield(s, param);
     ```

This approach yields the best of both worlds: a highly expressive, elegant, dynamic-looking programming model at the language level that compiles down to blistering-fast, devirtualized, flat C code.

---

## 6. Future Extensions: Domain Inference from Yield Predicates

In a fully refined implementation, specifying `parameter_domain` and `result_domain` explicitly for yield-ness is often redundant if the compiler or the runtime can statically inspect the signature of the provided `yield` function.

For instance, when implementing `std.yieldness` with an annotated lambda:
```chirp
my_cool_thing.implement(std.yieldness, YieldnessImpl {
    yield: (s: my_cool_thing, param: int) : string => do {
        // ...
    }
});
```

Because the `yield` function's parameter and return constraints are already explicitly declared (`param: int` and `: string`), the compiler can automatically synthesize the domains:
* `parameter_domain` is inferred as `int`.
* `result_domain` is inferred as `string`.

This eliminates boilerplate, enabling a shorthand registration syntax where only the `yield` implementation needs to be provided, and its domains are derived automatically.
