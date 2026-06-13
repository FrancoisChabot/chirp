# Chirp - VM

Chirp is interpreted during compilation, so it needs a robust and efficient VM to run in.

## Sandboxing

Compiling untrusted code needs to be safe. Details TBD once the model starts taking shape.

## Structure

- Code is stored on a linear tape that a head reads when executing
- Code is organized as a list of program units.
- A program unit can be changed/rewritten/etc without having to touch any other blocks of code
- A program unit can have devirtualized alternate implementations
- A program unit represents a whole lambda.

## Instructions

- Each instruction has an 8-bit identification prefix.
- The first 5 bits of an instruction represent the semantics (32 possible operations), the next 3 bits represent the domain (8 possible domains).

### Domains

In principle, every semantic instruction attempts to execute on any type. In practice, we can often narrow this down via static analysis and short circuit the "what are the types of the operand?" part of the evaluation. Each program unit *may* define up to 6 specialized domains (by way of function pointer tables). 

- `000` is reserved for the generic fallback.
- `111` is reserved for local subdomain injections (e.g., lc-analysis)

So, for example, the function `fib(n: int) => if (n < 2) n else fib(n-1) + fib(n-2)` would assign the `int` domain to `001`, and use `BinaryMath(0b001, ADD)`, which would map to an add function that assumes both operands are integers.


### Operands

Operand structure is defined on a instruction-per-instruction basis. However, the following pattern is used as much as possible:

Operand specicifications come in 2 or 3 bits variants. This is driven by the if instruction, which uses a 2-bits operand for the condition, and 3-bits for both branches, for a total of 8. 

```
0x00 -> The operand is an inline expression, when comes time to evaluate the operand, it is read as an instruction.
0x01 -> Stack-local: The operand is read as an index in the current stack frame. This includes arguments. 
0x02 -> Identifier: The operand is an identifier lookup. The compiler will generate a perfect hash table per Program Unit, ensuring O(1) collision-free runtime resolution of global/module names without chaining overhead.
0x03 -> Immediate value

// In the 3 bits variant
0x04 -> immediate uint8
0x05 -> immediate uint16
0x06 -> immediate uint32
0x07 -> immediate uint64
```

#### Destination-Driven Evaluation (0x00 Inline Expressions)

To avoid managing an unbounded temporary data stack when evaluating nested inline expressions (operand `0x00`), the VM employs **Destination-Driven Evaluation** (or Targeted Evaluation). 

This is particularly relevant for `Call` and `MakeAnonStruct` instructions. When the VM encounters a call such as `foo(x=bar(), y=baz())`:
1. It first evaluates the target callable (`foo`).
2. It allocates the backing struct/tuple that will hold the arguments.
3. Instead of asking `bar()` and `baz()` to return a value onto a generic stack, the VM passes those inline expressions a direct pointer/reference to their respective destination fields within the newly allocated argument struct.

The subroutines execute and write their results *directly* into their final resting place in memory. This zero-copy approach ensures the VM never needs an unbounded operand stack. The number of transient, floating values at any given point is rigidly capped (usually 1 or 2 pointers). The C++ call stack remains lightweight, simply passing destination pointers down the evaluation tree.

### Proposed Semantic Instructions (5-bit Opcode Space)

5 bits is *tight*. And we're already bundling binops as instruction groups. But the more per-lambda domains, the better. We're definitely going to have to benchmark that though (32-64 root instructions vs 4-8 domains).

With 5 bits, we have room for exactly 32 base semantics. The VM uses a unified instruction set for both expressions and statements, but it explicitly distinguishes between the two evaluation contexts (expression vs block statement). To reflect this boundary, statement-only operations are pushed to the very end of the opcode range.

#### 1. Control Flow & Scope
- `Block` (0x01): Evaluates a sequence of instructions. Pushes a new lexical scope.
- `Break` (0x02): Early exit from a block, yielding a value.
- `If` (0x03): Ternary branching (condition, true_branch, false_branch).
- `Loop` (0x04): Infinite loop (desugars `while` and `for` in combination with `If` and `Break`).
- `Match` (0x05): Evaluates a subject against a sequence of patterns. 

#### 2. Memory & Reference
- `Ref` (0x06): Creates a reference (`&` / `&mut`).
- `Deref` (0x07): Dereferences a value (`*`).
- `GetField` (0x08): Accesses a struct/enum member (`.`).
- `SetField` (0x09): Mutates a struct/enum member. (May potentially be consolidated into Assign)
- `Index` (0x0A): Accesses a collection element (`[]`).
- `SetIndex` (0x0B): Mutates a collection element. (May potentially be consolidated into Assign)

#### 3. Functions & Calls
- `Call` (0x0C): Invokes a callable (function, struct constructor, etc.) with arguments.
- `MakeLambda` (0x0D): Constructs a closure from a program unit, capturing necessary environment.
- `Return` (0x0E): Exits the current program unit (lambda) with a value.

#### 4. Consolidated Math & Logic
*Note: These instructions require an extra postfix byte/bits to specify the exact operation. Logical `&&` and `||` are usually desugared to `If`.*
- `BinaryMath` (0x0F): E.g., `+`, `-`, `*`, `/`, `%`.
- `UnaryMath` (0x10): E.g., `-` (negate), `!` (not), `~`.
- `Compare` (0x11): E.g., `==`, `!=`, `<`, `<=`, `>`, `>=`.

#### 5. Sets & Ranges
- `BelongsTo` (0x12) (`∈`)
- `Union` (0x13) (`∪`)
- `Intersect` (0x14) (`∩`)
- `MakeRange` (0x15) (`..`, `..=`)
- `MakeConstructedSet` (0x16) (`{x | ...}`)

#### 6. Data Structures
- `MakeStructDef` (0x17): Defines a new struct type.
- `MakeEnumDef` (0x18): Defines a new enum type.
- `MakeList` (0x19): Instantiates a list literal.
- `MakeEnumeratedSet` (0x1A): Instantiates an enumerated set literal (`{1, 2}`).
- `MakeAnonStruct` (0x1B): Instantiates an anonymous struct literal.

#### 7. Statements (Non-Expressions)
These instructions do not yield a value and are only valid in statement execution contexts (like the body of a `Block`).
- `Let` (0x1E / 30): Declares a new binding in the current scope with an initial value and optional constraint (`fc`).
- `Assign` (0x1F / 31): Mutates an existing binding or memory location.

*Total: 29 instructions used. 3 remaining slots (0x1C, 0x1D, 0x00) in the 5-bit space.*