import math
# Fan-out and height of a B+ tree for a given page size, key size and pointer size.
# An internal node holds m keys and m+1 child pointers: m*key + (m+1)*ptr <= page - header.
HEADER = 32
def fanout(page, key, ptr):
    return (page - HEADER - ptr) // (key + ptr) + 1
print("B+ tree height (root to leaf, counting the leaf level) at 70% fill; 8-byte child pointers, 8-byte values, 32-byte page header")
print(f"{'page':>8} {'key':>5} {'fan-out':>8} | {'n=1e6':>6} {'n=1e9':>6} {'n=1e12':>7}   keys per leaf")
for page in (4096, 8192, 16384, 65536):
    for key in (8, 16, 64):
        f = fanout(page, key, 8)
        leaf_keys = int((page - HEADER) // (key + 8) * 0.7)
        def h(n):
            leaves = math.ceil(n / leaf_keys); height = 1
            while leaves > 1: leaves = math.ceil(leaves / (f * 0.7)); height += 1
            return height
        print(f"{page:>8} {key:>5} {f:>8} | {h(1e6):>6} {h(1e9):>6} {h(1e12):>7}   {leaf_keys}")
print("\nbinary tree for comparison: log2(1e9) =", round(math.log2(1e9), 1), "levels")
