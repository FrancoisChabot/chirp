# Modules in Chirp (Draft)

This chapter defines how modules work in Chirp.

Following Chirp's core philosophy of maintaining an absurdly small semantic surface, modules do not introduce new fundamental physics to the language. Instead, they emerge from existing primitives: Values, Bindings, structs, and `import`.

## The Core Concept: Modules are Values

In traditional compiled languages, modules are often static, compile-time namespaces. In Chirp, where execution begins in the interpreter and compilation is a downstream library function, **a module is just a Value**.

Specifically, a module is a finalized struct-like value that aggregates a set of public bindings exported by a Chirp source file.

There is no special `Module` namespace. When you interact with a module, you are simply interacting with a value:

```chirp
let math = `import("./math.chirp");

let circumference = math.pi * diameter;
```

Module member access is ordinary struct property access. A module can be bound, constrained, passed around, or stored like any other value.

## Importing and Module Resolution

The bootstrap chapter introduces the intrinsic function used to load external resources:

```chirp
`import(key: string, format: string = "chirp"): `any;
```

This chapter defines the behavior of `import` when `format` is `"chirp"`.

When importing a Chirp module, the `key` identifies a Chirp source file. The interpreter resolves that key to a canonical module identity, evaluates the corresponding file if necessary, and returns the resulting module value.

The `format` parameter specifies the kind of payload being imported. It does not, by itself, dictate where the payload is located. For example:

* `"__chirp_boot"` identifies privileged bootstrap imports.
* `"chirp"` identifies ordinary Chirp source modules.
* Future formats such as `"C"`, `"c_abi"`, or `"dylib"` may identify FFI imports.

### Explicit Paths

If the key starts with a filesystem path indicator (`./`, `../`, or `/`), it is treated as an explicit path.

Relative paths are resolved relative to the file currently being evaluated. Absolute paths are resolved as absolute filesystem paths.

```chirp
let math = `import("./math.chirp");
```

Explicit paths bypass logical namespace resolution.

### Logical Module Keys

If the key does not start with a path indicator, it is treated as a logical module key.

```chirp
let vector = `import("std/vector");
let utils = `import("mylib/utils");
```

The interpreter resolves the first segment of a logical key against its configured namespaces.

### Reserved Namespaces

To avoid the fragility of C-style implicit include shadowing, Chirp reserves some logical namespaces.

The `std` namespace is reserved by the interpreter. Any logical import starting with `std/` is resolved against the active standard library root. It does not poll user search paths.

```chirp
let vector = `import("std/vector");
```

If a project happens to contain a local `std/` directory, that directory does not shadow the standard library.

To import from such a directory, use an explicit relative path:

```chirp
let local_stuff = `import("./std/my_stuff.chirp");
```

An implementation may provide an explicit `--std-override` mechanism to replace the active standard library root. This is intended for specialized environments, such as embedded targets.

### User Search Paths

For logical keys outside reserved namespaces, the interpreter may search user-configured import roots, such as directories provided by an `-I`-style option.

The exact command-line mechanism is implementation-defined, but the resolution rules must be deterministic. If resolution is ambiguous, the interpreter should reject the import rather than silently choose an arbitrary candidate.

## Module Evaluation and Caching

For `import(key, "chirp")`, the interpreter behaves as follows:

1. Resolve `key` to a canonical module identity.
2. If that module has already completed evaluation in the current interpreter session, return the cached module value.
3. If that module is currently being evaluated, report an import cycle.
4. Otherwise, evaluate the target file in a fresh module scope.
5. Collect the file's public exports.
6. Package those exports into a finalized anonymous struct-like value.
7. Cache that value under the module identity.
8. Return the module value.

Two imports of the same resolved module identity must return the same module value within a single interpreter session.

This rule is important because modules may define nominal values such as minted types or traits. Re-evaluating the same module would create distinct identities and change program semantics.

## Exporting: How a File Becomes a Module

When a Chirp file is evaluated as a module, its top-level statements run from top to bottom in a private module scope.

Bindings marked `pub` are exported. Non-`pub` bindings remain private to the module.

```chirp
// math.chirp

let internal_double = (x) => x * 2;

let pub final pi = 3;
let pub final tau = pi * 2;

let pub final scale = (x) => internal_double(x);
```

Importing this file returns a module value with fields corresponding to the public exports:

```chirp
let math = `import("./math.chirp");

`print(math.pi);
`print(math.tau);
`print(math.scale(10));
```

The private binding `internal_double` is not a field of the returned module value.

### Why Explicit Exports?

A module's public interface should be visible from the source file itself.

For this reason, Chirp uses explicit `pub` exports instead of treating the final expression of a file as the module value.

This keeps module APIs grep-able, documentable, and stable under ordinary edits.

## Finalized Module Values

The value returned by `import` is finalized.

Its exported fields cannot be reassigned through the module value:

```chirp
let math = `import("./math.chirp");

math.pi = 4; // Error: module fields are not assignable
```

This does not mean that all values reachable from a module are deeply immutable. It means that the exported field bindings of the module value itself are fixed.

In practice, a module behaves like a finalized struct whose fields are constrained to the values produced during module evaluation.

This property gives Calcification a stable target. If the compiler sees:

```chirp
math.scale(10)
```

and `math` is known to be a module value, it can resolve `scale` to the exact exported function value and may emit a direct call where appropriate.

## Cyclic Dependencies

Because importing a Chirp module evaluates code, cyclic imports must be detected.

For example:

```chirp
// a.chirp
let b = `import("./b.chirp");

// b.chirp
let a = `import("./a.chirp");
```

If a module attempts to import another module that is already in the current import stack, the interpreter must reject the program with a clear import-cycle error.

Future versions of Chirp may introduce deferred or lazy mechanisms that allow some cyclic structures to be expressed safely. Until then, module dependencies are expected to form a directed acyclic graph.

## The Interpreter's Burden

A compliant interpreter must:

* Provide the `` `import(key: string, format: string = "chirp"): `any `` intrinsic.
* Implement the `"chirp"` import format.
* Resolve explicit paths relative to the file currently being evaluated.
* Resolve logical module keys according to the namespace rules.
* Reserve the `std/` namespace for the active standard library root.
* Evaluate imported Chirp files in fresh module scopes.
* Collect `pub` top-level bindings into a finalized anonymous struct-like module value.
* Cache completed module values by canonical module identity.
* Return the cached value for repeated imports of the same module.
* Detect import cycles and report them as hard errors.

## The Bootstrap's Burden

The bootstrap does not define the mechanics of module loading, module resolution, export collection, caching, or cycle detection.

However, the bootstrap and standard library may provide modules under reserved namespaces such as `std/`, and may use ordinary module exports to define standard user-facing functionality.

In other words: module mechanics are the interpreter's burden; standard module contents are the bootstrap and standard library's burden.
