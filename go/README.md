## Generate go bindings
- Generate shared lib
  - `mkdir build && cd build`
  - `cmake -DCOMPILE_GO=on ../`
  - `make`
  - lib located at `../build/src/libbergamot-translator.dylib`
- Generate cpp wrappers and go module with bindings
  - `swig -go -cgo -c++ -intgosize 64 -use-shlib -soname libbergamot-translator.dylib go_bergamot.i`
  - (required?) Add compiler flags to `go_bergamot.go:L15/L16` (righ after `package go_bergamot`)
    ```
    // #cgo CFLAGS: -I../src/translator/ -I../3rd_party/marian-dev/src/
    // #cgo LDFLAGS: -L../build/src/translator/ -lbergamot-translator
    ```
  - final go module at `/go/go_bergamot.go`
- Call from test script in `/go-client`
  - `go run runme.go`