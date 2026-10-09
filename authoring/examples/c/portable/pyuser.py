import ctypes, os
lib = ctypes.CDLL(os.path.join(os.path.dirname(os.path.abspath(__file__)), "libcabi.so"))

class CPoint(ctypes.Structure):                     # must mirror the C layout EXACTLY (types, order, padding)
    _fields_ = [("x", ctypes.c_int32), ("y", ctypes.c_int32), ("weight", ctypes.c_double)]

lib.cabi_add.argtypes = [ctypes.c_int, ctypes.c_int]; lib.cabi_add.restype = ctypes.c_int
lib.cabi_dot.argtypes = [ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_double), ctypes.c_size_t]; lib.cabi_dot.restype = ctypes.c_double
lib.cabi_fill.argtypes = [ctypes.POINTER(CPoint), ctypes.c_size_t]; lib.cabi_fill.restype = None
lib.cabi_version.restype = ctypes.c_char_p

print("Python calls C:", lib.cabi_add(40, 2), lib.cabi_version().decode())
a = (ctypes.c_double * 3)(1, 2, 3); b = (ctypes.c_double * 3)(4, 5, 6)
print("dot =", lib.cabi_dot(a, b, 3))
pts = (CPoint * 4)(); lib.cabi_fill(pts, 4)
print("points:", [(p.x, p.y, p.weight) for p in pts], " sizeof(CPoint) =", ctypes.sizeof(CPoint))
