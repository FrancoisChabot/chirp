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

The `std` namespace is reserved by the interpreter. The interpreter has an active Chirp root directory containing `boot/` and `std/`. Any logical import starting with `std/` is resolved as a filesystem path relative to the active root's `std/` directory. It does not poll user search paths.

```chirp
let vector = `import("std/vector");
```

If a project happens to contain a local `std/` directory, that directory does not shadow the standard library.

To import from such a directory, use an explicit relative path:

```chirp
let local_stuff = `import("./std/my_stuff.chirp");
```

An implementation may provide an explicit root override, such as `--root-dir`, to replace the active Chirp root. This is intended for specialized environments, such as embedded targets.

### User Search Paths

For logical keys outside reserved namespaces, the interpreter may search user-configured import roots, such as directories provided by an `-I`-style option.

The exact command-line mechanism is implementation-defined, but the resolution rules must be deterministic. If resolution is ambiguous, the interpreter should reject the import rather than silently choose an arbitrary candidate.

## Module Evaluation and Caching

For `import(key, "chirp")`, the interpreter behaves as follows:

1. Resolve `key` to a canonical module identity. For filesystem imports, this is the normalized absolute path without following symlinks.
2. If that module has already completed evaluation (or failed to evaluate) in the current interpreter session, return the cached module value or cached error.
3. If that module is currently being evaluated (meaning it is actively on the dynamic import call stack), report an import cycle error.
4. Otherwise, evaluate the target file in a fresh module scope. This scope automatically inherits the global namespace populated by the bootstrap.
5. Collect the file's public exports.
6. Package those exports into a finalized anonymous struct-like value. The exported fields preserve the fundamental constraints (`fc`) of their original bindings.
7. Cache that value (or evaluation error) under the module identity.
8. Return the module value.

Two imports of the same resolved module identity must return the same module value within a single interpreter session.

This rule is important because modules may define nominal values such as minted types or traits. Re-evaluating the same module would create distinct identities and change program semantics. If a module fails to evaluate, that failure is also cached, avoiding re-evaluation of broken modules (a future bootstrap may mint a specific value to represent this error state).

## Exporting: How a File Becomes a Module

When a Chirp file is evaluated as a module, its top-level statements run from top to bottom in a private module scope.

Bindings marked `pub` are exported. Non-`pub` bindings remain private to the module.

```chirp
// math.chirp

let internal_double = (x) => x * 2;

let pub pi = 3;
let pub tau = pi * 2;

let pub scale = (x) => internal_double(x);
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

This does not mean that all values reachable from a module are deeply immutable. It means that the exported field bindings of the module value itself are fixed. The exported bindings themselves retain their original fundamental constraints (`fc`) from when they were declared within the module.

This property gives Calcification a stable target. If the compiler sees:

```chirp
math.scale(10)
```

and `math` is known to be a module value, it can resolve `scale` to the exact exported function value and may emit a direct call where appropriate.

## Cyclic Dependencies

Because importing a Chirp module evaluates code dynamically, cyclic imports must be detected at runtime.

For example:

```chirp
// a.chirp
let b = `import("./b.chirp");

// b.chirp
let a = `import("./a.chirp");
```

Evaluation happens until a module that's in the process of being imported tries to be imported again. If a dynamic `import()` call attempts to evaluate a module that is currently on the dynamic import call stack, the interpreter must reject the program with a clear import-cycle error.

Future versions of Chirp may introduce deferred or lazy mechanisms that allow some cyclic structures to be expressed safely. Until then, module dependencies are expected to form a directed acyclic graph.

## The Interpreter's Burden

A compliant interpreter must:

* Provide the `` `import(key: string, format: string = "chirp"): `any `` intrinsic.
* Implement the `"chirp"` import format.
* Resolve explicit paths relative to the file currently being evaluated.
* Resolve logical module keys according to the namespace rules.
* Reserve the `std/` namespace for the active Chirp root's `std/` directory.
* Evaluate imported Chirp files in fresh module scopes.
* Collect `pub` top-level bindings into a finalized anonymous struct-like module value.
* Cache completed module values by canonical module identity.
* Return the cached value for repeated imports of the same module.
* Detect import cycles and report them as hard errors.

## The Bootstrap's Burden

The bootstrap does not define the mechanics of module loading, module resolution, export collection, caching, or cycle detection.

However, the bootstrap and standard library may provide modules under reserved namespaces such as `std/`, and may use ordinary module exports to define standard user-facing functionality.

In other words: module mechanics are the interpreter's burden; standard module contents are the bootstrap and standard library's burden.
