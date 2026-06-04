# Chirp Grammar Draft

> **Parser artifact note:** This grammar describes surface-level syntax for the
> bootstrap parser. The resulting AST is a transient artifact used to populate
> interpreter IR; it is not the structure the interpreter is expected to execute
> directly. Syntactic acceptance does not imply semantic validity: the AST may
> contain constructs that are guaranteed to be rejected later, such as
> `(1 + 3) = 5;`.

This file describes the parser behavior in the current C++ bootstrap frontend at `interpreter/lib/frontend`. It is not a complete language grammar for every draft idea in this repository.

## Lexical Notes

The bootstrap lexer recognizes UTF-8 source text, skips standard whitespace, and supports `//` line comments.

```ebnf
Identifier       = (Alpha | "_") { Alpha | Digit | "_" } ;
Intrinsic        = "`" Identifier ;
SymbolicConstant = "#" Identifier ;
Number           = Digit { Digit } [ "." Digit { Digit } ] ;
String           = '"' { StringChar } '"' ;
StringChar       = SourceCharExceptDoubleQuote | "\" SourceChar ;
Character        = "'" ( Utf8Codepoint | Escape | UnicodeEscape ) "'" ;
Escape           = "\" ( "\" | "'" | '"' | "n" | "r" | "t" | "0" ) ;
UnicodeEscape    = "\u" HexDigit HexDigit HexDigit HexDigit ;
```

`-` is always tokenized as an operator. Negative numeric values are parsed as unary negation applied to a number expression, such as `-1`.

Character literals contain exactly one Unicode scalar value. Raw character literals spell that scalar value directly as UTF-8 source text. Escaped literals may use the common one-character escapes listed above or `\uXXXX`; surrogate code units in `\uD800` through `\uDFFF`, empty character literals, multi-codepoint literals, and malformed escapes are lexical errors.

The POC currently recognizes Unicode set operators directly: `∈`, `∉`, `⊆`, `⊂`, `⊄`, `⊇`, `⊃`, `⊅`, `∪`, and `∩`. ASCII aliases such as `in`, `union`, or `subset` are out of scope for the current parser.

The range operator family is tokenized by longest match:

| Token | Meaning |
| --- | --- |
| `..` | inclusive start, exclusive end |
| `..=` | inclusive start, inclusive end |

## Program And Statements

```ebnf
Program      = { Statement } EOF ;
Statement    = LetStmt | BreakStmt | IfStmt | AssignmentStmt | ExprStmt ;

LetStmt      = "let" BindingWithInitializer ";" ;
BreakStmt    = "break" [ Expr ] ";" ;
IfStmt       = "if" "(" Expr ")" IfBranch [ "else" IfBranch ] [ ";" ] ;
ExprStmt     = Expr ";" ;
AssignmentStmt = Expr AssignmentOp Expr ";" ;
IfBranch     = BraceExpr | Statement ;

AssignmentOp = "=" | "+=" | "-=" | "*=" | "/=" | "%=" ;
```

Assignment targets are syntactically parsed as ordinary expressions in the POC. L-value validity is intentionally left for later semantics and IR lowering.

`if` without `else` is valid only as `IfStmt`. `IfExpr` remains expression-level and requires an `else` branch.

A leading `if` in statement position parses as `ExprStmt` when it is the expression form:

```chirp
if (cond) a else b;
```

It parses as `IfStmt` when it is else-less or when its branches are statement-only forms such as assignments:

```chirp
if (cond) x = 1; else x = 2;
```

`break` targets the nearest enclosing block expression. `break;` yields `` `void ``.

Bindings:

```ebnf
BindingName            = Identifier | Intrinsic ;
BindingModifier        = "mut" | "final" ;
BindingWithInitializer = { BindingModifier } BindingName [ ParamList ] [ ":" Expr ] "=" Expr ;
BindingNoInitializer   = { BindingModifier } BindingName [ ":" Expr ] ;
NamedBinding           = { BindingModifier } BindingName [ ":" Expr ] [ "=" Expr ] ;
FieldBinding           = NamedBinding | { BindingModifier } BindingName ParamList [ ":" Expr ] "=" Expr ;

ParamList = "(" [ BindingNoInitializer { "," BindingNoInitializer } ] ")" ;
```

`let name(params): bound = body;` is function sugar. The parser lowers it to a let binding whose initializer is a lambda with the parsed parameters, optional return bound, and body expression.

`mut` and `final` are contextual modifiers in binding position. Outside that position they are ordinary identifiers. `final` marks a binding as unshadowable by descendant scopes; it does not imply assignment immutability.

Backtick-prefixed binding names are accepted by the parser so boot sources can define public intrinsics, but ordinary user code is not allowed to define them.

## Expressions

The expression grammar is precedence-based and left-associative at each binary level unless noted otherwise.

