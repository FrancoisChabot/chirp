# Chirp - User Guide - Structs, Enums, and Match

## Structs

Struct definitions are expressions that produce struct natures. The most common
field shape is:

```ebnf
BasicField = Identifier [ ":" Expr ] [ "=" Expr ] ;
StructExpr = "struct" "{" [ BasicField { "," BasicField } [ "," ] ] "}" ;
```

Example:

```chirp
let Point = struct { x: int, y: int };
let Point3D = struct { x: int, y: int, z: int = 0 };
```

## Constructing Struct Values

Struct values are constructed with ordinary call syntax.

```chirp
let p1 = Point(1, 2);
let p2 = Point(x=1, y=2);
let p3 = Point(y=2, x=1);

let p4 = Point3D(1, 2);
```

- Positional construction follows field order.
- Named construction can be reordered.
- Default field values are used when omitted.

## Field Access

Use `.` to read fields.

```chirp
`print(p2.x);
`print(p2.y);
```

## Enums
```ebnf
EnumExpr = "enum" "{" [ Identifier { "," Identifier } [ "," ] ] "}" ;
```

Example:

```chirp
let Color = enum { Red, Green, Blue };

let c = Color.Red;
`print(c == Color.Red);
```

Enum families also behave as sets of their values, so:

```chirp
`expect(Color.Red ∈ Color);
```

## `match`

`match` is an expression.

```ebnf
SetExpr   = Expr ;
MatchExpr = "match" Expr "{" [ MatchArm { "," MatchArm } [ "," ] ] "}" ;
MatchArm  = SetExpr "=>" Expr ;
```

Examples:

```chirp
let describe(n) = match n {
    {1, 2, 3} => "small",
    4..=6 => "medium",
    `any => "other"
};

let color_name(c) = match c {
    Color.Red => "red",
    Color.Green => "green",
    Color.Blue => "blue"
};
```

Any set-like thing can serve as a match arm condition.

## A Small Example

```chirp
let Shape = enum { Circle, Square };

let area(shape, size : int) = match shape {
    Shape.Circle => size * size,
    Shape.Square => size * size
};
```
