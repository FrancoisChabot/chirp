# TODO: Pointer Model, Address Spaces, and Slices

This note captures the current design conversation around pointers. The short
version is that the current spec treats pointers too much like machine
addresses, but Chirp needs a unified pointer model that behaves congruently in
both interpreter and machine contexts.

The model is not settled. This file is intentionally exploratory.

## Why this needs its own treatment

Pointers are doing too much conceptual work to remain a short subsection inside
the machine chapter.

They touch:

- ontology: what is a location, place, or address?
- traits/capabilities: pointer-ness, maybe contiguous-address-ness
- interpreter semantics: virtual address spaces and references to abstract
  values
- machine lowering: native addresses, stack slots, heap objects, spans, slices
- mutability: replacing the value stored in an addressable place
- flow refinement: aliasing and invalidation
- syntax: `&`, `&mut`, `*`, `->S`, indexing, and slicing

This is probably enough material for a story-style derivation chapter similar
to `01_the_story.md`, focused specifically on locations and pointers.

## Current concern

The current machine chapter says, roughly:

- values are immutable;
- imperative mutation comes from physical memory locations;
- `->S` is the set of memory locations containing values in `S`;
- dereferencing accesses the value currently residing there;
- aliases affect flow-sensitive refinement.

That is directionally useful for machine lowering, but too concrete for the
final ontology.

In particular, it risks baking in the idea that a pointer is fundamentally a
machine address. That is probably wrong for Chirp.

Chirp needs pointers to work during interpretation too. For example, the
interpreter may need something pointer-like that refers to a `Type`, a `Trait`,
a `Matcher`, a module, a binding, or another non-representable value. Those
things do not live at native machine addresses in the relevant semantic sense,
but the language should still be able to talk about addressable places
uniformly.

## Working mental model

A pointer should not be defined as a machine address.

A better working definition:

```text
A pointer is a value that denotes a place in some address space.
```

A machine pointer is then only one lowering of that idea.

This suggests a more general ontology:

- **Value**: immutable data.
- **Binding**: a named slot with `fc`, `lc`, and `cv`.
- **Place / Location**: something that can currently contain a value.
- **Address Space**: a domain in which places can be resolved.
- **Pointer**: a value whose type provides pointer-ness: resolution, read,
  write, and constraint information for a place in an address space.

Names are provisional. `Place`, `Location`, and `AddressSpace` need proper
design work.

## Pointer-ness as an ontological capability

Pointer-ness may belong next to set-ness and yield-ness.

The pattern is similar:

- set-ness powers `v in S` / `v ∈ S`;
- yield-ness powers forced computation or call-like behavior;
- pointer-ness powers dereference, store, and address resolution.

In other words, a value becomes usable by pointer operators because its type
supplies the relevant capability.

A rough pointer-ness capability might include:

```text
address_space(p)
read_domain(p)
write_domain(p)
read(p)
write(p, v)
```

The split between read and write domains is important.

A pointer may be readable as `S` without being writable as all of `S`. A
read-only pointer should not affect flow-sensitive refinement the same way as a
mutable pointer. Aliasing hazards are about write capability, not mere pointer
existence.

Potential rule shape:

```text
*p : read_domain(p)
*p = v is legal iff v in write_domain(p)
```

There may also need to be operations for identity/equality, lifetime, region
membership, and capability restriction, but those should not be assumed yet.

## Address spaces

The key abstraction is probably an address space.

Machine memory is one address space:

```text
MachineAddressSpace:
  pointer representation = native-ish machine address
  readable/writable values = representable values
  lowering = native load/store
```

The interpreter can have another:

```text
InterpreterAddressSpace:
  pointer representation = VM handle, object id, environment path, region id,
                           or similar
  readable/writable values = arbitrary Chirp values, including Type, Trait,
                             Matcher, module, etc.
  lowering = requires interpreter/runtime support; not necessarily statically
             representable
```

This lets syntax remain unified:

```chirp
&x
&mut x
*p
```

The meaning is resolved through the pointer value's address space and
pointer-ness, not by assuming native memory.

## Contiguity must be separate from pointer-ness

We need clean slicing syntax that maps efficiently to hardware, but not all
pointers are meaningfully contiguous.

A pointer to a `Type` in the interpreter can be addressable without supporting
pointer arithmetic. A pointer into a machine array or buffer should support
offsets and distance.

Therefore pointer-ness should not imply arithmetic.

A separate capability is likely needed. Provisional name:
**contiguous-address-ness**.

Possible operations:

```text
offset(p, n)
distance(p, q)
same_region(p, q)
element_stride(p)
```

Other names may be better. The important distinction is:

- pointer-ness: this value denotes an addressable place;
- contiguous-address-ness: this pointer participates in an ordered contiguous
  region where offsets and ranges are meaningful.

## Slices and ranges

Slices should probably not be modeled primarily as sets of pointers.

A set loses information that slicing needs:

- order;
- length;
- stride;
- adjacency;
- base;
- address space / region identity.

A better model is that a slice/span/range is an ordered contiguous range of
locations.

Rough shape:

```text
Span<S>:
  base: pointer-like value
  len: integer
  stride: maybe implicit 1
  address_space: same as base
  element domain: S
```

As a derived behavior, a span can expose set-like membership:

```text
p in span iff
  same_region(span.base, p) &&
  distance(span.base, p) in 0..span.len
```

But that should not be its primary identity.

Machine lowering should be straightforward:

```text
machine span = { base pointer, length }
span[i]      = *(base + i * stride)
```

Interpreter lowering could be:

```text
interpreter span = { region id, start offset, length }
span[i]          = read(region, start + i)
```

