# Installing the C solver

Installation support is separate from production acceptance; see the current
[capabilities and release gates](PRODUCTION_READINESS.md).

Build and stage a release with `make CC=clang` followed by
`make install PREFIX=/opt/bsat DESTDIR=/path/to/staging`. Omit `DESTDIR` to
install directly into a writable prefix. Installation provides `bin/bsat`,
`include/bsat.h`, the ABI-v1 shared library and `lib/pkgconfig/bsat.pc`.
`VERSION` identifies the package; the current package is a development release.
The default optimization flags target the build machine. For binaries distributed
to other machines, build with an explicit portable target, for example
`make CC=clang OPTFLAGS='-O3 -flto'`, and use the same flags for installation.

Set `PKG_CONFIG_PATH` to the prefix's `lib/pkgconfig` directory. Compile clients
with `pkg-config --cflags --libs bsat`, and arrange the platform's runtime
library search path (for example, an rpath to the installed `lib` directory).
The library soname is `libbsat.so.1` on Linux and `libbsat.1.dylib` on macOS.
The public header works with both C and C++.

`make package-test` installs into a disposable staging directory, checks the
pkg-config version, compiles and executes a C consumer against the frozen
ABI-v1 header and a C++ consumer against the installed header. Run this target
with `MODE=debug` to check the sanitizer build too. The frozen header is a
compatibility fixture: do not update it when adding future API declarations.

Linux CI also runs independent-instance embedding and history tests with
`MODE=tsan`. This checks exercised concurrent paths; it does not authorize
concurrent calls on the same solver instance. See `API_CONTRACT.md` for the
threading and lifetime contract.
