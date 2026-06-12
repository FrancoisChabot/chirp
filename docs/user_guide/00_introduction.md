# Chirp - User guide

If you haven't read the [motivation](../motivation.md) document yet, you should.

This guide is intentionally small. It covers the core surface language you need
to read and write simple Chirp programs, not every experimental or advanced
feature.

## Suggested Reading Order

1. [Introduction](./00_introduction.md)
2. [Sets](./01_sets.md)
3. [Values and Bindings](./02_values_and_bindings.md)
4. [Functions and Control Flow](./03_functions_and_control_flow.md)
5. [Structs, Enums, and Match](./04_structs_enums_and_match.md)

## Conventions

### Belonging

We use **belonging** for the `∈` relationship instead of the conventional **membership**. This is intentional.

Sets are fundamental to Chirp, not something built on top of it. In fact, most aspects of the language are described in terms of set theory. That becomes a problem as soon as we introduce the `.` operator, because `foo.bar` reads as "the bar member of foo" to most people with OOP experience. As much as we'd like to give sets first dibs on the word, there is too much inertia behind that usage.

So in Chirp land, values **belong** to sets. They are not "members" of sets.

## A Tiny Example

```chirp
let naturals = {x : int | x > 0};
let double(x : naturals) : int = x * 2;

let result = match double(3) {
    1..=5 => "small",
    `any => "big"
};

`print(result);
```

That example already uses most of the guide's core ideas:

- `let` bindings
- set-valued constraints
- function sugar
- ranges
- `match`
