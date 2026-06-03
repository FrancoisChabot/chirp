# Chirp

Chirp reads like TypeScript-annotated Zig, and it does so with an absurdly small semantic surface. Really.

If you want to see what that looks like, check out [`examples/lexer.chirp`](examples/lexer.chirp).

If you are curious how in the blazes that is supposed to work, start at [`docs/spec/00_introduction.md`](docs/spec/00_introduction.md).

This repo is all content, little guidance for the time being, but you are still more than welcome to poke around.

## Getting started

There's not much you can concretely do with chirp at the moment. The interpreter's AST-dump mode works pretty well though, so you can poke around the syntax easily at least. 

**Requirements**: CMake 3.20+, a C++20 compiler and a network connection for `FetchContent()` to be able to do its thing for googletest.

```bash
cmake -B build
cmake --build build && ctest --test-dir build

build/interpreter/chirp --ast-dump examples/lexer.chirp
```

There's also a prompt mode if you invoke `chirp` without any arguments, but it's not much more than a stub at the moment.

### Running the test suite

The [`tests/`](tests/) directory contains a bunch of chirp-written tests,

To run the suite, launch the python script, passing it the chirp executable as parameter.

```bash
python scripts/test_conformance.py path/to/chirp
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

- The core language design feels stable enough that the remaining abstractions look buildable on top of it. There is still a lot of design and implementation work ahead, but I don’t currently expect any *fundamental* problems to pop up in the model.

- I am very happy with the syntax as it is right now. It exposes the foundations directly, without abstraction, yet reads intuitively.

- I currently don't foresee any major roadblocks to building an interpreter that *works*. Building one that runs *fast* and contains a smart-enough solver will be tough, but there's nothing precluding its existence. As far as I can tell, of course.

- The `chirp` executable is good enough to parse chirp code and dump it as AST. It's a far cry from a proper interpreter, but that part is currently reliable enough to build on.

### Current objective

Reach the point where Chirp is useful as an interpreted scripting language, already showing off how it bridges declarative and imperative programming:

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

    print(out);
};
```

> [!NOTE]
> Don't let the Unicode `∈` scare you. You can write `` `in `` instead and use `chirp --format` to swap it out for you. I have it hooked up as my format-on-save in VSCode. 

### Long-term direction

Chirp is interpreter-first, but not interpreter-only. It's ultimately meant to be a metaprogramming systems-level tool. To get there, the roadmap looks roughly like this:

- Step 1: Fully support all of Chirp's semantics in the interpreter's VM
- Step 2: Get the constraint solver to a useful (as-in TypeScript-level-ish) state
- Step 3: Implement the Calcification process to narrow bindings down to representable types.
- Step 4: Implement low-level code emission

How far we can take it from there is unclear. I am reasonably confident that it will work for straightforward single-threaded computations (which is already something!). But where will the framework hit a hard wall? I have no idea at the moment.

## Repo map:

[`examples/`](examples/) : A collection of non-trivial examples of chirp code. 

[`interpreter/`](interpreter/) : A work-in-progress C++ interpreter. At the moment, it can parse chirp code and dump it as AST, as well as do the in-place ASCII->unicode operator replacement.

[`docs/spec/`](docs/spec/) : It's not trying to be a legalese-style spec at the moment. Formalism will happen once the dust settles a bit.

[`docs/design/drafts/`](docs/design/drafts/) : partially baked ideas that aren't locked down enough to put in the spec yet.

[`scripts/`](scripts/) : Developer utilities, including the conformance test runner.

[`tests/`](tests/) : A chirp-based test/conformance suite. Run via `scripts/test_conformance.py`.