This supports clean slicing syntax while preserving efficient hardware mapping.

## Aliasing and refinement

Aliasing should be framed in terms of write capability over places or ranges.

Important consequences:

- A read-only pointer should not poison refinement the same way a writable
  pointer does.
- A mutable pointer to a place invalidates or bounds refinements for that place.
- A mutable span invalidates or bounds refinements for every place that may
  overlap its range.
- Multiple active writable aliases combine by possible writes.

The current refinement-locking rule is directionally right, but it should be
recast once pointer-ness, write domains, and address ranges are defined.

Possible future statement:

```text
If a write-capable pointer or span may write values in W to a place P, then any
flow-sensitive refinement of P must be widened after a possible write to account
for W intersected with P's fundamental constraint.
```

For spans, the rule applies to all places that may overlap the span.

## Relationship to the current chapter order

The intended spec order is:

1. core ontology;
2. machine grounding;
3. computation as manipulation of the ontology.

That ordering is coherent. The machine chapter can ground the ontology before
computation, but it should be careful not to overdefine runtime behavior before
the computation model exists.

The current machine chapter is doing two jobs:

1. representability and grounding;
2. runtime behavior, pointers, mutation, aliasing, lowering.

The first belongs before computation. The second probably belongs after
computation or in a dedicated pointer/location chapter.

## Material spec changes to consider

### 1. Narrow the pre-computation machine chapter

The pre-computation machine chapter should focus on representability:

- what it means for values/types/constraints to imply finite layouts;
- how a binding's `fc` can bound storage shape;
- how multi-type constraints can lower to tagged layouts;
- why some values cannot be directly lowered.

Avoid settling pointer semantics there.

### 2. Reword physical memory locations

Current wording like "A Memory Location is a runtime physical container" is too
machine-specific.

Possible replacement direction:

```text
A Location is an addressable container that can currently hold a Value. Physical
memory locations are the machine-runtime realization of this more general
concept.
```

This leaves room for interpreter-owned locations.

### 3. Mark pointers as under active design

Until the pointer ontology is settled, the machine chapter should include a
clear caveat.

Possible text:

```text
Locations and pointer-like values are under active design. For the purpose of
this chapter, it is sufficient to know that Chirp needs a unified notion of
addressable places that can model both interpreter-owned values and machine
memory. Later chapters will define pointer-ness, contiguous ranges, slicing,
and aliasing in detail.
```

### 4. Avoid saying `->S` means only machine pointers

Current rough definition:

```text
If S is a set of representable values, then ->S is the set of all Memory
Locations that contain a value belonging to S.
```

This should probably become more general.

Possible direction:

```text
->S is the set of pointer-like values whose dereference/read result is
constrained by S.
```

But this is incomplete because it ignores write domains. We may need syntax or
typing rules that can distinguish read-only and writable pointer capabilities.

Open question:

```text
Does ->S describe read capability, write capability, or both?
```

Potential future split:

```text
->S          read-capable pointer to S
->mut S      write-capable pointer accepting S
->R/W        some explicit read/write domain pair
```

Names and syntax are not decided.

### 5. Separate pointer and contiguous range APIs

The spec should explicitly avoid implying that every pointer supports offset or
distance.

Introduce a separate model for contiguous pointers/spans/slices.

Needed concepts:

- same address space;
- same region;
- offset;
- distance;
- stride;
- length;
- overlap.

### 6. Recast aliasing rules in terms of write capability

The existing aliasing discussion should eventually be rewritten around:

- places;
- pointer write domains;
- spans/ranges of places;
- possible overlap;
- refinement widening after possible writes.

This will make the rules apply uniformly to interpreter pointers, machine
pointers, and slices.

### 7. Add a story-style pointer chapter

Candidate filename:

```text
00b_the_story_of_locations.md
03b_the_story_of_pointers.md
```

Possible outline:

1. Values are immutable, but programs need places whose contents can change.
2. A binding is one kind of place, but not the only kind.
3. A pointer denotes a place, not necessarily a machine address.
4. Address spaces make that statement precise.
5. Interpreter pointers require virtual address spaces.
6. Machine pointers are the representable lowering of some pointer-like values.
7. Not all pointers are contiguous.
8. Contiguity is a separate capability needed for arrays and slices.
9. Slices are ordered ranges of locations, not merely sets of pointers.
10. Aliasing is about write capability over places and ranges.
11. Only after that: syntax and lowering.

## Open questions

- What is the canonical term: place, location, slot, cell, address, or something
  else?
- Is a binding itself a location, or does a binding own/reference a location?
- Is pointer-ness implemented as a trait, a core capability, or both?
- Does `->S` describe read capability, write capability, or invariant
  read/write capability?
- How should read-only vs writable pointers be expressed?
- How should pointer equality work across address spaces?
- What is the lifetime model for interpreter address spaces?
- Can interpreter address spaces be user-defined?
- Are address spaces first-class values?
- Are regions first-class values?
- How do spans expose set-ness, if at all?
- Should slicing syntax construct a span value, a view, or something else?
- How does a pointer to a non-representable value behave during static
  compilation?
- What is the exact failure mode when pointer-like values cannot be lowered?

## Current safe stance

Do not overspecify pointers in the main spec yet.

For now, the implementor-facing spec should say only:

- Chirp will need a unified model of addressable places.
- Machine memory is one realization of that model.
- Interpreter-owned values need another realization.
- Pointer-ness and contiguous ranges are likely separate capabilities.
- Slices should preserve order, length, stride, and address-space identity.
- Aliasing rules should be based on write capability and possible overlap.

Everything else should remain explicitly provisional until the pointer story is
worked through.
