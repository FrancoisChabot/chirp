# Chirp - User guide

If you haven't read the [motivation](../motivation.md) document yet, you should.

## Conventions

### Belonging

We use **belonging** for the `∈` relationship instead of the conventional **membership**. This is intentional.

Sets are fundamental to Chirp, not something built on top of it. In fact, most aspects of the language are described in terms of set theory. That becomes a problem as soon as we introduce the `.` operator, because `foo.bar` reads as "the bar member of foo" to most people with OOP experience. As much as we'd like to give sets first dibs on the word, there is too much inertia behind that usage.

So in Chirp land, values **belong** to sets. They are not "members" of sets.