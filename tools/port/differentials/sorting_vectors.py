"""Shared original inventory/container stat choices and category selection; no asset I/O."""

SUPPORTED = (1, 2, 4, 5)
STATS = {
    0: tuple(range(24)),
    1: tuple(range(24)),
    2: (0, 8, 9, 10, 24, 25, 26),
    3: (1, 4, 5, 6, 7, 8, 9, 10, 40, 41, 42, *range(27, 40)),
    4: (0, 1, 4, 5, 8, 9, 10, 40, 41, 42, *range(27, 40)),
    5: (8, 9, 10, *range(27, 40)),
    6: (0, 56, -1, -0x80000000, 0x7FFFFFFF),
}


def sort_category(kind, number):
    """Select the same category, including the projection's unsupported integer controls."""
    if kind == 3:
        return (1, 2, 4, 5, 0, 3, 6, 7, -1, 256, 257, -0x80000000, 0x7FFFFFFF)[
            number % 13
        ]
    return (0, 3, 6, 0, SUPPORTED[number % 4])[kind]


def sort_stat(category, number):
    """Cycle the exact original comparator family without changing RNG consumption."""
    choices = STATS.get(category, (0,))
    return choices[number % len(choices)]


def populate_sort_order(order, first, count, last, number):
    """Seed identical empty/single/full/duplicate token controls in each captured order."""
    order[first : first + count] = [0] * count
    if number % 16 == 1 and count:
        order[first + count - 1] = last
    elif number % 64 == 4:
        order[first : first + count] = [(2, last, 1, 2)[n % 4] for n in range(count)]
    elif number % 16 != 0:
        for position, token in enumerate((2, 0, 1, 2, last)):
            if position < count:
                order[first + position] = token


def projection_index(capacities, category, number):
    """Select the original first/second/last record without consuming random values."""
    maximum = capacities[category] if 0 <= category < 7 else 2
    return (0, 1, maximum - 1)[number // 13 % 3]
