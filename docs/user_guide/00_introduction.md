# Chirp - User guide

If you're finding it hard to wrap your head around the language, reading the [motivation](../motivation.md) document might help provide context.

This guide is intentionally small and to the point. We'll be adding details around areas that people find confusing as 
we discover them.

## Suggested Reading Order

1. [Introduction](./00_introduction.md)
2. [Sets](./01_sets.md)
3. [Values and Bindings](./02_values_and_bindings.md)
4. [Functions and Control Flow](./03_functions_and_control_flow.md)
5. [Structs, Enums, and Match](./04_structs_enums_and_match.md)
6. [Traits](./05_traits.md)

## Conventions

### Belonging

We use **belonging** for the `∈` relationship instead of the conventional **membership**. This is intentional.

Sets are fundamental to Chirp, not something built on top of it. In fact, most aspects of the language are described in terms of set theory. That becomes a problem as soon as we introduce the `.` operator, because `foo.bar` reads as "the bar member of foo" to most people with OOP experience. As much as we'd like to give sets first dibs on the word, there is too much inertia behind that usage.

So in Chirp land, values **belong** to sets. They are not "members" of sets.

## A Tiny Example

```chirp
let naturals = {x : int | x > 0};
let double(x : naturals) : naturals = x * 2;

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
