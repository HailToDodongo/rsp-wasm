//Reality Signal Processor
#include "n64.h"

#include "nall/bit-range.hpp"
#include "nall/intrinsics.hpp"
#include "nall/endian.hpp"
#include "nall/range.hpp"
#include "nall/natural.hpp"
#include "nall/real.hpp"
#include "nall/integer.hpp"
#include "nall/types.hpp"
#include "memory/memory.hpp"

struct RSP : Thread, Memory::RCP<RSP> {
  
  struct Writable : public Memory::Writable {
    RSP& self;

    Writable(RSP& self) : self(self) {}

    template<u32 Size>
    auto read(u32 address) -> u64 {
      return Memory::Writable::read<Size>(address);
    }

    template<u32 Size>
    auto readUnaligned(u32 address) -> u64 {
      return Memory::Writable::readUnaligned<Size>(address);
    }

    template<u32 Size>
    auto write(u32 address, u64 value) -> void {
      Memory::Writable::write<Size>(address, value);
    }
    
    template<u32 Size>
    auto writeUnaligned(u32 address, u64 value) -> void {
      Memory::Writable::writeUnaligned<Size>(address, value);
    }

  } dmem{*this};
  Memory::Writable imem;


  //rsp.cpp
  auto load() -> void;
  auto unload() -> void;

  auto exec() -> void;

  auto instruction() -> void;
  auto instructionPrologue(u32 instruction) -> void;
  template<bool Recompiled> auto instructionEpilogue(u32 clocks) -> s32;

  auto power(bool reset) -> void;

  struct OpInfo {
    enum : u32 {
      Load      = 1 << 0,
      Store     = 1 << 1,
      Branch    = 1 << 2,
      Vector    = 1 << 3,
      VNopGroup = 1 << 4,  //dual issue conflicts with VNOP
      Bypass    = 1 << 5,
    };

    u32 flags;
    u32 vfake;  //only affects dual issue logic
    struct {
      u32 use, def;
    } r, v, vc;

    auto load() const -> bool { return flags & Load; }
    auto store() const -> bool { return flags & Store; }
    auto branch() const -> bool { return flags & Branch; }
    auto vector() const -> bool { return flags & Vector; }
    auto bypass() const -> bool { return flags & Bypass; }
  };

  static auto canDualIssue(const OpInfo& op0, const OpInfo& op1) -> bool {
    return op0.vector() != op1.vector()             //must be one SU and one VU
      && !(op0.v.def & (op1.v.use | op1.v.def))     //second op cannot read/write vector registers written by the first
      && !(op0.vc.def & (op1.vc.use | op1.vc.def))  //the same logic applies to vector control registers
      //certain instructions conflict due to "fake" uses from misinterpreted fields
      //such false conflicts only occur with VNOP if the preceding instruction is MTC2 or LTV
      && !(((op0.flags | ~op1.flags) & OpInfo::VNopGroup) && (op0.v.def & op1.vfake));
  }

  struct Pipeline {
    u32 address;
    u32 instruction;
    u32 clocks;
    u1 singleIssue;

    struct Stage {
      u1 load;
      u32 rWrite;
      u32 vWrite;
    } previous[3];

    struct : Stage {
      u1 store;
      u1 branch;
      u32 rRead;
      u32 vRead;
    } current;


    auto begin() -> void {
      clocks = 0;
    }

    auto end() -> void {
      readGPR(current.rRead);
      readVR(current.vRead);
      if(current.store) store();
      singleIssue = current.branch;

      previous[2] = previous[1];
      previous[1] = previous[0];
      previous[0] = current;
      current = {};
      clocks += 3;
    }

    auto stall() -> void {
      previous[2] = previous[1];
      previous[1] = previous[0];
      previous[0] = {};
      clocks += 3;
    }

    auto issue(const OpInfo& op) -> void {
      current.rRead |= op.r.use;
      if(!op.bypass()) current.rWrite |= op.r.def & ~1;  //zero register can't be written
      current.vRead |= op.v.use;
      current.vWrite |= op.v.def;
      current.load |= op.load();
      current.store |= op.store();
      current.branch |= op.branch();
    }

  private:
    auto readGPR(u32 mask) -> Pipeline& {
      if(mask & previous[0].rWrite) {
        stall(), stall();
      } else if(mask & previous[1].rWrite) {
        stall();
      }
      return *this;
    }

