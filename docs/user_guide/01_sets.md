# Chirp - User Guide - Sets

Sets are all over the place in chirp and get first billing syntax-wise. Being a set
in chirp is a ``trait` a type can have. Any value that implements it can be used as 
a set. This includes literals, types, predicates, function signatures, traits, ranges and really 
anything for which "is this value part of what you represent?" is a sensible question.

## Set Literals

In expression position, braces almost always introduce a set literal. That can mean either 
an Enumerated set or a Constructed set.

One important exception exists: `{x=1, y=2}` is parsed as an anonymous struct
literal, not as a set. This chapter focuses only on the set forms.

### Enumerated Sets

```ebnf
EnumeratedSet = "{" [ Expr { "," Expr } [ "," ] ] "}" ;
```

An enumerated set stores the values produced by its element expressions.

```chirp
{}                    // empty set
{3}                   // singleton set
{2 * 2, 3 * 3}        // values are evaluated when the set is built
{1, 2, 3, 5, 7, 11}
{{1, 2}, {3, 4}}
{"Hello", 3}
```

### Constructed Sets

```ebnf
SetExpr        = Expr ;
ConstructedSet = "{" Identifier [ ":" SetExpr ] "|" Expr "}" ;
```

A constructed set binds a candidate value to the identifier on the left of `|`
and evaluates the expression on the right to decide membership.

If a bound is present after `:`, that bound is checked first. The predicate
expression itself must evaluate to `Bool` in the current interpreter.

```chirp
let even_numbers = {x : int | x % 2 == 0};
let naturals = {x : int | x > 0};
let answer = {x | x == 42};
```

Notes:

- The optional bound must itself be a set-valued expression.
- `{x : int | x > 0}` and `{x | x ∈ int && x > 0}` are equivalent.

## Other Built-In Set Values

Not every set in Chirp is written with braces.

### Singleton Values

Literal values with built-in setness behave like singleton sets in membership
tests and constraints.

```chirp
`expect(1 ∈ 1);
let only_one : 1 = 1;
```

### Ranges

```ebnf
RangeExpr = Expr ( ".." | "..=" ) Expr ;
```

Ranges are sets with an inclusive start bound and either an exclusive or
inclusive end bound.

```chirp
1..5     // 1, 2, 3, 4
1..=5    // 1, 2, 3, 4, 5
'a'..='d' // 'a', 'b', 'c', 'd'
```

### Types

Types are sets of their instances.

```chirp
`expect(3 ∈ int);
`expect("hello" ∈ string);
`expect(!(true ∈ int));
```

This matters in both constraints and `match` expressions.

### Enum Families

Enum family values are sets of their variants.

```chirp
let Color = enum { Red, Green, Blue };

`expect(Color.Red ∈ Color);
`expect(Color.Green ∈ Color);
```

### Traits 

Traits act as sets of the values whose type implements the trait.

```chirp
let f : `Callable = (x) => x;
```

## Signature Sets

Signature expressions look like lambdas without a body.

```ebnf
SignatureExpr = "(" [ BindingNoInit { "," BindingNoInit } ] ")" "->" SetExpr ;
BindingNoInit = { "mut" | "final" } ( Identifier | Intrinsic ) [ ":" SetExpr ] ;
```

Example:

```chirp
let sig = (x: int, msg: string) -> void;
let print_msg = (x: int, msg: string) => `print(f"{x} - {msg}");

let callback: sig = print_msg;
callback(1, "yo");
```

Current limitation:

- Signature membership is only partially implemented today. For ordinary
  lambdas, the interpreter checks arity. It does not fully enforce parameter
  bounds or return constraints yet.

That means signature values are useful, but they are not yet a complete
call-shape checker.

## Set Operators

The implemented set operators are union, intersection, and complement.

```ebnf
SetUnionExpr        = SetExpr "∪" SetExpr ;
SetIntersectionExpr = SetExpr "∩" SetExpr ;
SetComplementExpr   = "~" SetExpr ;
```

Examples:

```chirp
let fizz = {x : int | x % 3 == 0};
let buzz = {x : int | x % 5 == 0};

let fizz_or_buzz = fizz ∪ buzz;
let fizz_and_buzz = fizz ∩ buzz;
let not_fizz = ~fizz;
```

Notes:

- Complements are intentionally treated as non-enumerable in the current
  interpreter.

## Using Sets

### Membership

Use `∈` and `∉`.

```ebnf
MembershipExpr = Expr ( "∈" | "∉" ) SetExpr ;
```

```chirp
let even_numbers = {x : int | x % 2 == 0};

`print(4 ∈ even_numbers);   // true
`print(5 ∉ even_numbers);   // true
```

### Constraints

Any set-valued expression can be used as a binding constraint.

```chirp
let naturals = {x : int | x > 0};
let v: naturals = foo();
let score : 0..=100 = 87;
let even : {x : int | x % 2 == 0} = 4;
```

If the value does not belong to the constraint set, evaluation fails.

### Match Patterns

`match` treats set-valued patterns as membership tests.

```ebnf
MatchExpr = "match" Expr "{" [ MatchArm { "," MatchArm } [ "," ] ] "}" ;
MatchArm  = Expr "=>" Expr ;
```

```chirp
let classify(v) = match v {
    {1, 2, 3} => "small",
    4..=6 => "medium",
    int => "some other integer",
    `any => "unknown"
};
```

That works because:

- enumerated sets are sets
- ranges are sets
- types are sets
- enum families are sets
- traits are sets

If a pattern is not set-valued, `match` falls back to ordinary equality.
