# Chirp

Chirp reads like TypeScript-annotated Zig, and it does so with an absurdly small semantic surface. Really.

If you want to see what that looks like, check out [`interpreter/lexer.chirp`](interpreter/lexer.chirp).

If you are curious how in the blazes that is supposed to work, start at [`spec/00_introduction.md`](spec/00_introduction.md).

This repo is all content, no structure for the time being, but you are still more than welcome to poke around.

## State of the project

- The core language design feels stable enough that the remaining abstractions look buildable on top of it. There is still a lot of design and implementation work ahead, but I don’t currently expect any *fundamental* problems to pop up in the model.

- I am very happy with the syntax as it is right now. It exposes the foundations directly, without abstraction, yet reads like Typescript-flavored zig. And it doesn't have *any* comptime/runtime flagging whatsoever.

- I currently don't foresee any major roadblocks to building an interpreter that *works*. Building one that runs *fast* and contains a smart-enough solver will be tough, but there's nothing precluding its existence. As far as I can tell, of course.

### Current objective

Reach the point where Chirp is useful as an interpreted scripting language, already capable of showing off how it bridges declarative and imperative programming:

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
> Don't let the unicode `∈` scare you. You can write `` `in `` instead and the code formatter `chirp --format` will swap it out for you.

### Long-term direction

Chirp is interpreter-first, but not interpreter-only. It's ultimately meant to be a metaprogramming systems-level tool. To get there, the roadmap looks roughly like this:

- Step 1: Fully support all of Chirp's semantics in the interpreter's VM
- Step 2: Get the constraint solver to a useful (as-in TypeScript-level-ish) state
- Step 3: Implement the Calcification process to narrow bindings down to representable types.
- Step 4: Implement low-level code emission

There is a lot of work ahead, but the path to Futamura-style projection without splitting Chirp into separate compile-time and runtime dialects is there.

## Repo map:

[`interpreter/`](interpreter/) : Not an actual interpreter, just a sandbox for exploring syntax and semantics. What the code DOES is almost certainly out of sync, but the way it goes about it should be a good demonstration of what we're trying to accomplish, especially as far as ergonomics go.

[`spec/`](spec/) : This is where the focus is at the moment. It's not trying to be a legalese-style spec at the moment. Formalism will happen once the dust settles a bit.

[`drafts/`](drafts/) : partially baked ideas that aren't locked down enough to put in the spec yet.
