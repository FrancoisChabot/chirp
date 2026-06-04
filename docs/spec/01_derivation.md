# Deriving Chirp’s Core Model

## Preamble

Do not take anything in here to the letter. The goal of this chapter is to contextualize the Chirp spec.

I am also going to rebuild some familiar ideas from first principles and intentionally avoid jargon for two reasons:

- I want this to be understandable by systems-level coders without a glossary attached to it.
- I find going through every step of the logic chain helps understand WHY things work the way they do.

## The Setup

Chirp is an engineered language. So to trace back where it comes from, we need to approach this from an engineering angle: We are building something that's meant to achieve a goal, under some constraints, by following a strategy. With a lot of rigor and a bit of creativity, that should lead us to something that works.

### The goal

I like Zig's model, and I also like TypeScript's expressiveness. It's hard to imagine two languages further apart (ok, it's pretty easy... but they are still very different beasts), so fusing them together seems like a suitable act of hubris for this vanity project. 

The holy grail would be if we could get something along the lines of Rust’s borrow checker as an emergent feature of a more general correctness engine. I've always felt that it's a shame it has so much syntactical noise just to handle a very specific category of bugs.

And let's try to make this as squeaky-clean as possible. The fewer primitive building blocks and special cases, the better. This is partly because it makes the language's mental model simpler, but it also makes the compiler more robust, and I'd be lying if I tried tp pretendaesticism and personal satisfaction didn't come into it.

### The constraints

### Constraint 1: Value representability

