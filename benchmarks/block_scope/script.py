def work(n, acc):
    if n <= 0:
        return acc
    a = acc + n
    b = a + 3
    c = b - 1
    d = c + 5
    return work(n - 1, d)


print(work(400, 0))
