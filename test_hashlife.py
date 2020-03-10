from hashlife import join, successor
from itertools import product

def test_bootstrap():
    # try to generate all 4x4 successors
    def product_tree(pieces):
        return [join(a, b, c, d) for a, b, c, d in product(pieces, repeat=4)]

    boot_2x2 = product_tree([on, off])
    boot_4x4 = product_tree(boot_2x2)
    centres = [successor(p, 1) for p in boot_4x4]