    auto readVR(u32 mask) -> Pipeline& {
      if(mask & previous[0].vWrite) {
        stall(), stall(), stall();
      } else if(mask & previous[1].vWrite) {
        stall(), stall();
      } else if(mask & previous[2].vWrite) {
        stall();
      }
      return *this;
    }

    auto store() -> void {
      while(previous[1].load) {
        stall();
      }
    }
  } pipeline;

  //dma.cpp
  auto dmaQueue(u32 clocks, Thread& thread) -> void;
  auto dmaStep(u32 clocks) -> void;
  auto dmaTransferStart(Thread& thread) -> void;
  auto dmaTransferStep() -> void;

  //io.cpp
  auto readWord(u32 address, Thread& thread) -> u32;
  auto writeWord(u32 address, u32 data, Thread& thread) -> void;
  auto ioRead(u32 address, Thread& thread) -> u32;
  auto ioWrite(u32 address, u32 data, Thread& thread) -> void;


  struct DMA {
    struct Regs {    
      n1  pbusRegion;
      n12 pbusAddress;
      n24 dramAddress;
      n12 length;
      n12 skip;
      n8  count;
    } pending, current;

    struct Status {
      n1 read;
      n1 write;

      auto any() -> n1 { return read | write; }
    } busy, full;

    s64 clock;
  } dma;

  struct Status : Memory::RCP<Status> {
    RSP& self;
    Status(RSP& self) : self(self) {}

    //io.cpp
    auto readWord(u32 address, Thread& thread) -> u32;
    auto writeWord(u32 address, u32 data, Thread& thread) -> void;

    n1 semaphore;
    n1 halted = 1;
    n1 broken;
    n1 full;
    n1 singleStep;
    n1 interruptOnBreak;
    n1 signal[8];
  } status{*this};

  //ipu.cpp
  union r32 {
    struct {  int32_t s32; };
    struct { uint32_t u32; };
  };
  using cr32 = const r32;

  struct IPU {
    enum Register : u32 {
      R0, AT, V0, V1, A0, A1, A2, A3,
      T0, T1, T2, T3, T4, T5, T6, T7,
      S0, S1, S2, S3, S4, S5, S6, S7,
      T8, T9, K0, K1, GP, SP, S8, RA,
    };

    r32 r[32];
    u16 pc; // previously u12; now u16 for performance.
  } ipu;

  struct Branch {
    enum : u32 { Step, Take, DelaySlot };

    auto inDelaySlot() const -> bool { return state == DelaySlot; }
    auto reset() -> void { state = Step; }
    auto take(u12 address) -> void { state = Take; pc = address; }
    auto delaySlot() -> void { state = DelaySlot; }

    u12 pc = 0;
    u32 state = Step;
  } branch;

