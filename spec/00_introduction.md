# The Chirp Specification

Behold, Chirp:

```
b ∈ S ≜ τ(S).mrp(S, b.cv : b.lc ⊆ b.fc) : τ(S).mrr(S, b.lc) ⊆ {true, false, undecided}
```

## Preface

The objective of this set of documents is to provide a single authoritative source of truth about the Chirp language for the purpose of implementors. While it may be a useful resource for users of the language from time to time, the document is not meant to be a user guide.

Chirp's design is a bit of an ouroboros of internal circular dependencies, and some aspects that appear fundamental at first glance are actually scaffolded on top of emergent behaviors that are far from evident when looking at the raw building blocks of the language. This speaks to the language's elegance, but it also means that its formal description can appear confusing as all heck on first contact.

To alleviate this, this specification begins with a chronicle of the design process, charting the exact sequence of reasoning that led to the core ontology.

---

## Introduction: The Two Worlds of Chirp

Chirp is meant to be used in *most* (though not all) places where you would reach for C, C++, Rust, or Zig. However, its architecture is fundamentally different: **It does not have a compiler in the traditional sense.**

The `chirp` executable is first and foremost an **interpreter**. The boundless, mathematically pure **Interpreter Environment** is where your program starts. In this environment, there are no physical constraints—types are first-class values, sets are dynamic predicates, and meta-programming is just ordinary, run-of-the-mill code execution.

**Compilation is not a phase of the executable; it is a library function** (provided by the standard library) that selectively lowers parts of your dynamic script into high-performance, zero-abstraction systems code.

```text
       ┌───────────────────────────────┐
       │     Interpreter Environment   │  ◄── Boundless, dynamic, types-as-values
       └───────────────┬───────────────┘
                       │
                       │ std.compile()  ◄── "Calcification" Gate Valve
                       ▼
       ┌───────────────────────────────┐
       │      Runtime Environment      │  ◄── Physically bounded, zero-overhead C-like
       └───────────────────────────────┘
```

The bridge between these two worlds is a process called **Calcification**. When you invoke compilation on a function entrypoint, the compiler freezes a closed-world snapshot of the interpreter's state and analyzes the flow of constraints. 

To enforce physical reality, the compiler enforces the **Golden Rule of Code Generation**:

> [!IMPORTANT]
> **Any binding that participates in dynamic runtime behavior MUST have its constraint reduced to a set containing only values with representable types (scalars, structs, tagged unions, or memory locations).**

If a binding in the compiled path cannot be proven down to a concrete physical layout, compilation halts. This ensures that the generated machine code (often lowered to C or LLVM IR) is deterministic, aligned, and blistering-fast.

---

## Conventions

### Syntactic Hygiene: The Backtick

In Chirp code, (almost) all language intrinsics are prefixed with a backtick (e.g., `` `any `` or `` `type ``). This ensures that user-defined code can never accidentally conflict with system-provided symbols.

> [!NOTE]
> **Documentation Convention:** Because writing nested backticks (`` ``` `any` ``` ``) is visually noisy and cumbersome in Markdown prose, this specification adopts a standard convention: we omit the backtick prefix when discussing these intrinsics in standard text paragraphs (e.g., writing "the type of `any` is `Type`"). The backtick prefix is only strictly included inside full code blocks or when disambiguation is absolutely necessary.

### Mathematical Notation: The Type-Of Operator (`τ`)

Throughout this specification's formal equations, we use the Greek letter **`τ` (tau)** as a compact, single-character mathematical substitute for the language's `typeof(x)` operator. That is, $\tau(x) \triangleq \text{typeof}(x)$.

---

## The Heart

While the entire core mechanisms driving everything in Chirp technically fit on a single line, that's actually 5 important relations all mashed together:

```text
b.lc ⊆ b.fc
b.cv ∈ b.lc
b ∈ S ≜ τ(S).mrp(S, b.cv)
τ(S).mrp(S, b.cv) ∈ τ(S).mrr(S, b.lc)
τ(S).mrr(S, b.lc) ⊆ {true, false, undecided}
```

That probably seems cryptic to you right now, but don't worry. By the end of the next chapter, these equations will make sense. By the end of the chapter after that, they should feel inevitable.

---

## Specification Roadmap

This specification is organized to guide you step-by-step through the mechanics of the language:

- **[01_the_story.md](01_the_story.md):** A Socratic derivation of the core ontology.
- **[02_the_core.md](02_the_core.md):** Bindings, Sets, Values, and Types. That's all we need.
- **[03_the_machine.md](03_the_machine.md):** How code becomes emmitable to C. 
