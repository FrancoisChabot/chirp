# Chirp - User Guide - Traits

Traits are first-class values in Chirp.

The two most important things to know are:

- a trait is a set of values
- a trait can also carry an interface describing the implementation data
  attached to a nature

In practice, the most important use of that interface is as a vtable: an
implementation value that stores the operations for a nature.

That is why traits work naturally as constraints:

```chirp
let f : `Callable = (x) => x;
```

## Creating A Trait

Traits are created with `` `make_trait ``.

```ebnf
MakeTraitCall = "`make_trait" "(" Expr ")" ;
```

The argument is the trait interface.

### Marker Traits

The simplest trait uses `void` as its interface.

```chirp
let Marker = `make_trait(void);
```

This says: implementations of `Marker` carry no extra data beyond the fact that
the nature implements the trait.

### Traits With Data

Traits can also require a structured implementation value.

```chirp
let Tagged = `make_trait(struct {
    tag: bool
});
```

Now an implementation must provide a value belonging to that interface.

## Implementing A Trait

Use `` `implement `` to attach a trait implementation to a nature.

```ebnf
ImplementCall =
    "`implement" "("
        "trait" "=" Expr ","
        "on" "=" Expr ","
        "impl" "=" Expr
    ")" ;
```

Examples:

```chirp
let Marker = `make_trait(void);

`implement(
    trait=Marker,
    on=int,
    impl=void
);
```

```chirp
let Tagged = `make_trait(struct { tag: bool });

`implement(
    trait=Tagged,
    on=int,
    impl={tag=true}
);
```

Important points:

- `on=` is usually a nature value such as `int` or a struct nature.
- `impl=` must belong to the trait's interface.
- only one implementation may exist for a given `(trait, on)` pair.

## Traits As Vtables

This is the most important day-to-day use of traits.

When a trait's interface is a struct nature, the implementation value acts like a
vtable for the target nature: it stores the functions that define that behavior.

```chirp
let Show = `make_trait(struct {
    show: (self) -> string
});

let Point = struct { x: int, y: int };

`implement(
    trait=Show,
    on=Point,
    impl={
        show = (self) => f"Point({self.x}, {self.y})"
    }
);

let render(x : Show) = do {
    let impl = `implementation(Show, `nature_of(x));
    impl.show(x)
};

`print(render(Point(1, 2)));
```

>[!NOTE]
> There's going to be syntactic sugar over this soon. `Show(x).show()` most likely.

## Traits As Sets

A trait behaves as the set of values whose nature implements that trait.

```chirp
let Marker = `make_trait(void);

`implement(
    trait=Marker,
    on=int,
    impl=void
);

`expect(1 ∈ Marker);
`expect("" ∉ Marker);
```

That also means traits work naturally in constraints and `match`:

```chirp
let constrained : Marker = 5;

let describe(v) = match v {
    Marker => "implemented",
    `any => "not implemented"
};
```

## Inspecting Traits

The basic helper functions are:

- `` `interface_of(trait) ``: returns the trait's interface
- `` `implements(trait, on) ``: checks whether a nature has an implementation
- `` `implementation(trait, on) ``: returns the registered implementation value

Example:

```chirp
let Marker = `make_trait(void);

`implement(
    trait=Marker,
    on=int,
    impl=void
);

`expect(`interface_of(Marker) == void);
`expect(`implements(Marker, int));
`expect(`implementation(Marker, int) == void);
```

## Built-In Traits

Bootstrapping already provides these traits:

- Core semantic traits:
  `` `Set ``, `` `Callable ``
- Memory and reference traits:
  `` `Drop ``, `` `Unique ``, `` `Dereferenceable ``, `` `DereferenceableMut ``
- Operator traits:
  `` `Comparable ``, `` `Additive ``, `` `Subtractive ``, `` `Negatable ``,
  `` `Multiplicative ``, `` `Divisible ``, `` `Modulable ``, `` `Indexable ``,
  `` `IndexAssignable ``

Several language operators are routed through these traits by the interpreter:

- `∈` / `∉` use `` `Set `` (implementing `Set actually enables those values to be used as sets ANYWHERE)
- call syntax uses `` `Callable ``
- `==`, `!=`, `<`, `<=`, `>`, `>=` use `` `Comparable ``
- `+` uses `` `Additive ``
- `-` uses `` `Subtractive `` and unary `-` uses `` `Negatable ``
- `*` uses `` `Multiplicative ``
- `/` uses `` `Divisible ``
- `%` uses `` `Modulable ``
- indexing `x[...]` uses `` `Indexable ``
- indexed assignment uses `` `IndexAssignable ``
- dereference behavior can route through `` `Dereferenceable `` and
  `` `DereferenceableMut ``

If you implement any arithmetic traits, you need to be aware that the interpreter is free to apply the usual transformations. `(a+b) == (b+a)`, etc...

## A Small Example

```chirp
let Counter = struct { value: int };

`implement(
    trait=`Additive,
    on=Counter,
    impl={
        add = (self, other) => Counter(value=self.value + other.value)
    }
);

let a = Counter(value=2);
let b = Counter(value=5);
let c = a + b;

`expect(c.value == 7);
```
