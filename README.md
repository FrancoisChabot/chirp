# Chirp

Chirp is being built as a systems programming language. Two things make its design unusual:

- Chirp code is a *script* that defines the program to be compiled, using the same machinery for metaprogramming as the code being built. 
- It uses a powerful and extensible set-theoretic constraint system in which guarantees are garden-variety values.

These two things play off of each other to create an unusually expressive and (I think) harmonious way to define compiled programs using surprisingly few building blocks.

> [!NOTE]
> Chirp is very much a work-in-progress. As of today, it's a perfectly workable scripting language. Albeit one with a very limited standard library.

## Getting started

**Requirements**: CMake 3.20+, a C++20 compiler and a network connection for `FetchContent()` to be able to do its thing for googletest.

```bash
cmake -B build
cmake --build build && ctest --test-dir build

build/interpreter/chirp script.chirp
```

Here's a sample script you can try:

```chirp
let fizz = { x:int | x % 3 == 0 };
let buzz = { x:int | x % 5 == 0 };

for (v ∈ 1..100) do {
    let out = match v {
        fizz ∩ buzz => "fizzbuzz",
        fizz => "fizz",
        buzz => "buzz",
        `any => v
    };

    `print(out);
};
```

> [!NOTE]
> Don't let the Unicode `∈` and `∩` scare you. You can write `` `in `` and `` `and `` instead and use `chirp --format` to swap it out for you. I have it hooked up as my format-on-save in VSCode. 

### Misleading intuition warning

If you are a seasoned developer and, as I suspect will likely be the case, you go have a look at some code samples before reading the spec, you will find yourself in *almost* familiar territory, and there's a real risk of misinterpreting what's going on.

```
                This
                 |
                 v
let some_var : int64 = 3;
```

Is NOT the variable's type in Chirp. Thinking about it this way is not the end of the world and won't prevent you from using the language, but it shackles you in a way where you'd miss out on a lot of what the language has to offer. The full explanation is in the [`docs/spec/`](docs/spec/). The first two chapters are approachable, so don't be shy.

## State of the project

### How stable is Chirp?

- The syntax and grammar are quite stable at this point. They aren't formally locked down yet though.
- The semantics are "mostly" stable, but there's a few points that are still a bit shaky
  - Set enumerability, finiteness and coextensiveness are likely to undergo tweaking.
  - Call signature checks are still a bit too loose for my liking.
  - Supersets and subsets are intentionally not dealt with right now, but we need to reckon with them eventually.
  - Function purity is not enforced quite as hard as it should be yet.
  - Mixed-typed binary operators are currently not supported, but will be.
- The standard library is not to be trusted at all for the time being.

### Current objectives

- Bring the documentation back in sync with the implementation. It has fallen behind.
- Develop an IR-based interpreter, so that the static solver has something to manipulate.
- Start populating the standard library.

### Roadmap

Chirp is interpreter-first, but not interpreter-only. It's ultimately meant to be a metaprogramming systems-level tool. But at the same time, the dynamic interpreter meta-language makes a suitable scripting tool in its own right, as a sort of "dynamically enforced TypeScript".

Because of this, the roadmap is split up in two:

#### Part 1: Dynamic Chirp

1. Fully support all of Chirp's semantics in the interpreter's VM (COMPLETE)
2. Get the repo to a publishable state. (IN PROGRESS)

MILESTONE: Start publishing Dynamic Chirp v0.1

3. Populate the standard library
4. Perform an optimization pass on the interpreter

MILESTONE: Evaluate the road to a Dynamic Chirp v1.0

#### Part 2: Static Chirp

1. Implement the static solver, the target should be TypeScript-ish level of provability
2. Implement the Calcification process to narrow bindings down to representable types.
3. Implement low-level code emission

MILESTONE: Start publishing Static Chirp v0.1

How far we can take it from there is unclear. I am reasonably confident that it will work for straightforward single-threaded computations (which is already something!). But where will the framework hit a hard wall? I have no idea at the moment.

## Repo map:

[`examples/`](examples/) : A collection of non-trivial examples of chirp code. 

[`interpreter/`](interpreter/) : A work-in-progress C++ interpreter. It can run Chirp scripts, load boot sources, dump ASTs, and do in-place ASCII->unicode operator replacement.

[`lib/chirp/boot`](lib/chirp/boot) : Chirp tries to define itself via its own language as much as possible. This is the Chirp-specified bridge between the raw interpreter and user code.

[`lib/chirp/std`](lib/chirp/std) : This is where the standard library lives.

[`docs/spec/`](docs/spec/) : It's not trying to be a legalese-style spec at the moment. Formalism will happen once the dust settles a bit.

[`docs/design/drafts/`](docs/design/drafts/) : partially baked ideas that aren't locked down enough to put in the spec yet.

[`scripts/`](scripts/) : Developer utilities, including the conformance test runner.

[`tests/`](tests/) : A chirp-based test/conformance suite. Run via `scripts/test_conformance.py`.

## Contributing

If you are looking at contributing to chirp itself, here's a few things to know.

### Running the test suite

The [`tests/`](tests/) directory contains a bunch of chirp-written tests.

It's hooked up to run automatically when invoking ctest. But you can also run it manually, passing it the chirp executable as a parameter.  

```bash
python scripts/test_conformance.py -j auto path/to/chirp
```
