# Support for Emscripten Cross Build

This module provides support for emscripten cross build

## Status

    $ make

You can run the WASM mandelbrot Qt example, if you copy its HTML
and the qtloader.js from the Qt's example folder after build with:

    $ emrun --serve_after_close workdir/LinkTarget/Executable/mandelbrot.html

REMINDER: Always start new tabs in the browser, reload might fail / cache!


## Setup for the LO WASM build (with Qt)

We're using Qt 5.15 with the officially supported emscripten v1.39.8.
But there are several potential problems with threads and exceptions, so this will likely
change later to a newer emscripten.

Qt WASM is not yet used with LO, just if you're wondering!

- See below under Docker build for another build option

### Setup emscripten

<https://emscripten.org/docs/getting_started/index.html>

    git clone https://github.com/emscripten-core/emsdk.git
    ./emsdk install 1.39.8
    ./emsdk activate --embedded 1.39.8

Example `bashrc` scriptlet:

    EMSDK_ENV=$HOME/Development/libreoffice/git_emsdk/emsdk_env.sh
    [ -f "$EMSDK_ENV" ] && \. "$EMSDK_ENV" 1>/dev/null 2>&1

### Setup Qt

<https://doc.qt.io/qt-5/wasm.html>

I originally build the Qt 5.15 branch, but probably better to build a tag like v5.15.2.

So:

    git clone https://github.com/qt/qt5.git
    cd qt5
    git checkout v5.15.2
    ./init-repository
    ./configure -xplatform wasm-emscripten -feature-thread -compile-examples -prefix $PWD/qtbase
    make -j<CORES> module-qtbase module-qtdeclarative

Building with examples will break with some of them, but at that point Qt already works.

At some point Qt configure failed for me with:

"Checking for target architecture... Project ERROR: target architecture detection binary not found."

What seems to have fixed this was to run "emsdk activate 1.39.8" again.

Current Qt fails to start the demo webserver: <https://bugreports.qt.io/browse/QTCREATORBUG-24072>

Use `emrun --serve_after_close` to run Qt WASM demos

Enabling multi-thread support in Firefox is a bit of work with older versions:

- <https://bugzilla.mozilla.org/show_bug.cgi?id=1477743#c7>
- <https://wiki.qt.io/Qt_for_WebAssembly#Multithreading_Support>
- <https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/SharedArrayBuffer>


### Setup LO

`autogen.sh` is patched to use emconfigure. That basically sets various environment vars,
especially `EMMAKEN_JUST_CONFIGURE`, which will create the correct output file names, checked by
`configure` (`a.out`).

There's a distro config for WASM (work in progress), that gets your
defaults right (and currently disables a ton of 3rd party stuff which
is not essential).

Recommended configure setup is thusly:

* grab defaults
    `--with-distro=LibreOfficeWASM32`

* local config
    `QT5DIR=/dir/of/git_qt5/qtbase`

* if you want to use ccache on both sides of the build
    `--with-build-platform-configure-options=--enable-ccache`
    `--enable-ccache`

### Using Docker to cross-build with emscripten

If you prefer a controlled environment (sadly emsdk install/activate
is _not_ stable over time, as e.g. nodejs versions evolve), that is
easy to replicate across different machines - consider the docker
images we're providing.

Config/setup file see
<https://git.libreoffice.org/lode/+/ccb36979563635b51215477455953252c99ec013>

Run

    docker-compose build

in the lode/docker dir to get the container prepared. Run

    PARALLELISM=4 BUILD_OPTIONS= BUILD_TARGET=build docker-compose run --rm -e PARALLELISM -e BUILD_TARGET -e BUILD_OPTIONS builder

to perform an actual `srcdir != builddir` build; the container mounts
checked-out git repo and output dir via `docker-compose.yml` (so make
sure the path names there match your setup):

The lode setup expects, inside the lode/docker subdir, the following directories:

- core (`git checkout`)
- workdir (the output dir - gets written into)
- cache (`ccache tree`)
- tarballs (external project tarballs gets written and cached there)


## Ideas for an UNO bridge implementation

My post to Discord #emscripten:

"I'm looking for a way to do an abstract call
from one WASM C++ object to another WASM C++ object, so like FFI / WebIDL,
just within WASM. All my code is C++ and normally I have bridge code, with
assembler to implement the function call /RTTI and exception semantics of the
specified platform. Code is at
<https://cgit.freedesktop.org/libreoffice/core/tree/bridges/source/cpp_uno>.
I've read a bit about `call_indirect` and stuff, but I don't have yet a good
idea, how I could implement this (and  there is an initial feature/wasm branch
for the interested). I probably need some fixed lookup table, like on iOS,
because AFAIK you can't dynamically generate code in WASM. So any pointers or
ideas for an implementation? I can disassemble some minimalistic WASM example
and read clang code for `WASM_EmscriptenInvoke`, but if there were some
standalone code or documentation I'm missing, that would be nice to know."

