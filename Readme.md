# RSP-WASM

WASM port of the RSP code from ares.<br/>
This project lets you emulate RSP code accurately via a WASM module,<br>
allowing for execution inside of JS and other languages.

Ares repo: https://github.com/ares-emulator/ares

## Build

Make sure you have a recent version of `clang` installed.

Run: 
```bash
./build.sh
```
To build the WASM module.

## Usage (JS)

For usage within JS, a wrapper is provided via `rspjs.js`.<br/>
This can be imported as ES6 module to construct an instance as well as some API functions to interact with the emulated RSP.

For an example, checkout `examples/test.mjs`.