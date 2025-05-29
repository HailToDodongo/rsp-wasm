#include "main.h"

void WASM_IMPORT(print_char)(u8 c);
void WASM_IMPORT(print_u32)(u32 c);

namespace {
  void print(const char* str) {
    for(;*str != 0; ++str)print_char(*str);
  }
}

#include "rsp/rsp.cpp"

void WASM_EXPORT(rsp_init)()
{
  ares::N64::rsp.load();
  ares::N64::rsp.reset();
}

void WASM_EXPORT(rsp_set_halted)(u32 isHalted)
{
  ares::N64::rsp.status.halted = isHalted ? 1 : 0;
}

void WASM_EXPORT(rsp_step)()
{
  ares::N64::rsp.exec();
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