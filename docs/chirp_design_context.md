# Chirp Design Context

This note is a compact design baseline for future language discussions. It
summarizes the project intent from the README and docs, then connects that to
the implemented bootstrap layer in `lib/chirp/boot`.

## Current State

Chirp is being built as a systems programming language with two unusual design
goals:

- the program is described by Chirp code itself, using the same machinery that
  later supports metaprogramming
- correctness constraints are expressed through a set-theoretic model instead
  of a conventional "types first" story

The project is explicitly still a work in progress. The current deliverable is
an interpreter-oriented Dynamic Chirp. Compilation, the static solver, and code
emission are not implemented yet.

The README also calls out a stability split:

- surface syntax and grammar are relatively stable
- semantics are "mostly" stable but several areas are still intentionally loose
- the standard library is not yet trustworthy

## Core Model

The most important conceptual move in Chirp is that `x : S` is not primarily a
type annotation. It is a constraint saying the binding's value must belong to a
set-valued expression `S`.

The design docs describe bindings through three layers:

- `fc` (fundamental constraint): the set of values the binding may hold in
  general
- `lc` (local constraint): the narrower set of values the binding may hold at a
  specific point in control flow
- `cv` (current value): the actual runtime value

The intended invariants are:

- `lc` is a subset of `fc`
- `cv` belongs to `lc`

This is the basis for Chirp's flow-sensitive reasoning. Mutability and control
flow analysis are framed as updates or refinements of those constraints rather
than as a separate type-system layer.

## Sets, Types, and Belonging

Chirp's docs are intentionally careful about not collapsing "type" and "set"
into a single idea.

- A value always has exactly one intrinsic runtime type.
- A set is any value whose type provides set behavior.
- Belonging is the central relation, written with `in` / `notin` or the Unicode
  operators.

The design-level model says set behavior consists of:

- a belonging predicate, usually described as `belongs(self, value)`
- a belonging range, `belongs_range(self, candidates)`, which predicts whether
  the answer space is `{true}`, `{false}`, `{true, false}`, or may include
  `undecided`

That second piece matters because Chirp does not want all set reasoning to be
forced into plain booleans. `undecided` exists as a real value and is
deliberately not a `bool`.

Types participate in this model as ordinary values. A type can itself be used
as a set of its instances, but that is meant to fall out of the general set
machinery rather than being a special case.

## Implemented Surface Language

The user guide, lexical spec, and grammar spec describe the current frontend
and interpreter-facing surface language.

Implemented user-facing building blocks include:

- `let` bindings with optional constraints
- set literals: enumerated sets and constructed sets
- ranges such as `1..5` and `1..=5`
- block expressions via `do { ... }`
- lambdas with `=>`
- function sugar on `let`
- `if`, `while`, and `for`
- structs and enums as expressions that produce values/types
- `match`, where set-like arms perform belonging tests
- traits as first-class values, set constraints, and implementation carriers
- positional or named calls, but not mixed in the same call

Important currently documented limits include:

- `for` currently iterates only over ranges
- the parser accepts some forms that are rejected later by the interpreter
- some signature syntax is accepted as a parser artifact and should not be
  treated as supported language design
- numeric literals may lex with fractional parts, but the current interpreter
  still rejects floating-point values semantically

The lexical and grammar docs are best read as "implemented behavior of the
current bootstrap frontend", not as the final semantic authority.

## What Bootstrap Actually Does

The repo path that matters is `lib/chirp/boot`, not `lib/bootstrap`.

Boot files are loaded by the interpreter in lexical filename order. They run
under special rules:

- `import(..., "__chirp_boot")` is available only during boot evaluation
- boot files may define backtick-prefixed names
- public top-level boot bindings are exported into the global namespace
- private top-level boot bindings remain in boot scope

This is not just convenience glue. The bootstrap layer installs a large part of
the language's public semantic protocol.

### `00_fundamental.chirp`

This file establishes the base bridge between host-provided primitives and
user-visible language semantics.

It defines or imports:

- primitive inspection and construction hooks such as `type_of`, trait
  registration, finite-value minting, purity checks, and set helpers
- foundational values like `int`, `char`, `string`, `symbol`, `bool`, `void`,
  `true`, `false`, `undecided`, `any`, and `empty`
- the `Set` trait with `belongs` and `belongs_range`
- the `Callable` trait with `param_space`, `result_space`, and `invoke`

It then registers those traits back into the interpreter so that membership and
call syntax dispatch through the boot-defined protocols instead of remaining
hardcoded.