  //cpu-instructions.cpp
  auto ADDIU(r32& rt, cr32& rs, s16 imm) -> void;
  auto ADDU(r32& rd, cr32& rs, cr32& rt) -> void;
  auto AND(r32& rd, cr32& rs, cr32& rt) -> void;
  auto ANDI(r32& rt, cr32& rs, u16 imm) -> void;
  auto BEQ(cr32& rs, cr32& rt, s16 imm) -> void;
  auto BGEZ(cr32& rs, s16 imm) -> void;
  auto BGEZAL(cr32& rs, s16 imm) -> void;
  auto BGTZ(cr32& rs, s16 imm) -> void;
  auto BLEZ(cr32& rs, s16 imm) -> void;
  auto BLTZ(cr32& rs, s16 imm) -> void;
  auto BLTZAL(cr32& rs, s16 imm) -> void;
  auto BNE(cr32& rs, cr32& rt, s16 imm) -> void;
  auto BREAK() -> void;
  auto J(u32 imm) -> void;
  auto JAL(u32 imm) -> void;
  auto JALR(r32& rd, cr32& rs) -> void;
  auto JR(cr32& rs) -> void;
  auto LB(r32& rt, cr32& rs, s16 imm) -> void;
  auto LBU(r32& rt, cr32& rs, s16 imm) -> void;
  auto LH(r32& rt, cr32& rs, s16 imm) -> void;
  auto LHU(r32& rt, cr32& rs, s16 imm) -> void;
  auto LUI(r32& rt, u16 imm) -> void;
  auto LW(r32& rt, cr32& rs, s16 imm) -> void;
  auto LWU(r32& rt, cr32& rs, s16 imm) -> void;
  auto NOR(r32& rd, cr32& rs, cr32& rt) -> void;
  auto OR(r32& rd, cr32& rs, cr32& rt) -> void;
  auto ORI(r32& rt, cr32& rs, u16 imm) -> void;
  auto SB(cr32& rt, cr32& rs, s16 imm) -> void;
  auto SH(cr32& rt, cr32& rs, s16 imm) -> void;
  auto SLL(r32& rd, cr32& rt, u8 sa) -> void;
  auto SLLV(r32& rd, cr32& rt, cr32& rs) -> void;
  auto SLT(r32& rd, cr32& rs, cr32& rt) -> void;
  auto SLTI(r32& rt, cr32& rs, s16 imm) -> void;
  auto SLTIU(r32& rt, cr32& rs, s16 imm) -> void;
  auto SLTU(r32& rd, cr32& rs, cr32& rt) -> void;
  auto SRA(r32& rd, cr32& rt, u8 sa) -> void;
  auto SRAV(r32& rd, cr32& rt, cr32& rs) -> void;
  auto SRL(r32& rd, cr32& rt, u8 sa) -> void;
  auto SRLV(r32& rd, cr32& rt, cr32& rs) -> void;
  auto SUBU(r32& rd, cr32& rs, cr32& rt) -> void;
  auto SW(cr32& rt, cr32& rs, s16 imm) -> void;
  auto XOR(r32& rd, cr32& rs, cr32& rt) -> void;
  auto XORI(r32& rt, cr32& rs, u16 imm) -> void;
  auto SPECIAL_INVALID(r32& rd, cr32& rt, cr32& rs) -> void;

  //scc.cpp: System Control Coprocessor
  auto MFC0(r32& rt, u8 rd) -> void;
  auto MTC0(cr32& rt, u8 rd) -> void;

  //vpu.cpp: Vector Processing Unit
  union r128 {
    struct { u64 order_msb2(hi, lo); } u128;

    auto byte(u32 index) -> uint8_t& { return ((uint8_t*)&u128)[15 - index]; }
    auto byte(u32 index) const -> uint8_t { return ((uint8_t*)&u128)[15 - index]; }

    auto element(u32 index) -> uint16_t& { return ((uint16_t*)&u128)[7 - index]; }
    auto element(u32 index) const -> uint16_t { return ((uint16_t*)&u128)[7 - index]; }

    auto u8(u32 index) -> uint8_t& { return ((uint8_t*)&u128)[15 - index]; }
    auto u8(u32 index) const -> uint8_t { return ((uint8_t*)&u128)[15 - index]; }

    auto s16(u32 index) -> int16_t& { return ((int16_t*)&u128)[7 - index]; }
    auto s16(u32 index) const -> int16_t { return ((int16_t*)&u128)[7 - index]; }

    auto u16(u32 index) -> uint16_t& { return ((uint16_t*)&u128)[7 - index]; }
    auto u16(u32 index) const -> uint16_t { return ((uint16_t*)&u128)[7 - index]; }

    //VCx registers
    auto get(u32 index) const -> bool { return u16(index) != 0; }
    auto set(u32 index, bool value) -> bool { return u16(index) = 0 - value, value; }

    //vu-registers.cpp
    auto operator()(u32 index) const -> r128;

    //serialization.cpp
    // auto serialize(serializer&) -> void;
  };
  using cr128 = const r128;

  struct VU {
    r128 r[32];
    r128 acch, accm, accl;
    r128 vcoh, vcol;  //16-bit little endian
    r128 vcch, vccl;  //16-bit little endian
    r128 vce;         // 8-bit little endian
     s16 divin;
     s16 divout;
    bool divdp;
  } vpu;

  static constexpr r128 zero{0ull, 0ull};
  static constexpr r128 invert{~0ull, ~0ull};

  auto accumulatorGet(u32 index) const -> u64;
  auto accumulatorSet(u32 index, u64 value) -> void;
  auto accumulatorSaturate(u32 index, bool slice, u16 negative, u16 positive) const -> u16;

