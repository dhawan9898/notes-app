"""Python client through ctypes. Run with: python3 use.py [broken]"""
import ctypes, sys

lib = ctypes.CDLL("./libacc.so")

class Stats(ctypes.Structure):                                   # must mirror acc_stats field by field, in order
    _fields_ = [("min", ctypes.c_double), ("max", ctypes.c_double), ("mean", ctypes.c_double), ("n", ctypes.c_size_t)]

if len(sys.argv) > 1 and sys.argv[1] == "broken":
    pass                                                          # forget the prototypes: ctypes assumes every function returns a C int
else:
    lib.acc_new.restype = ctypes.c_void_p                         # the handle is a 64-bit pointer
    lib.acc_free.argtypes = [ctypes.c_void_p]
    lib.acc_add.argtypes = [ctypes.c_void_p, ctypes.c_double]     # without this, a Python float would be passed as an int
    lib.acc_stats_get.argtypes = [ctypes.c_void_p, ctypes.POINTER(Stats)]
    CALLBACK = ctypes.CFUNCTYPE(None, ctypes.c_double, ctypes.c_void_p)
    lib.acc_foreach.argtypes = [ctypes.c_void_p, CALLBACK, ctypes.c_void_p]
    lib.acc_describe.restype = ctypes.c_void_p                    # not c_char_p: that would copy the text and lose the pointer we must free
    lib.acc_describe.argtypes = [ctypes.c_void_p]
    lib.acc_string_free.argtypes = [ctypes.c_void_p]
    lib.acc_strerror.restype = ctypes.c_char_p
    lib.acc_strerror.argtypes = [ctypes.c_int]

h = lib.acc_new()
if len(sys.argv) > 1 and sys.argv[1] == "broken":
    # 1. the default restype is a 32-bit int: a 64-bit pointer is cut to its low half. Compare with the full value from a correct prototype.
    full = ctypes.CFUNCTYPE(ctypes.c_void_p)(("acc_new", lib))()
    print(f"acc_new without restype gave {h & 0xffffffffffffffff:#x}; a correct prototype gives {full:#x} for another object")
    print("  (both are below 4 GiB here because this process's heap happens to live there: the bug is latent, and it appears with PIE/ASLR/mmap'd allocations)")
    # 2. a Python int is passed in an integer register, but acc_add takes a double, which the C ABI expects in a floating-point register
    for x in (3, 1, 4): lib.acc_add(h, x)
    s = Stats(); lib.acc_stats_get(h, ctypes.byref(s))
    print(f"added the integers 3, 1, 4 without argtypes -> the library stored n={s.n} values with min={s.min}, max={s.max} (expected 1 and 4)")
    sys.exit(0)

for x in (3.0, 1.5, 4.5, 9.0): assert lib.acc_add(h, x) == 0
s = Stats(); rc = lib.acc_stats_get(h, ctypes.byref(s))
print(f"Python: rc={rc} n={s.n} min={s.min} max={s.max} mean={s.mean}")

seen = []
cb = CALLBACK(lambda v, user: seen.append(v))                      # keep a reference to cb alive for the duration of the call
lib.acc_foreach(h, cb, None)
print("Python: values delivered to a Python callback:", seen)

p = lib.acc_describe(h); print("Python:", ctypes.string_at(p).decode()); lib.acc_string_free(p)
empty = lib.acc_new(); rc = lib.acc_stats_get(empty, ctypes.byref(s))
print("Python: empty accumulator ->", rc, lib.acc_strerror(rc).decode())
lib.acc_free(empty); lib.acc_free(h)
