namespace ares::N64 {
#include "rsp.hpp"
RSP rsp;

namespace {
    const char* CMD_RSPQ[] = {
      "INVALID",
      "NOOP",
      "JUMP",
      "CALL",
      "RET",
      "DMA",
      "WRITE_STATUS",
      "SWAP_BUFFERS",
      "TEST_WRITE_STATUS",
      "RDP_WAIT_IDLE",
      "RDP_SET_BUFFER",
      "RDP_APPEND_BUFFER",
      "??",
      "??",
      "??",
      "??",
    };
    const char* CMD_T3D[] = {
      "TRI_DRAW",
      "SCREEN_SIZE",
      "MATRIX_STACK",
      "SET_WORD",
      "VERT_LOAD",
      "LIGHT_SET",
      "DRAWFLAGS",
      "PROJ_SET",
      "FOG_RANGE",
      "FOG_STATE",
      "TRI_SYNC",
      "TRI_STRIP",
      "??",
      "??",
      "??",
      "??",
    };

    constexpr uint32_t addrTable = 0x1e8;
}

#include "decoder.cpp"
#include "dma.cpp"
#include "io.cpp"
#include "interpreter.cpp"
#include "interpreter-ipu.cpp"
#include "interpreter-scc.cpp"
#include "interpreter-vpu.cpp"
#include "serialization.cpp"

auto RSP::load() -> void {
  dmem.allocate(4 * 1024);
  imem.allocate(4 * 1024);
}

auto RSP::unload() -> void {
  dmem.reset();
  imem.reset();
}

auto RSP::main() -> void {
  while(Thread::clock < 0) {
    auto clock = Thread::clock;

    if(status.halted) {
      step(128);
    } else {
      instruction();
    }

    dmaStep(Thread::clock - clock);
  }
}

auto RSP::instruction() -> void {
  {
    u32 instruction = imem.read<Word>(ipu.pc);

    /*if(dmem.read<2>(addrTable) == 0) {
        printf(" TABLE=0: pc:%08X\n", ipu.pc);
    }

    // Libdragon load command
    if(ipu.pc == 0x44 && instruction == 0x24080800) {
      uint32_t word = ipu.r[4].u32;
      uint32_t ovl = (word >> 28) & 0xF;
      uint32_t cmd = (word >> 24) & 0xF;
      if(ovl == 0) {
        printf("[LD-RSP] Fetch Command: %08X | RSPQ: %s\n", ipu.r[4], CMD_RSPQ[cmd]); // a0
      } else if(ovl == 1) {
        printf("[LD-RSP] Fetch Command: %08X | T3D: %s\n", ipu.r[4], CMD_T3D[cmd]); // a0
      } else {
        printf("[LD-RSP] Fetch Command: %08X | Ovl:%d Cmd:%d\n", ipu.r[4], ovl, cmd); // a0
      }

      printf("  Table: ");
      for(int t=0; t<12; ++t) {
        auto table = dmem.read<2>(addrTable + t*2);
        printf("%04X ", (table << 2) & 0xFFF);
      }
      printf("\n");
    }*/

    instructionPrologue(instruction);
    pipeline.begin();
    OpInfo op0 = decoderEXECUTE(instruction);
    pipeline.issue(op0);
    interpreterEXECUTE();

    if(!pipeline.singleIssue && !op0.branch()) {
      u32 instruction = imem.read<Word>(ipu.pc + 4);
      OpInfo op1 = decoderEXECUTE(instruction);

      if(canDualIssue(op0, op1)) {
        instructionEpilogue<0>(0);
        instructionPrologue(instruction);
        pipeline.issue(op1);
        interpreterEXECUTE();
      }
    }

    pipeline.end();
    instructionEpilogue<0>(0);
  }

  //this handles all stepping for the interpreter
  //with the recompiler, it only steps for taken branch stalls
  step(pipeline.clocks);
}

auto RSP::instructionPrologue(u32 instruction) -> void {
  pipeline.address = ipu.pc;
  pipeline.instruction = instruction;
}

template<bool Recompiled>
auto RSP::instructionEpilogue(u32 clocks) -> s32 {
  if constexpr(Recompiled) {
    step(clocks);
  } else {
    ipu.r[0].u32 = 0;
  }

  switch(branch.state) {
  case Branch::Step: ipu.pc += 4; return status.halted;
  case Branch::Take: ipu.pc += 4; branch.delaySlot(); return status.halted;
  case Branch::DelaySlot:
    ipu.pc = branch.pc;
    branch.reset();
    pipeline.stall();
    if(branch.pc & 4) pipeline.singleIssue = 1;
    return 1;
  }

  unreachable;
}

auto RSP::power(bool reset) -> void {
  Thread::reset();
  dmem.fill();
  imem.fill();

  pipeline = {};
  dma = {};
  status.semaphore = 0;
  status.halted = 1;
  status.broken = 0;
  status.full = 0;
  status.singleStep = 0;
  status.interruptOnBreak = 0;
  for(auto& signal : status.signal) signal = 0;
  for(auto& r : ipu.r) r.u32 = 0;
  ipu.pc = 0;
  branch = {};
  for(auto& r : vpu.r) r = zero;
  vpu.acch = zero;
  vpu.accm = zero;
  vpu.accl = zero;
  vpu.vcoh = zero;
  vpu.vcol = zero;
  vpu.vcch = zero;
  vpu.vccl = zero;
  vpu.vce = zero;
  vpu.divin = 0;
  vpu.divout = 0;
  vpu.divdp = 0;

  reciprocals[0] = u16(~0);
  for(u16 index : range(1, 512)) {
    u64 a = index + 512;
    u64 b = (u64(1) << 34) / a;
    reciprocals[index] = u16(b + 1 >> 8);
  }

  for(u16 index : range(0, 512)) {
    u64 a = index + 512 >> (index % 2 == 1);
    u64 b = 1 << 17;
    //find the largest b where b < 1.0 / sqrt(a)
    while(a * (b + 1) * (b + 1) < (u64(1) << 44)) b++;
    inverseSquareRoots[index] = u16(b >> 1);
  }
}

}
