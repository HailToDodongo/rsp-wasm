#include "../../nall/bit.hpp"

using namespace nall;

struct Readable {
  explicit operator bool() const {
    return size > 0;
  }

  auto reset() -> void {
    data = nullptr;
    size = 0;
    maskByte = 0;
    maskHalf = 0;
    maskWord = 0;
    maskDual = 0;
  }

  auto fill(u32 value = 0) -> void {
    for(u32 address = 0; address < size; address += 4) {
      *(u32*)&data[address & maskWord] = value;
    }
  }


  template<u32 Size>
  auto read(u32 address) -> u64 {
    if constexpr(Size == Byte) return *(u8* )&data[address & maskByte ^ 3];
    if constexpr(Size == Half) return *(u16*)&data[address & maskHalf ^ 2];
    if constexpr(Size == Word) return *(u32*)&data[address & maskWord ^ 0];
    if constexpr(Size == Dual) {
      u64 upper = read<Word>(address + 0);
      u64 lower = read<Word>(address + 4);
      return upper << 32 | lower << 0;
    }
    unreachable;
  }

  template<u32 Size>
  auto write(u32 address, u64 value) -> void {
  }

  template<u32 Size>
  auto readUnaligned(u32 address) -> u64 {
    static_assert(Size == Half || Size == Word || Size == Dual);
    if constexpr(Size == Half) {
      u16 upper = read<Byte>(address + 0);
      u16 lower = read<Byte>(address + 1);
      return upper << 8 | lower << 0;
    }
    if constexpr(Size == Word) {
      u32 upper = readUnaligned<Half>(address + 0);
      u32 lower = readUnaligned<Half>(address + 2);
      return upper << 16 | lower << 0;
    }
    if constexpr(Size == Dual) {
      u64 upper = readUnaligned<Word>(address + 0);
      u64 lower = readUnaligned<Word>(address + 4);
      return upper << 32 | lower << 0;
    }
    unreachable;
  }

  template<u32 Size>
  auto writeUnaligned(u32 address, u64 value) -> void {
    static_assert(Size == Half || Size == Word || Size == Dual);
  }



//private:
  u8* data = nullptr;
  u32 size = 0;
  u32 maskByte = 0;
  u32 maskHalf = 0;
  u32 maskWord = 0;
  u32 maskDual = 0;
};
