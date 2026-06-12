# Chirp - User Guide - Values and Bindings

## Common Values

You will see these literal forms constantly:

```chirp
1
true
false
"hello"
'a'
#done
[1, 2, 3]
`void
```

Sets are important enough to get their own chapter, so they are covered in
[01_sets.md](./01_sets.md).

## `let` Bindings

The basic form is:

```ebnf
LetStmt = "let" Identifier [ ":" Expr ] "=" Expr ";" ;
```

Examples:

```chirp
let x = 1;
let name : string = "chirp";
let positive : {n : int | n > 0} = 4;
```

A binding can optionally have a constraint after `:`.

- `let x = 1;` lets the initializer determine the value.
- `let x : int = 1;` additionally requires the value to belong to `int`.

## Assignment

Assignment is a statement:

```ebnf
AssignStmt = Expr "=" Expr ";" ;
```

Example:

```chirp
let count = 1;
count = count + 1;
```

Compound assignment also exists:

```chirp
count += 1;
count -= 1;
```

## Blocks

Chirp uses `do { ... }` for block expressions.

```ebnf
BlockExpr = "do" "{" { Statement } [ Expr ] "}" ;
```

Examples:

```chirp
let value = do {
    let x = 1;
    x + 2
};

let early = do {
    break "done";
};
```

- A block is an expression.
- The last bare expression becomes the block's result.
- `break value;` exits the current block immediately with that value.
- `break;` exits with `` `void ``.

## A Small Example

```chirp
let start = 10;

let answer = do {
    let next = start + 1;
    next * 2
};

`print(answer);
```