  auto CFC2(r32& rt, u8 rd) -> void;
  auto CTC2(cr32& rt, u8 rd) -> void;
  template<u8 e> auto LBV(r128& vt, cr32& rs, s8 imm) -> void;
  template<u8 e> auto LDV(r128& vt, cr32& rs, s8 imm) -> void;
  template<u8 e> auto LFV(r128& vt, cr32& rs, s8 imm) -> void;
  template<u8 e> auto LHV(r128& vt, cr32& rs, s8 imm) -> void;
  template<u8 e> auto LLV(r128& vt, cr32& rs, s8 imm) -> void;
  template<u8 e> auto LPV(r128& vt, cr32& rs, s8 imm) -> void;
  template<u8 e> auto LQV(r128& vt, cr32& rs, s8 imm) -> void;
  template<u8 e> auto LRV(r128& vt, cr32& rs, s8 imm) -> void;
  template<u8 e> auto LSV(r128& vt, cr32& rs, s8 imm) -> void;
  template<u8 e> auto LTV(u8 vt, cr32& rs, s8 imm) -> void;
  template<u8 e> auto LUV(r128& vt, cr32& rs, s8 imm) -> void;
  template<u8 e> auto LWV(r128& vt, cr32& rs, s8 imm) -> void;
  template<u8 e> auto MFC2(r32& rt, cr128& vs) -> void;
  template<u8 e> auto MTC2(cr32& rt, r128& vs) -> void;
  template<u8 e> auto SBV(cr128& vt, cr32& rs, s8 imm) -> void;
  template<u8 e> auto SDV(cr128& vt, cr32& rs, s8 imm) -> void;
  template<u8 e> auto SFV(cr128& vt, cr32& rs, s8 imm) -> void;
  template<u8 e> auto SHV(cr128& vt, cr32& rs, s8 imm) -> void;
  template<u8 e> auto SLV(cr128& vt, cr32& rs, s8 imm) -> void;
  template<u8 e> auto SPV(cr128& vt, cr32& rs, s8 imm) -> void;
  template<u8 e> auto SQV(cr128& vt, cr32& rs, s8 imm) -> void;
  template<u8 e> auto SRV(cr128& vt, cr32& rs, s8 imm) -> void;
  template<u8 e> auto SSV(cr128& vt, cr32& rs, s8 imm) -> void;
  template<u8 e> auto STV(u8 vt, cr32& rs, s8 imm) -> void;
  template<u8 e> auto SUV(cr128& vt, cr32& rs, s8 imm) -> void;
  template<u8 e> auto SWV(cr128& vt, cr32& rs, s8 imm) -> void;
  template<u8 e> auto VABS(r128& vd, cr128& vs, cr128& vt) -> void;
  template<u8 e> auto VADD(r128& vd, cr128& vs, cr128& vt) -> void;
  template<u8 e> auto VADDC(r128& vd, cr128& vs, cr128& vt) -> void;
  template<u8 e> auto VAND(r128& vd, cr128& vs, cr128& vt) -> void;
  template<u8 e> auto VCH(r128& vd, cr128& vs, cr128& vt) -> void;
  template<u8 e> auto VCL(r128& vd, cr128& vs, cr128& vt) -> void;
  template<u8 e> auto VCR(r128& vd, cr128& vs, cr128& vt) -> void;
  template<u8 e> auto VEQ(r128& vd, cr128& vs, cr128& vt) -> void;
  template<u8 e> auto VGE(r128& vd, cr128& vs, cr128& vt) -> void;
  template<u8 e> auto VLT(r128& vd, cr128& vs, cr128& vt) -> void;
  template<bool U, u8 e>
  auto VMACF(r128& vd, cr128& vs, cr128& vt) -> void;
  template<u8 e> auto VMACF(r128& vd, cr128& vs, cr128& vt) -> void { VMACF<0, e>(vd, vs, vt); }
  template<u8 e> auto VMACU(r128& vd, cr128& vs, cr128& vt) -> void { VMACF<1, e>(vd, vs, vt); }
  auto VMACQ(r128& vd) -> void;
  template<u8 e> auto VMADH(r128& vd, cr128& vs, cr128& vt) -> void;
  template<u8 e> auto VMADL(r128& vd, cr128& vs, cr128& vt) -> void;
  template<u8 e> auto VMADM(r128& vd, cr128& vs, cr128& vt) -> void;
  template<u8 e> auto VMADN(r128& vd, cr128& vs, cr128& vt) -> void;
  template<u8 e> auto VMOV(r128& vd, u8 de, cr128& vt) -> void;
  template<u8 e> auto VMRG(r128& vd, cr128& vs, cr128& vt) -> void;
  template<u8 e> auto VMUDH(r128& vd, cr128& vs, cr128& vt) -> void;
  template<u8 e> auto VMUDL(r128& vd, cr128& vs, cr128& vt) -> void;
  template<u8 e> auto VMUDM(r128& vd, cr128& vs, cr128& vt) -> void;
  template<u8 e> auto VMUDN(r128& vd, cr128& vs, cr128& vt) -> void;
  template<bool U, u8 e>
  auto VMULF(r128& rd, cr128& vs, cr128& vt) -> void;
  template<u8 e> auto VMULF(r128& rd, cr128& vs, cr128& vt) -> void { VMULF<0, e>(rd, vs, vt); }
  template<u8 e> auto VMULU(r128& rd, cr128& vs, cr128& vt) -> void { VMULF<1, e>(rd, vs, vt); }
  template<u8 e> auto VMULQ(r128& rd, cr128& vs, cr128& vt) -> void;
  template<u8 e> auto VNAND(r128& rd, cr128& vs, cr128& vt) -> void;
  template<u8 e> auto VNE(r128& vd, cr128& vs, cr128& vt) -> void;
  auto VNOP() -> void;
  template<u8 e> auto VNOR(r128& vd, cr128& vs, cr128& vt) -> void;
  template<u8 e> auto VNXOR(r128& vd, cr128& vs, cr128& vt) -> void;
  template<u8 e> auto VOR(r128& vd, cr128& vs, cr128& vt) -> void;
  template<bool L, u8 e>
  auto VRCP(r128& vd, u8 de, cr128& vt) -> void;
  template<u8 e> auto VRCP(r128& vd, u8 de, cr128& vt) -> void { VRCP<0, e>(vd, de, vt); }
  template<u8 e> auto VRCPL(r128& vd, u8 de, cr128& vt) -> void { VRCP<1, e>(vd, de, vt); }
  template<u8 e> auto VRCPH(r128& vd, u8 de, cr128& vt) -> void;
  template<bool D, u8 e>
  auto VRND(r128& vd, u8 vs, cr128& vt) -> void;
  template<u8 e> auto VRNDN(r128& vd, u8 vs, cr128& vt) -> void { VRND<0, e>(vd, vs, vt); }
  template<u8 e> auto VRNDP(r128& vd, u8 vs, cr128& vt) -> void { VRND<1, e>(vd, vs, vt); }
  template<bool L, u8 e>
  auto VRSQ(r128& vd, u8 de, cr128& vt) -> void;
  template<u8 e> auto VRSQ(r128& vd, u8 de, cr128& vt) -> void { VRSQ<0, e>(vd, de, vt); }
  template<u8 e> auto VRSQL(r128& vd, u8 de, cr128& vt) -> void { VRSQ<1, e>(vd, de, vt); }
  template<u8 e> auto VRSQH(r128& vd, u8 de, cr128& vt) -> void;
  template<u8 e> auto VSAR(r128& vd, cr128& vs) -> void;
  template<u8 e> auto VSUB(r128& vd, cr128& vs, cr128& vt) -> void;
  template<u8 e> auto VSUBC(r128& vd, cr128& vs, cr128& vt) -> void;
  template<u8 e> auto VXOR(r128& rd, cr128& vs, cr128& vt) -> void;
  template<u8 e> auto VZERO(r128& rd, cr128& vs, cr128& vt) -> void;

//unserialized:
  u16 reciprocals[512];
  u16 inverseSquareRoots[512];

  //decoder.cpp
  auto decoderEXECUTE(u32 instruction) const -> OpInfo;
  auto decoderSPECIAL(u32 instruction) const -> OpInfo;
  auto decoderREGIMM(u32 instruction) const -> OpInfo;
  auto decoderSCC(u32 instruction) const -> OpInfo;
  auto decoderVU(u32 instruction) const -> OpInfo;
  auto decoderLWC2(u32 instruction) const -> OpInfo;
  auto decoderSWC2(u32 instruction) const -> OpInfo;

  //interpreter.cpp
  auto interpreterEXECUTE() -> void;
  auto interpreterSPECIAL() -> void;
  auto interpreterREGIMM() -> void;
  auto interpreterSCC() -> void;
  auto interpreterVU() -> void;
  auto interpreterLWC2() -> void;
  auto interpreterSWC2() -> void;

  auto INVALID() -> void;
};

extern RSP rsp;
