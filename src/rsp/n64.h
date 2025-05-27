#pragma once

struct Thread {
  auto reset() -> void {
    clock = 0;
  }

  auto step(u32 clocks) -> void {
    clock += clocks;
  }

  s64 clock;
};

enum : u32 { Read, Write };
enum : u32 { Byte = 1, Half = 2, Word = 4, Dual = 8, DCache = 16, ICache = 32 };


namespace Accuracy::RSP
{
  constexpr bool SISD = true;
  constexpr bool SIMD = true;  
}
