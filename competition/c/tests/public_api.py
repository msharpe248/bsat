"""Typed ctypes bindings for test drivers; use only the installed public ABI."""
import ctypes as C


class Stats(C.Structure):
    _fields_ = [('version', C.c_uint32), ('result', C.c_int32),
                *[(n, C.c_uint64) for n in ('conflicts', 'decisions', 'propagations',
                                          'reused_preparations', 'owned_capacity_bytes')],
                ('cpu_seconds', C.c_double)]


Cancel = C.CFUNCTYPE(C.c_int, C.c_void_p)


def literals(values):
    return (C.c_int * len(values))(*values)


def library(path):
    lib = C.CDLL(str(path))
    ptr, integer, u64 = C.c_void_p, C.c_int, C.c_uint64
    specs = {
        'create': (ptr, [C.c_uint32, C.c_uint32]), 'destroy': (None, [ptr]),
        'add_clause': (integer, [ptr, C.POINTER(integer), C.c_size_t]),
        'solve': (integer, [ptr, C.POINTER(integer), C.c_size_t]),
        'value': (integer, [ptr, integer]), 'failed': (integer, [ptr, integer]), 'error': (integer, [ptr]),
        'checkpoint': (integer, [ptr]),
        'export_query': (integer, [ptr, C.c_char_p, C.c_char_p]),
        'get_stats': (integer, [ptr, C.POINTER(Stats), C.c_size_t]),
        'set_query_limits': (integer, [ptr, C.c_double, C.c_uint32, C.c_uint32]),
        'set_service_limits': (integer, [ptr, C.c_double, u64]),
        'service_limit_hit': (integer, [ptr]),
        'get_journal_bytes': (integer, [ptr, C.POINTER(u64)]),
        'set_journal_limit': (integer, [ptr, u64]),
        'set_terminate': (None, [ptr, ptr, Cancel]),
    }
    for name, (result, arguments) in specs.items():
        f = getattr(lib, 'bsat_' + name)
        f.restype, f.argtypes = result, arguments
    return lib
