import fs from 'node:fs/promises';

class RSP {
  constructor(wasmModule) {    
    /*const { 
      rsp_init, rsp_step,
      rsp_ptr_dmem, rsp_ptr_imem,
      rsp_ptr_gpr, rsp_ptr_vpr,
      rsp_set_halted
    } = wasmModule.instance.exports;*/
    this.fn = wasmModule.instance.exports;
    const wasmMemBuff = wasmModule.instance.exports.memory.buffer;
    
    this.fn.rsp_init();
    this.fn.rsp_set_halted(0);
    
    this.GPR = new DataView(wasmMemBuff, this.fn.rsp_ptr_gpr());
    this.VPR = new DataView(wasmMemBuff, this.fn.rsp_ptr_vpr());
    this.IMEM = new DataView(wasmMemBuff, this.fn.rsp_ptr_imem());
    this.DMEM = new DataView(wasmMemBuff, this.fn.rsp_ptr_dmem());
  }

  step() { this.fn.rsp_step(); }

  getGPR(reg) {
    return this.GPR.getUint32(reg * 4, true);
  }

  setGPR(reg, value) {
    this.GPR.setUint32(reg * 4, value >>> 0, true);
  }

  getPC() {
    return this.GPR.getUint32(32 * 4, true);
  }
}

/**
 * @returns {Promise<RSP>}
 */
export async function createRSP() {
  const wasmBuffer = await fs.readFile('rsp.wasm');
  const wasmModule = await WebAssembly.instantiate(wasmBuffer, {
    env: {
      print_char: (c) => process.stdout.write(String.fromCharCode(c)),
      print_u32: (n) => process.stdout.write(n.toString(16)),
    }
  });
  return new RSP(wasmModule);
}