```ebnf
Expr        = LogicOr ;
LogicOr     = LogicAnd { "||" LogicAnd } ;
LogicAnd    = Equality { "&&" Equality } ;
Equality    = Comparison { ( "==" | "!=" ) Comparison } ;
Comparison  = Range { ComparisonOp Range } ;
Range       = Term { RangeOp Term } ;
Term        = Factor { ( "+" | "-" | "∪" ) Factor } ;
Factor      = Unary { ( "*" | "/" | "%" | "∩" ) Unary } ;
Unary       = "&" Unary
            | "&mut" Unary
            | "->" Unary
            | "->mut" Unary
            | ( "!" | "-" | "*" | "~" ) Unary
            | Postfix ;
Postfix     = Primary { "." Primary | CallArgs | IndexArgs } ;

ComparisonOp = ">" | ">=" | "<" | "<=" | "∈" | "∉" | "⊆" | "⊂" | "⊄" | "⊇" | "⊃" | "⊅" ;
RangeOp      = ".." | "..=" ;
CallArgs     = "(" [ PositionalArgs | NamedArgs ] ")" ;
IndexArgs    = "[" Expr { "," Expr } "]" ;
```

Call arguments may be all positional or all named, but not mixed:

```ebnf
PositionalArgs = Expr { "," Expr } ;
NamedArgs      = Identifier "=" Expr { "," Identifier "=" Expr } ;
```

Constructor-like syntax is just call syntax. For example, `Point(x=1, y=2)` parses as an ordinary call with named arguments. Whether `Point` can construct a value is a semantic/yield-ness question, not a distinct grammar form.

Postfix struct literals such as `Point { x: 1, y: 2 }` are intentionally not part of the grammar. Braces remain reserved for sets, constructed sets, and statement blocks.

Index expressions require at least one index expression.

`&mut` and `->mut` are fused operators; whitespace is not allowed inside them.
Their mutability belongs to that pointer layer only:

```chirp
&x              // address-of
&mut x          // mutable address-of
->int           // read-capable pointer-set constraint
->mut int       // write-capable pointer-set constraint
->mut ->int     // mutable outer pointer layer
-> ->mut int    // mutable inner pointer layer
->mut ->mut int // both pointer layers marked mutable
```

The parser records these as nested unary AST nodes. The semantic and IR layers decide which combinations are valid and what each write capability permits.

## Primary Expressions

```ebnf
Primary =
    "struct" StructFields
  | "for" "(" BindingNoInitializer "∈" Expr ")" Expr
  | "while" "(" Expr ")" Expr
  | "if" "(" Expr ")" Expr "else" Expr
  | MatchExpr
  | ListExpr
  | Number | String | Character | SymbolicConstant
  | Identifier | Intrinsic
  | LambdaExpr | GroupExpr
  | BlockExpr
  | BraceExpr ;
```

Lambda and grouping both start with `(`. The parser treats a parenthesized form as a lambda only when a matching `=>` appears after the parameter list, optionally after a return bound.

```ebnf
LambdaExpr = "(" [ BindingNoInitializer { "," BindingNoInitializer } ] ")" [ ":" Expr ] "=>" Expr ;
GroupExpr  = "(" Expr ")" ;
```

Lists:

```ebnf
ListExpr = "[" [ Expr { "," Expr } [ "," ] ] "]" ;
```

Struct expressions define structured values/types for the POC. Fields are named bindings, and trailing commas are accepted. Function-sugar fields are accepted when they include a body initializer.

```ebnf
StructFields = "{" [ FieldBinding { "," FieldBinding } [ "," ] ] "}" ;
```

Pattern Matching:

```ebnf
MatchExpr = "match" Expr "{" [ MatchArm { "," MatchArm } [ "," ] ] "}" ;
MatchArm  = Expr "=>" Expr ;
```

## Block Expressions and Brace Expressions

Braces are strictly and unconditionally reserved for set construction:

- `{}` parses as an empty enumerated set.
- `{x}` parses as a singleton enumerated set.
- `{x, y}` and `{x, y,}` parse as enumerated sets.
- `{x | predicate}` and `{x : bound | predicate}` parse as constructed sets.

Because braces are dedicated to sets, statement blocks are explicitly prefixed with the `do` keyword. This makes parsing completely unambiguous and eliminates the need for complex, speculative lookaheads.

```ebnf
BlockExpr      = "do" "{" { Statement } [ Expr ] "}" ;
BraceExpr      = EnumeratedSet | ConstructedSet ;
EnumeratedSet  = "{" [ Expr { "," Expr } [ "," ] ] "}" ;
ConstructedSet = "{" Identifier [ ":" Expr ] "|" Expr "}" ;
```

A `BlockExpr` evaluates to its optional trailing `Expr`. If the trailing `Expr` is omitted, the block evaluates to `` `void ``. 

At the parser level, this trailing expression is implicitly desugared:
*   If a trailing `Expr` is present, it is wrapped in an implicit `BreakStmt(Expr)`.
*   If it is absent, an implicit `BreakStmt(Void)` is appended.

Explicit `break expr;` statements remain fully supported for early-exit control flow.

```chirp
let one = {1};                 // singleton set
let empty = {};                // empty set
let evens = {x : int | x % 2 == 0};
let value = do { let y = 1; y }; // y is the tail expression (implicitly breaks with y)
```

No-op function bodies should be explicit rather than overloading `{}` as an empty block:

```chirp
let noop_block() = do {}; // empty block implicitly yields `void
let noop_value() = `void;
```
