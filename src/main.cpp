#include "main.h"
#include "rsp/rsp.cpp"

void WASM_EXPORT(rsp_init)()
{
  ares::N64::rsp.reset();
}