# Chirp

Chirp reads like TypeScript-annotated Zig, and it does so with an absurdly small semantic surface. Really.

If you want to see what that looks like, check out [`interpreter/lexer.chirp`](interpreter/lexer.chirp).

If you are curious how in the blazes that is supposed to work, start at [`spec/00_introduction.md`](spec/00_introduction.md).

This repo is all content, no structure for the time being, but you are still more than welcome to poke around.

## State of the project

- The core language design feels stable enough that the remaining abstractions look buildable on top of it. There is still a lot of design and implementation work ahead, but I don’t currently expect any *fundamental* problems to pop up in the model.

- I am very happy with the syntax as it is right now. It exposes the foundations directly, without abstraction, yet reads like Typescript-flavored zig. And it doesn't have *any* comptime/runtime flagging whatsoever.

- I currently don't foresee any major roadblocks to building an interpreter that *works*. Building one that runs *fast* and contains a smart-enough solver will be tough, but there's nothing precluding its existence. As far as I can tell, of course.

## Repo map:

[`interpreter/`](interpreter/) : Not an actual interpreter, just a sandbox for exploring syntax and semantics. What the code DOES is almost certainly out of sync, but the way it goes about it should be a good demonstration of what we're trying to accomplish, especially as far as ergonomics go.

[`spec/`](spec/) : This is where the focus is at the moment. It's not trying to be a legalese-style spec at the moment. Formalism will happen once the dust settles a bit.

[`drafts/`](drafts/) : partially baked ideas that aren't locked down enough to put in the spec yet.
