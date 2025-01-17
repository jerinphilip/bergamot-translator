<img src="https://browser.mt/images/about.jpg">

# bergamot-translator

[![native](https://github.com/browsermt/bergamot-translator/actions/workflows/native.yml/badge.svg)]()
[![python + wasm](https://github.com/browsermt/bergamot-translator/actions/workflows/build.yml/badge.svg)]()
[![PyPI version](https://badge.fury.io/py/bergamot.svg)](https://badge.fury.io/py/bergamot)
[![twitter](https://img.shields.io/twitter/url.svg?label=Follow%20@BergamotProject&style=social&url=http://twitter.com/BergamotProject)](https://twitter.com/BergamotProject)

bergamot-translator enables client-side machine translation on the
consumer-grade machine. Developed as part of the
[Bergamot](https://browser.mt/) project, the library builds on top of:

1. [Marian](https://marian-nmt.github.io/): Neural Machine Translation (NMT)
   library. This repository uses the fork
   [browsermt/marian-dev](https://github.com/browsermt/marian-dev), which
   optimizes for faster inference on intel CPUs and adds WebAssembly support.
2. [student models](https://github.com/browsermt/students): Compressed neural
   models that enable translation on consumer-grade devices.

bergamot-translator wraps marian to add sentence splitting, on-the-fly
batching, HTML markup translation, and a more suitable API to develop
applications. Development continuously tests the functionality on Windows,
MacOS and Linux operating systems on `x86_64`, and WebAssembly cross-platform
target. `aarch64` native support is available for Android and Mac M1 (early
stages).

## Usage

### As a C++ library

bergamot-translator uses the CMake build system. Use the library target
`bergamot-translator` in projects that intend to build applications on top of
the library. Latest developer documentation is available at
[browser.mt/docs/main](https://browser.mt/docs/main).

### In other languages

We provide bindings to Python and JavaScript through WebAssembly.

#### Python

This repository provides a python module which also comes with  a command-line
interface to use available models. The module is available to install through PyPI for Linux and MacOS at the moment.


```bash
python3 -m pip install bergamot
```

Find an example for a quick-start on Colab below:

[![Open In Colab](https://colab.research.google.com/assets/colab-badge.svg)](https://colab.research.google.com/drive/1AHpgewVJBFaupwAbZq0e6TdX6REx0Ul0)

For more comprehensive documentation of using the python as a library see
[browser.mt/docs/main/python.html](https://browser.mt/docs/main/python.html).

#### JavaScript/WebAssembly

WebAssembly and JavaScript support is developed for an offline-translation
browser extension intended for use in Mozilla Firefox web-browser. emscripten
is used to compile C/C++ sources to WebAssembly. You may use the pre-built
`bergamot-translator-worker.js` and `bergamot-translator-worker.wasm` available
from [releases](https://github.com/browsermt/bergamot-translator/releases).

WebAssembly is available in Firefox and Google Chrome. It is also possible to
use the components through NodeJS. For an example of how to use this, please look at
this [Hello World](./wasm/node-test.js) example.  For a complete demo that
works locally in your modern browser see
[mozilla.github.io/translate](https://mozilla.github.io/translate/).

WebAssembly is slower due to lack of optimized matrix-multiply primitives.
Nightly builds of Mozilla Firefox have faster GEMM (Generalized Matrix
Multiplication) capabilities and are expected to be slightly faster. 
The browser environment can also use Native Messaging as a third option to translate web-pages locally, which is the fastest at the moment. 

The
following chart created from
[jelmervdl/firefox-translations/pull#19](https://github.com/jelmervdl/firefox-translations/pull/19)
shows how each method compares against one another in terms of words-per-second
(wps).

<img src="./doc/bergamot-comparisons.png" width=800>

## Applications

### translateLocally

For a cross platform batteries included GUI application that builds on top of
bergamot-translator, checkout
[translateLocally](https://github.com/XapaJIaMnu/translateLocally).
translateLocally provides model downloading from a repository and curates
models. 

### Browser Extension

Mozilla, as part of Bergamot Project builds and maintains
[firefox-translations](https://github.com/mozilla/firefox-translations/). The
official Firefox extension uses WebAssembly.

See
[jelmervdl/firefox-translations](https://github.com/jelmervdl/firefox-translations/)
for Chrome extension (Manifest V2), which in addition to WebAssembly, supports
faster local translation via [Native
Messaging](https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Native_messaging)
supported by
[translateLocally](https://github.com/XapaJIaMnu/translateLocally).


## Contributing

We appreciate all contributions. There are several ways to contribute to this
project.

1. **Code**: Improvements to the source are always welcome. If you are planning to
   contribute back bug-fixes to this repository, please do so without any
   further discussion.  If you plan to contribute new features, utility functions,
   or extensions to the core, please
   [discuss](https://github.com/browsermt/bergamot-translator/discussions) the
   feature with us first.
2. **Models**: Bergamot, being a wrapper on marian should comfortably work with
   models trained using marian. We prefer models that are trained following the
   recipe in
   [browsermt/students](https://github.com/browsermt/students/tree/master/train-student)
   so that they are smaller in size and enable fast inference on the
   consumer-grade machine.

## Acknowledgements

<table>
<tr>
<td> <img src="./doc/eu-logo.png" width=120> </td>
<td> This project has received funding from the European Union’s Horizon 2020
research and innovation programme under grant agreement No 825303.  </td>
</tr>
</table>

The builds generate library that can be integrated to any project. All the public header files are specified in `src` folder.
A short example of how to use the APIs is provided in `app/bergamot.cpp` file.