This file also makes two important categories into sets:

- traits behave as sets of values whose types implement them
- types behave as sets of their instances

That is the key bridge between the design docs and everyday source code like
`let x : int = 1;`.

### `01_scoping.chirp`

This introduces scoping-related semantic traits:

- `Drop` for cleanup on scope exit when a value is not terminally moved
- `Unique` as a marker trait opting a type out of default copyability

The docs frame this as part of a longer-term goal where ownership-style
properties emerge from the general reasoning framework instead of requiring a
completely separate subsystem.

### `02_memory.chirp`

This layer exposes host-backed heap primitives and defines reference-oriented
traits:

- `Dereferenceable`
- `DereferenceableMut`

It also defines a `ReferenceSet` struct type whose set semantics describe
references to values belonging to a target set, optionally requiring mutable
dereference support.

### `03_operators.chirp`

Operator semantics are expressed through traits instead of direct per-operator
type tables.

The bootstrap layer defines traits for:

- comparison
- additive, subtractive, negation, multiplication
- division and modulo
- indexing and indexed assignment

The file also documents important semantic assumptions:

- user-defined operator dispatch is only defined for same-runtime-type operands
  for now
- comparison and arithmetic implementations are expected to satisfy algebraic
  laws the interpreter may rely on
- purity is part of the semantic contract even where enforcement is still weak

### `04_conveniences.chirp`

This file adds convenience semantics that are important for ergonomics and for
eventual machine-shape constraints.

It defines:

- common integer ranges such as `int8`, `int16`, `int32`, `int64`, `uint8`,
  `uint16`, `uint32`, and `uint64`
- singleton-set behavior for common literal-bearing types, making values like
  `1` or `"hi"` usable directly in `match` arms and constraints
- `exit`

The singleton-set behavior is especially important because it removes the need
to write `{1}` everywhere a literal pattern is intended.

### `05_testing.chirp` and `99_dev_compromises.chirp`

These are not core semantic foundations in the same sense as the earlier files,
but they matter for understanding the current environment.

- `05_testing.chirp` exposes the reference test harness intrinsics such as
  `expect`, `expect_stdout`, and related helpers
- `99_dev_compromises.chirp` provides current I/O conveniences like `print`,
  `write`, and `input`, with an explicit note that they may later be replaced by
  lower-level primitives or FFI-driven mechanisms

## Interpreter vs Boot vs Draft Spec

There are three different layers of truth in the repo:

- `docs/spec/04_lexical.md` and `docs/spec/05_grammar.md` describe the current
  implemented frontend behavior
- `lib/chirp/boot` defines a large share of the user-visible semantic protocols
  after startup
- `docs/spec/drafts/02_core.md` and `docs/motivation.md` explain the intended
  model and where the design is trying to go

Those layers mostly align, but they should not be treated as identical.

In particular:

- the draft core spec is more semantically ambitious than the current
  interpreter in some areas
- the bootstrap library already replaces some interpreter hardcoding, but not
  all of it
- some docs describe contracts and future expectations that are currently UB or
  best-effort rather than rigidly enforced

## Active Design Tensions

These are the main watchpoints worth carrying into future design discussions:

- The biggest intuition trap is reading constraints as ordinary type
  annotations. The docs repeatedly warn against this.
- The language wants "types as sets" behavior without reducing all set-valued
  reasoning to types.
- `undecided` is a real part of the set model, but user-facing control flow
  still wants mostly ordinary booleans.
- The interpreter-first implementation still contains compromises, while the
  design aims for a more uniform semantic story.
- Mixed-type operator semantics are deliberately deferred.
- Enumerability, finiteness, coextensiveness, purity enforcement, and stronger
  call-signature reasoning are all identified as areas that may still move.

## Practical Reading Order

For future design work, the most useful order is:

1. `README.md` for project state and roadmap
2. `docs/motivation.md` for the "why"
3. `docs/spec/drafts/02_core.md` for the intended semantic model
4. `docs/user_guide/` for the current user-facing mental model
5. `docs/spec/04_lexical.md` and `docs/spec/05_grammar.md` for implemented
   frontend behavior
6. `lib/chirp/boot/00_fundamental.chirp` through `05_testing.chirp` for the
   actual semantic bridge used by the interpreter

That sequence gives the least misleading picture of what Chirp currently is:
an interpreter-backed language prototype whose semantics are increasingly being
defined in Chirp itself, using sets and traits as the common substrate.