At the end of the day, this needs to compile down to machine code. I will be shoving `int64`s and `double`s into registers, and storing struct-equivalents in memory. As such, sooner or later, a value needs to break down to a **predictable** layout without a mandatory level of indirection (indirection is acceptable, as long as it's an opt-in).

### Constraint 2: Polymorphism

Unions, traits, protocols, etc... are just too useful to be left on the table. This clashes against constraint 1, so it bears calling out.

### Constraint 3: Top-to-bottom semantic consistency

In a perfect world, compile-time meta-programming should be indistinguishable from runtime code. 

This one has a lot more wiggle room for compromises. But this project is my "ship in a bottle", so if I have to relax it, it won't be for lack of trying to preserve it.

### The Strategy

Generalize the crap out of everything and mess with the model and the syntax until:
- Syntax starts breaking down
- Implementation becomes impossible
- runtime performance is compromised
- One of the constraints is compromised
- We reach an irreducible set of primitives

I think of it as a gradient descent through the design space.

## The Journey

### The red herring

Off the get-go, Constraint 2 forces our hand:

**If we need a variable to act differently based on what happens to be stored in it (polymorphism), then it can hold values of different types.**

Taking this to the extreme would be "any variable can hold a value of any type" which is JavaScript/Python and... just... **NO**.

Walking it back a bit would be "A variable can hold some values, but not necessarily all of them", which is really just a fancy way to say that a variable has a **set** of values it's allowed to have. That's nice because it allows for anything between a single value (a const), all the way to `any` if we ever need it. 

Nevertheless, practically speaking (constraint 1), I still need to have control over the type of a variable. I must be able to say `let x: int64 = 3;` and be able to *trust* that this optimizes all the way down to registers. So... types are sets, I guess.  

Leaning into types-as-sets, we can immediately see `let x: int64 ∪ bool = foo();` as a pretty sweet syntax for declaring unions. 

Actually... if we also assume types are values (which is sort-of required for constraint 3 anyways), we get:

```
let int_or_bool = int64 ∪ bool;

let x: int_or_bool = foo();
```

No need for a union keyword! That's a win, we can probably generalize that to type definitions at large:

```
let Point = struct {
  x: float,
  y: float,
};

// Why the hell not?
let point_list = Vector(Point);

let v : point_list = point_list(); // Roughly
```

Nothing new under the sun here. We are in clear TypeScript land. But I do like the look and feel of it, and constraint 3 is on the right track. The compilation rules needed for this to bake down reliably to machine code will be tricky, but them's the breaks. If I want constraint 3, I'll have to reckon with that sooner or later, so there's no reason to toss this. I *like* it.

If `int_or_bool` is *just* a set by joining two types, can we just generalize and throw in arbitrary sets where a type normally fits?

```
let u : {1,2,3} = 1;
let v : 0..9 = 3;
```

Looks like it! And it looks *slick*. I can already see the curly brace set literals causing grammar issues (especially if I want to support expression blocks). But this is so *good* that I have to see where it goes.

(insert part-time years of throwing spaghetti at the wall, I'll spare you the endless dead-ends).

This is a complete local minimum. We have to get over a hill.

### The epiphany: Actually types are NOT sets (except when we need them to be)

You know what? This whole Type <-> Set equivalency is suspect. 

It doesn't even make practical sense. There are a myriad of situations where I'd use a set as a value instead of a type. So sets are value-like first and foremost, using them in a type-ish context is secondary. But at the same time, I still need to be able to express "the set of all ints" cleanly, and types are well suited for that. 

Let's pull on that: `{1,2,3}` is a *value*, which means it has a *type* (that type being StaticallyEnumeratedSet or something along these lines). Same goes for `0..9`, which is a *value* of *type* IntRange or whatever. So right off the bat, I have two different types whose values behave like sets, so I need these types to have a property that allows their instances to be used as sets. A sort of set-ness, defined by a function that determines if a value belongs to the set: A *Belonging Predicate* (`bp`). 

SPOILERS: Later we'll realize this can't be a predicate in every single case, but we'll stick to it by convention.

That's basically a trait (Rust)/interface (Java)/protocol (ObjC) etc... I know those! And they are explicitly something I'm supposed to support eventually (see Constraint #2). Nothing stops me from recursively using trait-like capabilities as part of the definition of the system itself, as long as the loop closes. That's not any different than a garden-variety recursive function.

So... What **if** sets weren't a *thing*, and set-ness was instead a capability that certain Types provide for their instances? Then types don't have to be sets, they only need to supply belonging behavior when used as sets. It would also mean that sets don't have to be types either. Let's keep pulling on that thread and see what happens.

We can start laying down some rules.

- Every Value has an intrinsic type
- Sets are values whose intrinsic Type supplies a `bp`, roughly a function from `(this, v: any) : bool` (that's what "does something belong to a set" reduces down to at the end of the day...)
- Variables (or whatever the fundamental building block they are made of, let's call that **bindings**) have a *current value* and *a set of values they can have*

### Bindings

I've written enough TypeScript to know that we can generalize this even more. In TS, which value can be assigned to a Binding doesn't depend only on how it's declared, but also *where* in the code that assignment happens. 

So really, a Binding has:
- A set of values it can have *in general*
- A set of values it can have *at this specific point in the program* 
- A single value it has *right now* in a real temporal, wall-clock, sense.

Let's draw this out, with `fc` for the fundamental constraint, `lc` for the local constraint, and `cv` for the current value. 

Since every one of these is narrower or equal to the one before it, down to a point for `cv`, it forms a funnel (or a stick when dealing with a const):

```
  mutable     const
\   fc   /    |fc|
 \      /     |  |
  \ lc /      |lc|
   \  /       |  |
    cv         cv
```

That should give us all the necessary building blocks to express TypeScript-style Flow-Sensitive Typing (it's just the compiler messing with `lc`).

Wait a sec! There's another thing that seems backwards. The shape is not a stick because the binding is a const. The binding is a const because the shape is a stick. If I play my cards right, I should be able to twist this into making constness an emergent feature of the language!

I wonder what else I can generalize with this model? If I combine it with the set-ness capability model for `fc` and `lc`, I might be able to take this surprisingly far...

From here, we can plausibly satisfy contraint #1:

"If the set of values a Binding can hold just happens to be of a single type, then the compiler can treat it the same way C treats a variable."

**Note to self:**
`fc` `lc` and `cv` are "properties of bindings". We could *technically* generalize the idea and give bindings an open-ended set of properties, but this is where we are hitting diminishing abstraction returns. We've already seen mutability can emerge from the three we have so far, so I say let's pin that and revisit it if/when a motivating example can be found.

## Where do values come from?

I still need to figure out how actual code that *computes* things sits on top of that, but as long as I can express it in terms of everything we have so far, and as long as it can be done sanely with those building blocks, I'm good. It becomes an acceptance criterion. SPOILERS: It's feasible, but messy. Going over it here would just muddy the waters.

## One final wrinkle 

I'm not going to go over the whole derivation here, but if you try and lay this all out as is, you run into some classic paradoxes. It boils down to the fact that it is still possible to express a set that cannot resolve to either `true` or `false` for certain values.

For example, imagine a set whose rule is "it contains every set that does not contain itself". Now ask whether that set contains itself. If the answer is `true`, then by its own rule it should be `false`. If the answer is `false`, then by its own rule it should be `true`.

That means set-ness needs to operate on ternary (`true`/`false`/`undecided`) logic in *some* cases. That would be too much of a pain-in-the-butt to force on users when all they want to do is `if (some_char ∈ {' ', '\t', '\r', '\n'})`. We'd definitely be into "Syntax starts breaking down" territory if that didn't evaluate to a bool. 

So **most** `bp`s evaluate to a `{true, false}` set, and **some** evaluate to `{true, false, undecided}`, and sets must be able to announce which ahead of time. 

We might as well allow sets to announce `{true}`, `{false}`, `{undecided}`, while we are at it. That's probably useful for optimization. 

In other words, set-ness needs a second method: one that takes a set of "candidate" values and returns the set of answers its `bp` can produce. The technical term for "the set of values a function can return" is its range, so we'll call this the set's **Belonging Range** (`br`). That "set of candidate values" may sound nebulous, but in `b ∈ S`, that's just `b`'s `lc`!

`br` is a pre-flight check. `undecided` being in it does not mean a particular belonging test failed to decide. It means that, for this set of candidate values, evaluating `bp` could produce `undecided`. At the same time, if `br` contains only a single value, invoking `bp` is unnecessary.

In practice, `br` does not need to be magic. A boring implementation can be conservative.
- Is `bp` a DAG -> `{true, false}`, otherwise `{true, false, undecided}`.
- Some sets can do better because their answer is obvious. `any` always returns `true`, so its `br` is `{true}`, etc...
- Custom sets can provide their own `br`, but that is a promise: “my `bp` will only ever return one of these answers.” If that promise is false, all bets are off.

This is easilly confusing, so an example can help:

```
let EvenNumber = {x | x ∈ int && x % 2 == 0};

let foo(s: string, v: int ) = do {
  let test_string = s ∈ EvenNumber; // br is {false} because x ∈ int is guaranteed to fail. this can be optimized out
  let test_num = v ∈ EvenNumber;    // br is {true, false}, so it becomes a runtime computation.
};
```

Finally, it's pretty subtle so it's worth pointing out: In practice, a `bp` will return `undecided` by the interpreter giving up after some cost threshold has been reached. However, if a `br` is just `{true, false}`, then the interpreter can be confident that there's going to be an answer eventually. This reduces the risk of cost-based decidability causing false positives.

Now set-ness looks more like: "The **Belonging Predicate** of a set returns an element of its **Belonging Range**" (`bp(S, b) ∈ br(S, b.lc)`). This way, we can ensure that users will be dealing with garden variety booleans in most scenarios, and will have to face ternary logic only when doing something suspect.

## Conclusion

Putting all of that together, we end up with an architecture that looks like this:

```text
                ┌────────────────────────────────────────────────────┐
                │                     Traits                         │◄──┐
                └────────────────────────────────────────────────────┘   │
                ┌────────────────────────────────────────────────────┐   │
                │                   Computation                      │   │
                └────────────────────────────────────────────────────┘   │
                                        ...                              │
                ┌────────────────────────────────────────────────────┐   │
                │                     CORE                           │   │
                │  ┌─────────┐                        ┌─────────┐    │   │
                │  │  VALUE  │◄───────────────────────│ BINDING │    │   │
                │  └─────────┘                fc/lc/cv└─────────┘    │   │
                │   has│▲   ▲                               ▲ │ fc/lc│   │
                │      ││   │                               │ │      │   │
                │      ││   │                    br uses lc │ │      │   │
                │      ││   │ bp tests values               │ ▼      │   │
                │      ▼│is └─────────────────────────┌─────────┐    │   │
                │  ┌─────────┐                        │ SETNESS │────│───┘ is
                │  │  TYPE   │-----------------------►└─────────┘    │
                │  └─────────┘ can implement                         │
                └────────────────────────────────────────────────────┘
```



That's a **lot** of circular dependencies, but I have yet to find a way to break it. This could really use a proper proof, but I'm not really a theorem kind of guy. If the language works, I'm sure some grad student will be delighted to make a paper out of doing it for me :) . 

Computation is its own can of worms and may eventually get a similar companion reasoning file. But you should now be equipped with all of the intuition you need to go through the rest of the spec.

Next up: [The Core](02_core.md), where we'll see a more tightly formalized version of this architecture.
