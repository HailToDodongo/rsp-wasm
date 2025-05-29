# RSP-JS

WASM port of the RSP code from ares.<br/>
This project lets you emulate RSP code accurately via a WASM module,<br>
allowing for execution inside of JS and other languages.

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

### Example

```js
import {createRSP} from './rspjs.js';

// Create RSP instance
const rsp = await createRSP();

// Set some instructions in IMEM
rsp.IMEM.setUint32(0, 0x3401_1234, true); // ori $1, $0, 0x1234
rsp.IMEM.setUint32(4, 0x3401_ABCD, true); // ori $1, $0, 0xABCD

// Run for a few steps
for(let steps=0; steps<4; ++steps) {
  // R/W access to all registers
  const pc = rsp.getPC().toString(16);
  const reg1 = rsp.getGPR(1).toString(16);
  console.log(`PC: ${pc} | REG: ${reg1}`);
  rsp.step();
}

// Output:
// PC: 0 | REG: 0
// PC: 4 | REG: 1234
// PC: 8 | REG: abcd
// PC: c | REG: abcd
```