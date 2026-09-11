#include "main.h"
#include "rsp/rsp.cpp"

// Reactor entry point: runs the global constructors exactly once.
// Must be called by the host before any other export (see rspjs.js).
extern "C" void __wasm_call_ctors();
extern "C" void WASM_EXPORT(_initialize)()
{
  __wasm_call_ctors();
}

void WASM_EXPORT(rsp_init)()
{
  ares::N64::rsp.load();
  ares::N64::rsp.power(true); // resets state and builds the vrcp/vrsq tables
}

void WASM_EXPORT(rsp_set_halted)(u32 isHalted)
{
  ares::N64::rsp.status.halted = isHalted ? 1 : 0;
}

u32 WASM_EXPORT(rsp_get_halted)()
{
  return ares::N64::rsp.status.halted;
}

void WASM_EXPORT(rsp_step)(u32 steps)
{
  for(int i=0; i<steps; ++i) {
    ares::N64::rsp.exec();
  }
}

u32 WASM_EXPORT(rsp_ptr_dmem)()
{
  return (u32)ares::N64::rsp.dmem.data;
}

u32 WASM_EXPORT(rsp_ptr_imem)()
{
  return (u32)ares::N64::rsp.imem.data;
}

u32 WASM_EXPORT(rsp_ptr_gpr)(u32 reg)
{
  return (u32)ares::N64::rsp.ipu.r;
}

u32 WASM_EXPORT(rsp_ptr_vpr)(u32 reg)
{
  return (u32)ares::N64::rsp.vpu.r;
}

u32 WASM_EXPORT(rsp_get_cycles)()
{
  return ares::N64::rsp.clock;
}


u32 WASM_EXPORT(rsp_ptr_rdram)()
{
  return (u32)ares::N64::rdram.data;
}

u32 WASM_EXPORT(rsp_rdram_size)()
{
  return ares::N64::rdram.size;
}
