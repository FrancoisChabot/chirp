def make_adder(x):
    return lambda y: x + y


def work(n, acc):
    if n <= 0:
        return acc
    add = make_adder(n)
    return work(n - 1, add(acc))


print(work(400, 0))
