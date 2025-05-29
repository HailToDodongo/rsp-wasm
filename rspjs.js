import {readFile} from 'fs/promises';

const REGS_SCALAR = [
  "$zero", "$at", "$v0", "$v1", "$a0", "$a1", "$a2", "$a3",
  "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7",
  "$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7",
  "$t8", "$t9",
  "$k0", "$k1", "$gp", "$sp", "$fp", "$ra",
];

// MIPS vector register in correct order ($v00 - $v31)
const REGS_VECTOR = [
  "$v00", "$v01", "$v02", "$v03", "$v04", "$v05", "$v06", "$v07",
  "$v08", "$v09", "$v10", "$v11", "$v12", "$v13", "$v14", "$v15",
  "$v16", "$v17", "$v18", "$v19", "$v20", "$v21", "$v22", "$v23",
  "$v24", "$v25", "$v26", "$v27", "$v28", "$v29", "$v30", "$v31",
];

const REG_MAP = {};
for(let i=0; i<32; ++i) {
  REG_MAP[REGS_SCALAR[i]] = i;
  REG_MAP[REGS_VECTOR[i]] = i;
}

class RSP {
  constructor(wasmModule) {
    this.fn = wasmModule.instance.exports;
    const wasmMemBuff = wasmModule.instance.exports.memory.buffer;

    this.fn.rsp_init();
    this.fn.rsp_set_halted(0);

    this.GPR = new DataView(wasmMemBuff, this.fn.rsp_ptr_gpr());
    this.VPR = new DataView(wasmMemBuff, this.fn.rsp_ptr_vpr());
    this.IMEM = new DataView(wasmMemBuff, this.fn.rsp_ptr_imem());
    this.DMEM = new DataView(wasmMemBuff, this.fn.rsp_ptr_dmem());
  }

  step(count = 1) { 
    this.fn.rsp_step(count); 
  }

  /**
   * Reads a scalar register
   * @param {number|string} reg
   * @returns {number}
   */
  getGPR(reg) {
    if(typeof(reg) === 'string')reg = REG_MAP[reg];
    return this.GPR.getUint32(reg * 4, true);
  }

  /**
   * Sets a scalar register
   * @param {number|string} reg
   * @param {number} value
   */
  setGPR(reg, value) {
    if(typeof(reg) === 'string')reg = REG_MAP[reg];
    this.GPR.setUint32(reg * 4, value >>> 0, true);
  }

  /**
   * Reads a complete vector register
   * @param {number|string} reg
   * @returns {[number, number, number, number, number, number, number, number]}
   */
  getVPR(reg) {
    if(typeof(reg) === 'string')reg = REG_MAP[reg];
    const res = [];
    for(let i=0; i<8; ++i) {
      res[7-i] = this.VPR.getUint16(reg * 16 + i*2, true);
    }
    return res;
  }

  /**
   * Sets a complete vector register
   * @param {number|string} reg
   * @param {[number, number, number, number, number, number, number, number]} values
   */
  setVPR(reg, values) {
    if(typeof(reg) === 'string')reg = REG_MAP[reg];
    for(let i=0; i<8; ++i) {
      this.VPR.setUint16(reg * 16 + i*2, values[7-i] >>> 0, true);
    }
  }

  /**
   * Current PC
   * @returns {number}
   */
  getPC() {
    return this.GPR.getUint32(32 * 4, true);
  }
}

let wasmBuffer;

/**
 * @returns {Promise<RSP>}
 */
export async function createRSP() {
  if(!wasmBuffer) {
    const filePath = new URL('rsp.wasm', import.meta.url);
    wasmBuffer = readFile(filePath);
  }
  const wasmModule = await WebAssembly.instantiate(await wasmBuffer, {});
  return new RSP(wasmModule);
}
