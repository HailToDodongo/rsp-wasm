import {createRSP} from './rspjs.js';

const rsp = await createRSP();
rsp.IMEM.setUint32(0, 0x3401_1234, true); // ori $1, $0, 0x1234
rsp.IMEM.setUint32(4, 0x3401_ABCD, true); // ori $1, $0, 0xABCD

for(let steps=0; steps<4; ++steps) {
  const pc = rsp.getPC().toString(16);
  const reg1 = rsp.getGPR(1).toString(16);
  console.log(`PC: ${pc} | REG: ${reg1}`);
  rsp.step();
}
