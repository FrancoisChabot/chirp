# Chirp

Chirp reads like TypeScript-annotated Zig, and it does so with an absurdly small semantic surface. Really.

If you want to see what that looks like, check out [`examples/lexer.chirp`](examples/lexer.chirp).

If you are curious how in the blazes that is supposed to work, start at [`docs/spec/00_introduction.md`](docs/spec/00_introduction.md).

This repo is all content, little guidance for the time being, but you are still more than welcome to poke around.

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
> Don't let the Unicode `∈` scare you. You can write `` `in `` instead and use `chirp --format` to swap it out for you. I have it hooked up as my format-on-save in VSCode. 


### Running the test suite

The [`tests/`](tests/) directory contains a bunch of chirp-written tests,

It's hooked up to run automatically when invoking on ctest. But you can also run it manually, passing it the chirp executable as parameter.  

```bash
python scripts/test_conformance.py -j auto path/to/chirp
```

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

### Current objective

Fully support all of Chirp's semantics in the interpreter's VM. The core syntax is there, but there are still some important pieces missing:

- C-ABI FFI

### Roadmap

Chirp is interpreter-first, but not interpreter-only. It's ultimately meant to be a metaprogramming systems-level tool. But at the same time, the dynamic interpreter meta-language makes a suitable scripting tool in its own right, as a sort of "dynamically enforced TypeScript".

Because of this, the roadmap is split up in two:

#### Part 1: Dynamic Chirp

1. Fully support all of Chirp's semantics in the interpreter's VM (IN PROGRESS)

MILESTONE: Start publishing Dynamic Chirp v0.1

2. Populate the standard library
3. Perform an optimization pass on the interpreter

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

[`lib/chirp/std`](lib/chirp/std) : This is where the standard library will live.

[`docs/spec/`](docs/spec/) : It's not trying to be a legalese-style spec at the moment. Formalism will happen once the dust settles a bit.

[`docs/design/drafts/`](docs/design/drafts/) : partially baked ideas that aren't locked down enough to put in the spec yet.

[`scripts/`](scripts/) : Developer utilities, including the conformance test runner.

[`tests/`](tests/) : A chirp-based test/conformance suite. Run via `scripts/test_conformance.py`.
