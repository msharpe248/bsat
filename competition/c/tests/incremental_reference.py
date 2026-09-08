"""Minimal retained IPASIR reference binding; no BSAT internals or shared code."""
import ctypes as C
import time

class Reference:
    def __init__(self, path):
        self.lib=C.CDLL(str(path));lib=self.lib
        signatures={'init':([],C.c_void_p),'release':([C.c_void_p],None),
                    'add':([C.c_void_p,C.c_int],None),'assume':([C.c_void_p,C.c_int],None),
                    'solve':([C.c_void_p],C.c_int),'val':([C.c_void_p,C.c_int],C.c_int),
                    'failed':([C.c_void_p,C.c_int],C.c_int),'signature':([],C.c_char_p)}
        for name,(args,result) in signatures.items():
            f=getattr(lib,'ipasir_'+name);f.argtypes=args;f.restype=result
        self.s=lib.ipasir_init();assert self.s
        self.signature=lib.ipasir_signature().decode()
        self.stop=False;self.calls=0;self.deadline=0
        self.Callback=C.CFUNCTYPE(C.c_int,C.c_void_p)
        def terminate(_):
            self.calls+=1
            return self.stop or time.monotonic()>=self.deadline
        self.callback=self.Callback(terminate)
        lib.ipasir_set_terminate.argtypes=[C.c_void_p,C.c_void_p,self.Callback]
        lib.ipasir_set_terminate.restype=None
        lib.ipasir_set_terminate(self.s,None,self.callback)
    def add(self,clause):
        for lit in clause:self.lib.ipasir_add(self.s,lit)
        self.lib.ipasir_add(self.s,0)
    def solve(self,assumptions):
        self.deadline=time.monotonic()+10
        for lit in assumptions:self.lib.ipasir_assume(self.s,lit)
        return self.lib.ipasir_solve(self.s)
    def model(self,n):
        return 'v '+' '.join(str(self.lib.ipasir_val(self.s,v) or v) for v in range(1,n+1))+' 0\n'
    def close(self):
        if self.s:self.lib.ipasir_release(self.s);self.s=None
