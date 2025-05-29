import {createRSP} from '../dist/index.js';
import {assemble} from 'armips';

const files = await assemble(`
.rsp 

.create "dmem.bin", 0x0000
  TEST_CONST: .db 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88
  .align 2
.close

.create "imem.bin", 0x1000
    ori $t0, $0, 0x1234
    ori $t1, $0, 0xABCD
    ori $t2, $0, TEST_CONST
    ldv $v01[0], 0($t2)
    vadd $v02, $v01, $v02
    lb $1, (TEST_CONST+3)($0)
    nop
.close
`);

const imem = files["imem.bin"];
const dmem = files["dmem.bin"];

const rsp = await createRSP();

// Load into emulator (swap BE to LE)
const imemView = new DataView(imem.buffer);
for(let i=0; i<imem.byteLength; i += 4) {
  rsp.IMEM.setUint32(i, imemView.getUint32(i, false), true);
}

const dmemView = new DataView(dmem.buffer);
for(let i=0; i<dmem.byteLength; i += 4) {
  rsp.DMEM.setUint32(i, dmemView.getUint32(i, false), true);
}

rsp.setVPR("$v02", [1,2,3,4,5,6,7,8]);

for(let steps=0; steps<6; ++steps) {
  const pc = rsp.getPC().toString(16);
  const t0 = rsp.getGPR("$t0").toString(16);
  const v02 = rsp.getVPR("$v02");

  console.log(`PC: ${pc} | t0: ${t0} | $v02: ${v02}`);
  rsp.step();
}