We basically would go the same way then the other backends. Write the bridge in
C++, which is probably largely boilerplate code, but the function call in WAT
(<https://github.com/WebAssembly/wabt>) based on the LLVM WASM calling
conventions in `WASM_EmscriptenInvoke`. I didn't get a reply to that question for
hours. Maybe I'll open an Emscripten issue, if we really have to implement
this.

WASM dynamic dispatch:

- <https://fitzgeraldnick.com/2018/04/26/how-does-dynamic-dispatch-work-in-wasm.html>


## Workaround for eventual clang WASM compiler bug

````
sc/source/core/data/attarray.cxx:378:44: error: call to member function 'erase' is ambiguous
                        aNewCondFormatData.erase(nIndex);
                        ~~~~~~~~~~~~~~~~~~~^~~~~
include/o3tl/sorted_vector.hxx:86:15: note: candidate function
    size_type erase( const Value& x )
              ^
include/o3tl/sorted_vector.hxx:97:10: note: candidate function
    void erase( size_t index )
````

This is currently patched by using `x.erase` (`x.begin() + nIndex`).

There shouldn't be an ambiguity, because of "[WebAssembly] Change size_t to `unsigned long`."
<https://reviews.llvm.org/rGdf07a35912d78781ed6a62a7c032bfef5085a4f5#change-IrS9f6jH6PFq>,
from "Jul 23 2018" which pre-dates the emscripten tag 1.39.8 from 02/14/2020 by ~1.5y.


## Tools for problem diagnosis

* `nm -s` should list the symbols in the archive, based on the index generated by ranlib.
  If you get linking errors that archive has no index.


## Emscripten filesystem access with threads

This is closed, but not really fixed IMHO:

- <https://github.com/emscripten-core/emscripten/issues/3922>


## Dynamic libraries `/` modules in emscripten

There is a good summary in:

- <https://bugreports.qt.io/browse/QTBUG-63925>

Summary: you can't use modules and threads.

This is mentioned at the end of:

- <https://github.com/emscripten-core/emscripten/wiki/Linking>

The usage of `MAIN_MODULE` and `SIDE_MODULE` has other problems, a major one IMHO is symbol resolution at runtime only.
So this works really more like plugins in the sense of symbol resolution without dependencies `/` rpath.

There is some clang-level dynamic-linking in progress (WASM dlload). The following link is already a bit old,
but I found it a god summary of problems to expect:

- <https://iandouglasscott.com/2019/07/18/experimenting-with-webassembly-dynamic-linking-with-clang/>


## Mixed information, links, problems, TODO

More info on Qt WASM emscripten pthreads:

- <https://wiki.qt.io/Qt_for_WebAssembly#Multithreading_Support>

WASM needs `-pthread` at compile, not just link time for atomics support. Alternatively you can provide
`-s USE_PTHREADS=1`, but both don't seem to work reliable, so best provide both.
<https://github.com/emscripten-core/emscripten/issues/10370>

The output file must have the prefix .o, otherwise the WASM files will get a
`node.js` shebang (!) and ranlib won't be able to index the library (link errors).

Qt with threads has further memory limit. From Qt configure:
````
Project MESSAGE: Setting PTHREAD_POOL_SIZE to 4
Project MESSAGE: Setting TOTAL_MEMORY to 1GB
````

You can actually allocate 4GB:

- <https://bugzilla.mozilla.org/show_bug.cgi?id=1392234>

LO uses a nested event loop to run dialogs in general, but that won't work, because you can't drive
the browser event loop. like VCL does with the system event loop in the various VCL backends.
Changing this will need some major work (basically dropping Application::Execute).

But with the know problems with exceptions and threads, this might change:

- <https://github.com/emscripten-core/emscripten/pull/11518>
- <https://github.com/emscripten-core/emscripten/issues/11503>
- <https://github.com/emscripten-core/emscripten/issues/11233>
- <https://github.com/emscripten-core/emscripten/issues/12035>

We're also using emconfigure at the moment. Originally I patched emscripten, because it
wouldn't create the correct a.out file for C++ configure tests. Later I found that
the `emconfigure` sets `EMMAKEN_JUST_CONFIGURE` to work around the problem.

ICU bug:

- <https://github.com/emscripten-core/emscripten/issues/10129>

Alternative, probably:

- <https://developer.mozilla.org/de/docs/Web/JavaScript/Reference/Global_Objects/Intl>

There is a wasm64, but that still uses 32bit pointers!

Old outdated docs:

- <https://wiki.documentfoundation.org/Development/Emscripten>

Reverted patch:

- <https://cgit.freedesktop.org/libreoffice/core/commit/?id=0e21f6619c72f1e17a7b0a52b6317810973d8a3e>

Generally <https://emscripten.org/docs/porting>:

- <https://emscripten.org/docs/porting/guidelines/api_limitations.html#api-limitations>
- <https://emscripten.org/docs/porting/files/file_systems_overview.html#file-system-overview>
- <https://emscripten.org/docs/porting/pthreads.html>
- <https://emscripten.org/docs/porting/emscripten-runtime-environment.html>

This will be interesting:

- <https://emscripten.org/docs/getting_started/FAQ.html#how-do-i-run-an-event-loop>

This didn't help much yet:

- <https://github.com/emscripten-ports>

Emscripten supports standalone WASI binaries:

- <https://github.com/emscripten-core/emscripten/wiki/WebAssembly-Standalone>
- <https://www.qt.io/qt-examples-for-webassembly>
- <http://qtandeverything.blogspot.com/2017/06/qt-for-web-assembly.html>
- <http://qtandeverything.blogspot.com/2020/>
- <https://emscripten.org/docs/api_reference/Filesystem-API.html>
- <https://discuss.python.org/t/add-a-webassembly-wasm-runtime/3957/12>
- <http://git.savannah.gnu.org/cgit/config.git>
- <https://webassembly.org/specs/>
- <https://developer.chrome.com/docs/native-client/>
- <https://emscripten.org/docs/getting_started/downloads.html>
- <https://github.com/openpgpjs/openpgpjs/blob/master/README.md#getting-started>
- <https://developer.mozilla.org/en-US/docs/WebAssembly/Using_the_JavaScript_API>
- <https://github.com/bytecodealliance/wasmtime/blob/main/docs/WASI-intro.md>
- <https://www.ip6.li/de/security/x.509_kochbuch/openssl-fuer-webassembly-compilieren>
- <https://emscripten.org/docs/introducing_emscripten/about_emscripten.html#about-emscripten-porting-code>
- <https://emscripten.org/docs/compiling/Building-Projects.html>

