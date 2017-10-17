## sqlitexx

A header-only C++14 wrapper for SQLite3.

The minimum SQLite3 version supported is currently `3.14.1`.

### Supported Compilers

Any C++14 capable compiler should be able to compile this.

Of note:

- GCC 5.1+
- Clang 3.4+
- MSVC 2015 or higher

I have only tested this on GCC 7.0.

### Installing

The library is entirely header-only, but if you want to generate a single file for even easier inclusion just
run the `single.py` script. e.g.

```
$ python single.py
Creating single header for project sqlitexx.
Current revision: b62b491

...
Successfully output file to  ./sqlitexx.hpp
```

Afterwards you will get a single header file for convenience.

### License

MIT. See LICENSE.
