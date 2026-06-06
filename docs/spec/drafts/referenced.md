# References (Draft)

References are a type of value that alias a *binding*. They are Chirp's equivalent to C pointers.

A reference *always* points to a binding.

General syntax:
```
let x : int = 3;
let x_ref : ->int = &x;

*x = 5;

`print(*x_ref); // prints 5
```

## operators

### The arrow operatorss `->` and `->mut`

If `S` is a set of values, `->` is the set of references to that value 

### The reference operators `&` and `&mut`

The `&` operator creates a *soft reference* to its argument, which must be a binding.

When a soft binding is assigned as the `cv` of a reference binding, it is stored as a *hard reference*. And the target binding's refcount is increased by one.

When a binding holding a hard reference is destoryed or re-assigned, it decrements the target binding's refcount.

If a binding is destoyed with a non-zero ref-count, that triggers an immediate evaluation error of the "Binding destroyed while a reference wtill points to it" error.

```
let x : int = 3
let mut ptr: ->int = &x;  // x's refcount is unaltered

do {
    let y : int = 5;
    ptr = &y;        
}; // Runtime error: y is destroyed while still being pointed to.
```

However, there are exceptions to this process:

- A reference assigned to a stack-based binding that sits higher on the stack from the source does not affect that binding's ref-count (I think? Feels about right...)

### The dereference operator `*`


### Delaoing with lifetimes



In Dynamic Chirp, this causes a runtime error

## Iterators

...