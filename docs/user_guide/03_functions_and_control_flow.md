# Chirp - User Guide - Functions and Control Flow

## Lambdas

Lambda syntax uses `=>`.

```ebnf
BindingNoInit = Identifier [ ":" Expr ] ;
LambdaExpr    = "(" [ BindingNoInit { "," BindingNoInit } ] ")" [ ":" Expr ] "=>" Expr ;
```

Examples:

```chirp
let identity = (x) => x;
let sum = (x : int, y : int) : int => x + y;
```

## Function Sugar

Most named functions in Chirp are written as sugared `let` bindings:

```ebnf
FunctionSugar = "let" Identifier "(" [ BindingNoInit { "," BindingNoInit } ] ")"
                [ ":" Expr ] "=" Expr ";" ;
```

Example:

```chirp
let double(x : int) : int = x * 2;
let choose(flag : bool) : int = if (flag) 1 else 2;
```

This is equivalent in spirit to binding a name to a lambda.

## Calls

Calls use ordinary parentheses.

```ebnf
PositionalArgs = Expr { "," Expr } ;
NamedArgs      = Identifier "=" Expr { "," Identifier "=" Expr } ;
CallExpr       = Expr "(" [ PositionalArgs | NamedArgs ] ")" ;
```

Examples:

```chirp
double(6);
sum(1, 2);
sum(x=1, y=2);
sum(y=2, x=1);
```

- A single call may use either positional arguments or named arguments, but not
  both at once.

## `if`

`if` exists both as an expression and as a statement.

Expression form:

```ebnf
IfExpr = "if" "(" Expr ")" Expr "else" Expr ;
```

Statement form:

```ebnf
IfStmt = "if" "(" Expr ")" Statement [ "else" Statement ] ;
```

Examples:

```chirp
let x = if (true) 1 else 2;    // else is mandatory

if (x > 0) `print("positive"); // no else necessary
```

## `while` and `for`

Loop forms are expressions, though in practice they are usually used for their
effects.

```ebnf
WhileExpr = "while" "(" Expr ")" Expr ;
ForExpr   = "for" "(" BindingNoInit "∈" Expr ")" Expr ;
```

Examples:

```chirp
let n = 1;
while (n <= 3) do {
    `print(n);
    n += 1;
};

for (v ∈ 1..=3) do {
    `print(v);
};
```

- `for` currently iterates only over ranges.

## A Small Example

```chirp
let factorial(n : int) : int = do {
    let acc = 1;
    let i = 1;
    while (i <= n) do {
        acc *= i;
        i += 1;
    };
    acc
};
```
