#define ACCH vpu.acch
#define ACCM vpu.accm
#define ACCL vpu.accl
#define VCOH vpu.vcoh
#define VCOL vpu.vcol
#define VCCH vpu.vcch
#define VCCL vpu.vccl
#define VCE  vpu.vce

#define DIVIN  vpu.divin
#define DIVOUT vpu.divout
#define DIVDP  vpu.divdp

static auto countLeadingZeros(u32 value) -> u32 {
#if defined(COMPILER_MICROSOFT)
  unsigned long index;
  _BitScanReverse(&index, value);
  return index ^ 31;
#elif __has_builtin(__builtin_clz)
  return __builtin_clz(value);
#else
  s32 index;
  for(index = 31; index >= 0; --index) {
    if(value >> index & 1) break;
  }
  return 31 - index;
#endif
}

auto RSP::r128::operator()(u32 index) const -> r128 {
  r128 v{*this};
  switch(index) {
  case  0: break;
  case  1: break;
  case  2: v.u16(1) = v.u16(0); v.u16(3) = v.u16(2); v.u16(5) = v.u16(4); v.u16(7) = v.u16(6); break;
  case  3: v.u16(0) = v.u16(1); v.u16(2) = v.u16(3); v.u16(4) = v.u16(5); v.u16(6) = v.u16(7); break;
  case  4: v.u16(1) = v.u16(2) = v.u16(3) = v.u16(0); v.u16(5) = v.u16(6) = v.u16(7) = v.u16(4); break;
  case  5: v.u16(0) = v.u16(2) = v.u16(3) = v.u16(1); v.u16(4) = v.u16(6) = v.u16(7) = v.u16(5); break;
  case  6: v.u16(0) = v.u16(1) = v.u16(3) = v.u16(2); v.u16(4) = v.u16(5) = v.u16(7) = v.u16(6); break;
  case  7: v.u16(0) = v.u16(1) = v.u16(2) = v.u16(3); v.u16(4) = v.u16(5) = v.u16(6) = v.u16(7); break;
  case  8: for(u32 n : range(8)) v.u16(n) = v.u16(0); break;
  case  9: for(u32 n : range(8)) v.u16(n) = v.u16(1); break;
  case 10: for(u32 n : range(8)) v.u16(n) = v.u16(2); break;
  case 11: for(u32 n : range(8)) v.u16(n) = v.u16(3); break;
  case 12: for(u32 n : range(8)) v.u16(n) = v.u16(4); break;
  case 13: for(u32 n : range(8)) v.u16(n) = v.u16(5); break;
  case 14: for(u32 n : range(8)) v.u16(n) = v.u16(6); break;
  case 15: for(u32 n : range(8)) v.u16(n) = v.u16(7); break;
  }
  return v;
}

auto RSP::accumulatorGet(u32 index) const -> u64 {
  return (u64)ACCH.u16(index) << 32 | (u64)ACCM.u16(index) << 16 | (u64)ACCL.u16(index) << 0;
}

auto RSP::accumulatorSet(u32 index, u64 value) -> void {
  ACCH.u16(index) = value >> 32;
  ACCM.u16(index) = value >> 16;
  ACCL.u16(index) = value >>  0;
}

auto RSP::accumulatorSaturate(u32 index, bool slice, u16 negative, u16 positive) const -> u16 {
  if(ACCH.s16(index) < 0) {
    if(ACCH.u16(index) != 0xffff) return negative;
    if(ACCM.s16(index) >= 0) return negative;
  } else {
    if(ACCH.u16(index) != 0x0000) return positive;
    if(ACCM.s16(index) < 0) return positive;
  }
  return !slice ? ACCL.u16(index) : ACCM.u16(index);
}

auto RSP::CFC2(r32& rt, u8 rd) -> void {
  r128 hi, lo;
  switch(rd & 3) {
  case 0x00: hi = VCOH; lo = VCOL; break;
  case 0x01: hi = VCCH; lo = VCCL; break;
  case 0x02: hi = zero; lo = VCE;  break;
  case 0x03: hi = zero; lo = VCE;  break;  //unverified
  }

  rt.u32 = 0;
  for(u32 n : range(8)) {
    rt.u32 |= lo.get(n) << 0 + n;
    rt.u32 |= hi.get(n) << 8 + n;
  }
  rt.u32 = s16(rt.u32);
}

auto RSP::CTC2(cr32& rt, u8 rd) -> void {
  r128* hi;
  r128* lo;
  r128 null;
  switch(rd & 3) {
  case 0x00: hi = &VCOH;   lo = &VCOL; break;
  case 0x01: hi = &VCCH;   lo = &VCCL; break;
  case 0x02: hi = nullptr; lo = &VCE;  break;
  case 0x03: hi = nullptr; lo = &VCE;  break;  //unverified
  }

  for(u32 n : range(8)) {
    *lo = r128{n, rt.u32 & 1 << 0 + n};
    *hi = r128{n, rt.u32 & 1 << 8 + n};
  }
}

template<u8 e>
auto RSP::LBV(r128& vt, cr32& rs, s8 imm) -> void {
  auto address = rs.u32 + imm;
  vt.byte(e) = dmem.read<Byte>(address);
}

template<u8 e>
auto RSP::LDV(r128& vt, cr32& rs, s8 imm) -> void {
  auto address = rs.u32 + imm * 8;
  auto start = e;
  auto end = min(start + 8, 16);
  for(u32 offset = start; offset < end; offset++) {
    vt.byte(offset & 15) = dmem.read<Byte>(address++);
  }
}

template<u8 e>
auto RSP::LFV(r128& vt, cr32& rs, s8 imm) -> void {
  auto address = rs.u32 + imm * 16;
  auto index = (address & 7) - e;
  address &= ~7;
  auto start = e;
  auto end = min(start + 8, 16);
  r128 tmp;
  for(u32 offset = 0; offset < 4; offset++) {
    tmp.element(offset + 0) = dmem.read<Byte>(address + (index + offset * 4 + 0 & 15)) << 7;
    tmp.element(offset + 4) = dmem.read<Byte>(address + (index + offset * 4 + 8 & 15)) << 7;
  }
  for(u32 offset = start; offset < end; offset++) {
    vt.byte(offset) = tmp.byte(offset);
  }
}

template<u8 e>
auto RSP::LHV(r128& vt, cr32& rs, s8 imm) -> void {
  auto address = rs.u32 + imm * 16;
  auto index = (address & 7) - e;
  address &= ~7;
  for(u32 offset = 0; offset < 8; offset++) {
    vt.element(offset) = dmem.read<Byte>(address + (index + offset * 2 & 15)) << 7;
  }
}

template<u8 e>
auto RSP::LLV(r128& vt, cr32& rs, s8 imm) -> void {
  auto address = rs.u32 + imm * 4;
  auto start = e;
  auto end = min(start + 4, 16);
  for(u32 offset = start; offset < end; offset++) {
    vt.byte(offset & 15) = dmem.read<Byte>(address++);
  }
}

template<u8 e>
auto RSP::LPV(r128& vt, cr32& rs, s8 imm) -> void {
  auto address = rs.u32 + imm * 8;
  auto index = (address & 7) - e;
  address &= ~7;
  for(u32 offset = 0; offset < 8; offset++) {
    vt.element(offset) = dmem.read<Byte>(address + (index + offset & 15)) << 8;
  }
}

template<u8 e>
auto RSP::LQV(r128& vt, cr32& rs, s8 imm) -> void {
  auto address = rs.u32 + imm * 16;
  auto start = e;
  auto end = min(16 + e - (address & 15), 16);
  for(u32 offset = start; offset < end; offset++) {
    vt.byte(offset & 15) = dmem.read<Byte>(address++);
  }
}

template<u8 e>
auto RSP::LRV(r128& vt, cr32& rs, s8 imm) -> void {
  auto address = rs.u32 + imm * 16;
  auto index = e;
  auto start = 16 - ((address & 15) - index);
  address &= ~15;
  for(u32 offset = start; offset < 16; offset++) {
    vt.byte(offset & 15) = dmem.read<Byte>(address++);
  }
}

template<u8 e>
auto RSP::LSV(r128& vt, cr32& rs, s8 imm) -> void {
  auto address = rs.u32 + imm * 2;
  auto start = e;
  auto end = min(start + 2, 16);
  for(u32 offset = start; offset < end; offset++) {
    vt.byte(offset & 15) = dmem.read<Byte>(address++);
  }
}

template<u8 e>
auto RSP::LTV(u8 vt, cr32& rs, s8 imm) -> void {
  auto address = rs.u32 + imm * 16;
  auto begin = address & ~7;
  address = begin + ((e + (address & 8)) & 15);
  auto vtbase = vt & ~7;
  auto vtoff = e >> 1;
  for (u32 i = 0; i < 8; i++) {
    vpu.r[vtbase + vtoff].byte(i * 2 + 0) = dmem.read<Byte>(address++);
    if (address == begin + 16) address = begin;
    vpu.r[vtbase + vtoff].byte(i * 2 + 1) = dmem.read<Byte>(address++);
    if (address == begin + 16) address = begin;
    vtoff = vtoff + 1 & 7;
  }
}

template<u8 e>
auto RSP::LUV(r128& vt, cr32& rs, s8 imm) -> void {
  auto address = rs.u32 + imm * 8;
  auto index = (address & 7) - e;
  address &= ~7;
  for(u32 offset = 0; offset < 8; offset++) {
    vt.element(offset) = dmem.read<Byte>(address + (index + offset & 15)) << 7;
  }
}

template<u8 e>
auto RSP::LWV(r128& vt, cr32& rs, s8 imm) -> void {
  auto address = rs.u32 + imm * 16;
  auto start = 16 - e;
  auto end = e + 16;
  for(u32 offset = start; offset < end; offset++) {
    vt.byte(offset & 15) = dmem.read<Byte>(address);
    address += 4;
  }
}

template<u8 e>
auto RSP::MFC2(r32& rt, cr128& vs) -> void {
  auto hi = vs.byte(e + 0 & 15);
  auto lo = vs.byte(e + 1 & 15);
  rt.u32 = s16(hi << 8 | lo << 0);
}

template<u8 e>
auto RSP::MTC2(cr32& rt, r128& vs) -> void {
               vs.byte(e + 0) = rt.u32 >> 8;
  if (e != 15) vs.byte(e + 1) = rt.u32 >> 0;
}

template<u8 e>
auto RSP::SBV(cr128& vt, cr32& rs, s8 imm) -> void {
  auto address = rs.u32 + imm;
  dmem.write<Byte>(address, vt.byte(e));
}

template<u8 e>
auto RSP::SDV(cr128& vt, cr32& rs, s8 imm) -> void {
  auto address = rs.u32 + imm * 8;
  auto start = e;
  auto end = start + 8;
  for(u32 offset = start; offset < end; offset++) {
    dmem.write<Byte>(address++, vt.byte(offset & 15));
  }
}

template<u8 e>
auto RSP::SFV(cr128& vt, cr32& rs, s8 imm) -> void {
  auto address = rs.u32 + imm * 16;
  auto base = address & 7;
  address &= ~7;
  switch (e) {
  case 0: case 15:
    dmem.write<Byte>(address + (base +  0 & 15), vt.element(0) >> 7);
    dmem.write<Byte>(address + (base +  4 & 15), vt.element(1) >> 7);
    dmem.write<Byte>(address + (base +  8 & 15), vt.element(2) >> 7);
    dmem.write<Byte>(address + (base + 12 & 15), vt.element(3) >> 7);
    break;
  case 1:
    dmem.write<Byte>(address + (base +  0 & 15), vt.element(6) >> 7);
    dmem.write<Byte>(address + (base +  4 & 15), vt.element(7) >> 7);
    dmem.write<Byte>(address + (base +  8 & 15), vt.element(4) >> 7);
    dmem.write<Byte>(address + (base + 12 & 15), vt.element(5) >> 7);
    break;
  case 4:
    dmem.write<Byte>(address + (base +  0 & 15), vt.element(1) >> 7);
    dmem.write<Byte>(address + (base +  4 & 15), vt.element(2) >> 7);
    dmem.write<Byte>(address + (base +  8 & 15), vt.element(3) >> 7);
    dmem.write<Byte>(address + (base + 12 & 15), vt.element(0) >> 7);
    break;
  case 5:
    dmem.write<Byte>(address + (base +  0 & 15), vt.element(7) >> 7);
    dmem.write<Byte>(address + (base +  4 & 15), vt.element(4) >> 7);
    dmem.write<Byte>(address + (base +  8 & 15), vt.element(5) >> 7);
    dmem.write<Byte>(address + (base + 12 & 15), vt.element(6) >> 7);
    break;
  case 8:
    dmem.write<Byte>(address + (base +  0 & 15), vt.element(4) >> 7);
    dmem.write<Byte>(address + (base +  4 & 15), vt.element(5) >> 7);
    dmem.write<Byte>(address + (base +  8 & 15), vt.element(6) >> 7);
    dmem.write<Byte>(address + (base + 12 & 15), vt.element(7) >> 7);
    break;
  case 11:
    dmem.write<Byte>(address + (base +  0 & 15), vt.element(3) >> 7);
    dmem.write<Byte>(address + (base +  4 & 15), vt.element(0) >> 7);
    dmem.write<Byte>(address + (base +  8 & 15), vt.element(1) >> 7);
    dmem.write<Byte>(address + (base + 12 & 15), vt.element(2) >> 7);
    break;
  case 12:
    dmem.write<Byte>(address + (base +  0 & 15), vt.element(5) >> 7);
    dmem.write<Byte>(address + (base +  4 & 15), vt.element(6) >> 7);
    dmem.write<Byte>(address + (base +  8 & 15), vt.element(7) >> 7);
    dmem.write<Byte>(address + (base + 12 & 15), vt.element(4) >> 7);
    break;
  default:
    dmem.write<Byte>(address + (base +  0 & 15), 0);
    dmem.write<Byte>(address + (base +  4 & 15), 0);
    dmem.write<Byte>(address + (base +  8 & 15), 0);
    dmem.write<Byte>(address + (base + 12 & 15), 0);
    break;
  }
}

template<u8 e>
auto RSP::SHV(cr128& vt, cr32& rs, s8 imm) -> void {
  auto address = rs.u32 + imm * 16;
  auto index = address & 7;
  address &= ~7;
  for(u32 offset = 0; offset < 8; offset++) {
    auto byte = e + offset * 2;
    auto value = vt.byte(byte + 0 & 15) << 1 | vt.byte(byte + 1 & 15) >> 7;
    dmem.write<Byte>(address + (index + offset * 2 & 15), value);
  }
}

template<u8 e>
auto RSP::SLV(cr128& vt, cr32& rs, s8 imm) -> void {
  auto address = rs.u32 + imm * 4;
  auto start = e;
  auto end = start + 4;
  for(u32 offset = start; offset < end; offset++) {
    dmem.write<Byte>(address++, vt.byte(offset & 15));
  }
}

template<u8 e>
auto RSP::SPV(cr128& vt, cr32& rs, s8 imm) -> void {
  auto address = rs.u32 + imm * 8;
  auto start = e;
  auto end = start + 8;
  for(u32 offset = start; offset < end; offset++) {
    if((offset & 15) < 8) {
      dmem.write<Byte>(address++, vt.byte((offset & 7) << 1));
    } else {
      dmem.write<Byte>(address++, vt.element(offset & 7) >> 7);
    }
  }
}

template<u8 e>
auto RSP::SQV(cr128& vt, cr32& rs, s8 imm) -> void {
  auto address = rs.u32 + imm * 16;
  auto start = e;
  auto end = start + (16 - (address & 15));
  for(u32 offset = start; offset < end; offset++) {
    dmem.write<Byte>(address++, vt.byte(offset & 15));
  }
}

template<u8 e>
auto RSP::SRV(cr128& vt, cr32& rs, s8 imm) -> void {
  auto address = rs.u32 + imm * 16;
  auto start = e;
  auto end = start + (address & 15);
  auto base = 16 - (address & 15);
  address &= ~15;
  for(u32 offset = start; offset < end; offset++) {
    dmem.write<Byte>(address++, vt.byte(offset + base & 15));
  }
}

template<u8 e>
auto RSP::SSV(cr128& vt, cr32& rs, s8 imm) -> void {
  auto address = rs.u32 + imm * 2;
  auto start = e;
  auto end = start + 2;
  for(u32 offset = start; offset < end; offset++) {
    dmem.write<Byte>(address++, vt.byte(offset & 15));
  }
}

template<u8 e>
auto RSP::STV(u8 vt, cr32& rs, s8 imm) -> void {
  auto address = rs.u32 + imm * 16;
  auto start = vt & ~7;
  auto end = start + 8;
  auto element = 16 - (e & ~1);
  auto base = (address & 7) - (e & ~1);
  address &= ~7;
  for(u32 offset = start; offset < end; offset++) {
    dmem.write<Byte>(address + (base++ & 15), vpu.r[offset].byte(element++ & 15));
    dmem.write<Byte>(address + (base++ & 15), vpu.r[offset].byte(element++ & 15));
  }
}

template<u8 e>
auto RSP::SUV(cr128& vt, cr32& rs, s8 imm) -> void {
  auto address = rs.u32 + imm * 8;
  auto start = e;
  auto end = start + 8;
  for(u32 offset = start; offset < end; offset++) {
    if((offset & 15) < 8) {
      dmem.write<Byte>(address++, vt.element(offset & 7) >> 7);
    } else {
      dmem.write<Byte>(address++, vt.byte((offset & 7) << 1));
    }
  }
}

template<u8 e>
auto RSP::SWV(cr128& vt, cr32& rs, s8 imm) -> void {
  auto address = rs.u32 + imm * 16;
  auto start = e;
  auto end = start + 16;
  auto base = address & 7;
  address &= ~7;
  for(u32 offset = start; offset < end; offset++) {
    dmem.write<Byte>(address + (base++ & 15), vt.byte(offset & 15));
  }
}

template<u8 e>
auto RSP::VABS(r128& vd, cr128& vs, cr128& vt) -> void {
  if constexpr(Accuracy::RSP::SISD) {
    r128 vte = vt(e);
    for(u32 n : range(8)) {
      if(vs.s16(n) < 0) {
        if(vte.s16(n) == -32768) {
          ACCL.s16(n)  = -32768;
          vd.s16(n)    = 32767;
        } else {
          ACCL.s16(n) = -vte.s16(n);
          vd.s16(n)   = -vte.s16(n);
        }
      } else if(vs.s16(n) > 0) {
        ACCL.s16(n) = +vte.s16(n);
        vd.s16(n)   = +vte.s16(n);
      } else {
        ACCL.s16(n) = 0;
        vd.s16(n)   = 0;
      }
    }
  }

  if constexpr(Accuracy::RSP::SIMD) {
    #if ARCHITECTURE_SUPPORTS_SSE4_1
    r128 vs0, slt;
    vs0  = _mm_cmpeq_epi16(vs, zero);
    slt  = _mm_srai_epi16(vs, 15);
    vd   = _mm_andnot_si128(vs0, vt(e));
    vd   = _mm_xor_si128(vd, slt);
    ACCL = _mm_sub_epi16(vd, slt);
    vd   = _mm_subs_epi16(vd, slt);
    #endif
  }
}

template<u8 e>
auto RSP::VADD(r128& vd, cr128& vs, cr128& vt) -> void {
  if constexpr(Accuracy::RSP::SISD) {
    cr128 vte = vt(e);
    for(u32 n : range(8)) {
      s32 result = vs.s16(n) + vte.s16(n) + VCOL.get(n);
      ACCL.s16(n) = result;
      vd.s16(n) = sclamp<16>(result);
    }
    VCOL = zero;
    VCOH = zero;
  }

  if constexpr(Accuracy::RSP::SIMD) {
    #if ARCHITECTURE_SUPPORTS_SSE4_1
    r128 vte = vt(e), sum, min, max;
    sum  = _mm_add_epi16(vs, vte);
    ACCL = _mm_sub_epi16(sum, VCOL);
    min  = _mm_min_epi16(vs, vte);
    max  = _mm_max_epi16(vs, vte);
    min  = _mm_subs_epi16(min, VCOL);
    vd   = _mm_adds_epi16(min, max);
    VCOL = zero;
    VCOH = zero;
    #endif
  }
}

template<u8 e>
auto RSP::VADDC(r128& vd, cr128& vs, cr128& vt) -> void {
  if constexpr(Accuracy::RSP::SISD) {
    cr128 vte = vt(e);
    for(u32 n : range(8)) {
      u32 result = vs.u16(n) + vte.u16(n);
      ACCL.u16(n) = result;
      VCOL.set(n, result >> 16);
    }
    VCOH = zero;
    vd = ACCL;
  }

  if constexpr(Accuracy::RSP::SIMD) {
    #if ARCHITECTURE_SUPPORTS_SSE4_1
    r128 vte = vt(e), sum;
    sum  = _mm_adds_epu16(vs, vte);
    ACCL = _mm_add_epi16(vs, vte);
    VCOL = _mm_cmpeq_epi16(sum, ACCL);
    VCOL = _mm_cmpeq_epi16(VCOL, zero);
    VCOH = zero;
    vd   = ACCL;
    #endif
  }
}

template<u8 e>
auto RSP::VAND(r128& vd, cr128& vs, cr128& vt) -> void {
  if constexpr(Accuracy::RSP::SISD) {
    r128 vte = vt(e);
    for(u32 n : range(8)) {
      ACCL.u16(n) = vs.u16(n) & vte.u16(n);
    }
    vd = ACCL;
  }

  if constexpr(Accuracy::RSP::SIMD) {
    #if ARCHITECTURE_SUPPORTS_SSE4_1
    ACCL = _mm_and_si128(vs, vt(e));
    vd   = ACCL;
    #endif
  }
}

template<u8 e>
auto RSP::VCH(r128& vd, cr128& vs, cr128& vt) -> void {
  if constexpr(Accuracy::RSP::SISD) {
    cr128 vte = vt(e);
    for(u32 n : range(8)) {
      if((vs.s16(n) ^ vte.s16(n)) < 0) {
        s16 result = vs.s16(n) + vte.s16(n);
        ACCL.s16(n) = (result <= 0 ? -vte.s16(n) : vs.s16(n));
        VCCL.set(n, result <= 0);
        VCCH.set(n, vte.s16(n) < 0);
        VCOL.set(n, 1);
        VCOH.set(n, result != 0 && vs.u16(n) != (vte.u16(n) ^ 0xffff));
        VCE.set(n, result == -1);
      } else {
        s16 result = vs.s16(n) - vte.s16(n);
        ACCL.s16(n) = (result >= 0 ? vte.s16(n) : vs.s16(n));
        VCCL.set(n, vte.s16(n) < 0);
        VCCH.set(n, result >= 0);
        VCOL.set(n, 0);
        VCOH.set(n, result != 0 && vs.u16(n) != (vte.u16(n) ^ 0xffff));
        VCE.set(n, 0);
      }
    }
    vd = ACCL;
  }

  if constexpr(Accuracy::RSP::SIMD) {
    #if ARCHITECTURE_SUPPORTS_SSE4_1
    r128 vte = vt(e), nvt, diff, diff0, vtn, dlez, dgez, mask;
    VCOL  = _mm_xor_si128(vs, vte);
    VCOL  = _mm_cmplt_epi16(VCOL, zero);
    nvt   = _mm_xor_si128(vte, VCOL);
    nvt   = _mm_sub_epi16(nvt, VCOL);
    diff  = _mm_sub_epi16(vs, nvt);
    diff0 = _mm_cmpeq_epi16(diff, zero);
    vtn   = _mm_cmplt_epi16(vte, zero);
    dlez  = _mm_cmpgt_epi16(diff, zero);
    dgez  = _mm_or_si128(dlez, diff0);
    dlez  = _mm_cmpeq_epi16(zero, dlez);
    VCCH  = _mm_blendv_epi8(dgez, vtn, VCOL);
    VCCL  = _mm_blendv_epi8(vtn, dlez, VCOL);
    VCE   = _mm_cmpeq_epi16(diff, VCOL);
    VCE   = _mm_and_si128(VCE, VCOL);
    VCOH  = _mm_or_si128(diff0, VCE);
    VCOH  = _mm_cmpeq_epi16(VCOH, zero);
    mask  = _mm_blendv_epi8(VCCH, VCCL, VCOL);
    ACCL  = _mm_blendv_epi8(vs, nvt, mask);
    vd    = ACCL;
    #endif
  }
}

template<u8 e>
auto RSP::VCL(r128& vd, cr128& vs, cr128& vt) -> void {
  if constexpr(Accuracy::RSP::SISD) {
    cr128 vte = vt(e);
    for(u32 n : range(8)) {
      if(VCOL.get(n)) {
        if(VCOH.get(n)) {
          ACCL.u16(n) = VCCL.get(n) ? -vte.u16(n) : vs.u16(n);
        } else {
          u16 sum = vs.u16(n) + vte.u16(n);
          bool carry = (vs.u16(n) + vte.u16(n)) != sum;
          if(VCE.get(n)) {
            ACCL.u16(n) = VCCL.set(n, (!sum || !carry)) ? -vte.u16(n) : vs.u16(n);
          } else {
            ACCL.u16(n) = VCCL.set(n, (!sum && !carry)) ? -vte.u16(n) : vs.u16(n);
          }
        }
      } else {
        if(VCOH.get(n)) {
          ACCL.u16(n) = VCCH.get(n) ? vte.u16(n) : vs.u16(n);
        } else {
          ACCL.u16(n) = VCCH.set(n, (s32)vs.u16(n) - (s32)vte.u16(n) >= 0) ? vte.u16(n) : vs.u16(n);
        }
      }
    }
    VCOL = zero;
    VCOH = zero;
    VCE = zero;
    vd = ACCL;
  }

  if constexpr(Accuracy::RSP::SIMD) {
    #if ARCHITECTURE_SUPPORTS_SSE4_1
    r128 vte = vt(e), nvt, diff, ncarry, nvce, diff0, lec1, lec2, leeq, geeq, le, ge, mask;
    nvt    = _mm_xor_si128(vte, VCOL);
    nvt    = _mm_sub_epi16(nvt, VCOL);
    diff   = _mm_sub_epi16(vs, nvt);
    ncarry = _mm_adds_epu16(vs, vte);
    ncarry = _mm_cmpeq_epi16(diff, ncarry);
    nvce   = _mm_cmpeq_epi16(VCE, zero);
    diff0  = _mm_cmpeq_epi16(diff, zero);
    lec1   = _mm_and_si128(diff0, ncarry);
    lec1   = _mm_and_si128(nvce, lec1);
    lec2   = _mm_or_si128(diff0, ncarry);
    lec2   = _mm_and_si128(VCE, lec2);
    leeq   = _mm_or_si128(lec1, lec2);
    geeq   = _mm_subs_epu16(vte, vs);
    geeq   = _mm_cmpeq_epi16(geeq, zero);
    le     = _mm_andnot_si128(VCOH, VCOL);
    le     = _mm_blendv_epi8(VCCL, leeq, le);
    ge     = _mm_or_si128(VCOL, VCOH);
    ge     = _mm_blendv_epi8(geeq, VCCH, ge);
    mask   = _mm_blendv_epi8(ge, le, VCOL);
    ACCL   = _mm_blendv_epi8(vs, nvt, mask);
    VCCH   = ge;
    VCCL   = le;
    VCOH   = zero;
    VCOL   = zero;
    VCE    = zero;
    vd     = ACCL;
    #endif
  }
}

template<u8 e>
auto RSP::VCR(r128& vd, cr128& vs, cr128& vt) -> void {
  if constexpr(Accuracy::RSP::SISD) {
    cr128 vte = vt(e);
    for(u32 n : range(8)) {
      if((vs.s16(n) ^ vte.s16(n)) < 0) {
        VCCH.set(n, vte.s16(n) < 0);
        ACCL.u16(n) = VCCL.set(n, vs.s16(n) + vte.s16(n) + 1 <= 0) ? ~vte.u16(n) : vs.u16(n);
      } else {
        VCCL.set(n, vte.s16(n) < 0);
        ACCL.u16(n) = VCCH.set(n, vs.s16(n) - vte.s16(n) >= 0) ? vte.u16(n) : vs.u16(n);
      }
    }
    VCOL = zero;
    VCOH = zero;
    VCE = zero;
    vd = ACCL;
  }

  if constexpr(Accuracy::RSP::SIMD) {
    #if ARCHITECTURE_SUPPORTS_SSE4_1
    r128 vte = vt(e), sign, dlez, dgez, nvt, mask;
    sign = _mm_xor_si128(vs, vte);
    sign = _mm_srai_epi16(sign, 15);
    dlez = _mm_and_si128(vs, sign);
    dlez = _mm_add_epi16(dlez, vte);
    VCCL = _mm_srai_epi16(dlez, 15);
    dgez = _mm_or_si128(vs, sign);
    dgez = _mm_min_epi16(dgez, vte);
    VCCH = _mm_cmpeq_epi16(dgez, vte);
    nvt  = _mm_xor_si128(vte, sign);
    mask = _mm_blendv_epi8(VCCH, VCCL, sign);
    ACCL = _mm_blendv_epi8(vs, nvt, mask);
    vd   = ACCL;
    VCOL = zero;
    VCOH = zero;
    VCE  = zero;
    #endif
  }
}

template<u8 e>
auto RSP::VEQ(r128& vd, cr128& vs, cr128& vt) -> void {
  if constexpr(Accuracy::RSP::SISD) {
    cr128 vte = vt(e);
    for(u32 n : range(8)) {
      ACCL.u16(n) = VCCL.set(n, !VCOH.get(n) && vs.u16(n) == vte.u16(n)) ? vs.u16(n) : vte.u16(n);
    }
    VCCH = zero;  //unverified
    VCOL = zero;
    VCOH = zero;
    vd = ACCL;
  }

  if constexpr(Accuracy::RSP::SIMD) {
    #if ARCHITECTURE_SUPPORTS_SSE4_1
    r128 vte = vt(e), eq;
    eq   = _mm_cmpeq_epi16(vs, vte);
    VCCL = _mm_andnot_si128(VCOH, eq);
    ACCL = _mm_blendv_epi8(vte, vs, VCCL);
    VCCH = zero;  //unverified
    VCOH = zero;
    VCOL = zero;
    vd   = ACCL;
    #endif
  }
}

template<u8 e>
auto RSP::VGE(r128& vd, cr128& vs, cr128& vt) -> void {
  if constexpr(Accuracy::RSP::SISD) {
    cr128 vte = vt(e);
    for(u32 n : range(8)) {
      ACCL.u16(n) = VCCL.set(n, vs.s16(n) > vte.s16(n) || (vs.s16(n) == vte.s16(n) && (!VCOL.get(n) || !VCOH.get(n)))) ? vs.u16(n) : vte.u16(n);
    }
    VCCH = zero;  //unverified
    VCOL = zero;
    VCOH = zero;
    vd = ACCL;
  }

  if constexpr(Accuracy::RSP::SIMD) {
    #if ARCHITECTURE_SUPPORTS_SSE4_1
    r128 vte = vt(e), eq, gt, es;
    eq   = _mm_cmpeq_epi16(vs, vte);
    gt   = _mm_cmpgt_epi16(vs, vte);
    es   = _mm_and_si128(VCOH, VCOL);
    eq   = _mm_andnot_si128(es, eq);
    VCCL = _mm_or_si128(gt, eq);
    ACCL = _mm_blendv_epi8(vte, vs, VCCL);
    VCCH = zero;
    VCOH = zero;
    VCOL = zero;
    vd   = ACCL;
    #endif
  }
}

template<u8 e>
auto RSP::VLT(r128& vd, cr128& vs, cr128& vt) -> void {
  if constexpr(Accuracy::RSP::SISD) {
    cr128 vte = vt(e);
    for(u32 n : range(8)) {
      ACCL.u16(n) = VCCL.set(n, vs.s16(n) < vte.s16(n) || (vs.s16(n) == vte.s16(n) && VCOL.get(n) && VCOH.get(n))) ? vs.u16(n) : vte.u16(n);
    }
    VCCH = zero;
    VCOL = zero;
    VCOH = zero;
    vd = ACCL;
  }

  if constexpr(Accuracy::RSP::SIMD) {
    #if ARCHITECTURE_SUPPORTS_SSE4_1
    r128 vte = vt(e), eq, lt;
    eq   = _mm_cmpeq_epi16(vs, vte);
    lt   = _mm_cmplt_epi16(vs, vte);
    eq   = _mm_and_si128(VCOH, eq);
    eq   = _mm_and_si128(VCOL, eq);
    VCCL = _mm_or_si128(lt, eq);
    ACCL = _mm_blendv_epi8(vte, vs, VCCL);
    VCCH = zero;
    VCOH = zero;
    VCOL = zero;
    vd   = ACCL;
    #endif
  }
}

template<bool U, u8 e>
auto RSP::VMACF(r128& vd, cr128& vs, cr128& vt) -> void {
  if constexpr(Accuracy::RSP::SISD) {
    cr128 vte = vt(e);
    for(u32 n : range(8)) {
      accumulatorSet(n, accumulatorGet(n) + (s64)vs.s16(n) * (s64)vte.s16(n) * 2);
      if constexpr(U == 0) {
        vd.u16(n) = accumulatorSaturate(n, 1, 0x8000, 0x7fff);
      }
      if constexpr(U == 1) {
        vd.u16(n) = ACCH.s16(n) < 0 ? 0x0000 : ACCH.s16(n) || ACCM.s16(n) < 0 ? 0xffff : ACCM.u16(n);
      }
    }
  }

  if constexpr(Accuracy::RSP::SIMD) {
    #if ARCHITECTURE_SUPPORTS_SSE4_1
    r128 vte = vt(e), lo, md, hi, carry, omask;
    lo    = _mm_mullo_epi16(vs, vte);
    hi    = _mm_mulhi_epi16(vs, vte);
    md    = _mm_slli_epi16(hi, 1);
    carry = _mm_srli_epi16(lo, 15);
    hi    = _mm_srai_epi16(hi, 15);
    md    = _mm_or_si128(md, carry);
    lo    = _mm_slli_epi16(lo, 1);
    omask = _mm_adds_epu16(ACCL, lo);
    ACCL  = _mm_add_epi16(ACCL, lo);
    omask = _mm_cmpeq_epi16(ACCL, omask);
    omask = _mm_cmpeq_epi16(omask, zero);
    md    = _mm_sub_epi16(md, omask);
    carry = _mm_cmpeq_epi16(md, zero);
    carry = _mm_and_si128(carry, omask);
    hi    = _mm_sub_epi16(hi, carry);
    omask = _mm_adds_epu16(ACCM, md);
    ACCM  = _mm_add_epi16(ACCM, md);
    omask = _mm_cmpeq_epi16(ACCM, omask);
    omask = _mm_cmpeq_epi16(omask, zero);
    ACCH  = _mm_add_epi16(ACCH, hi);
    ACCH  = _mm_sub_epi16(ACCH, omask);
    if constexpr(!U) {
      lo = _mm_unpacklo_epi16(ACCM, ACCH);
      hi = _mm_unpackhi_epi16(ACCM, ACCH);
      vd = _mm_packs_epi32(lo, hi);
    } else {
      r128 mmask, hmask;
      mmask = _mm_srai_epi16(ACCM, 15);
      hmask = _mm_srai_epi16(ACCH, 15);
      md    = _mm_or_si128(mmask, ACCM);
      omask = _mm_cmpgt_epi16(ACCH, zero);
      md    = _mm_andnot_si128(hmask, md);
      vd    = _mm_or_si128(omask, md);
    }
    #endif
  }
}

auto RSP::VMACQ(r128& vd) -> void {
  for(u32 n : range(8)) {
    s32 product = ACCH.element(n) << 16 | ACCM.element(n) << 0;
    if(product < 0 && !(product & 1 << 5)) product += 32;
    else if(product >= 32 && !(product & 1 << 5)) product -= 32;
    ACCH.element(n) = product >> 16;
    ACCM.element(n) = product >>  0;
    vd.element(n) = sclamp<16>(product >> 1) & ~15;
  }
}

template<u8 e>
auto RSP::VMADH(r128& vd, cr128& vs, cr128& vt) -> void {
  if constexpr(Accuracy::RSP::SISD) {
    cr128 vte = vt(e);
    for(u32 n : range(8)) {
      s32 result = (accumulatorGet(n) >> 16) + vs.s16(n) * vte.s16(n);
      ACCH.u16(n) = result >> 16;
      ACCM.u16(n) = result >>  0;
      vd.u16(n) = accumulatorSaturate(n, 1, 0x8000, 0x7fff);
    }
  }

  if constexpr(Accuracy::RSP::SIMD) {
    #if ARCHITECTURE_SUPPORTS_SSE4_1
    r128 vte = vt(e), lo, hi, omask;
    lo    = _mm_mullo_epi16(vs, vte);
    hi    = _mm_mulhi_epi16(vs, vte);
    omask = _mm_adds_epu16(ACCM, lo);
    ACCM  = _mm_add_epi16(ACCM, lo);
    omask = _mm_cmpeq_epi16(ACCM, omask);
    omask = _mm_cmpeq_epi16(omask, zero);
    hi    = _mm_sub_epi16(hi, omask);
    ACCH  = _mm_add_epi16(ACCH, hi);
    lo    = _mm_unpacklo_epi16(ACCM, ACCH);
    hi    = _mm_unpackhi_epi16(ACCM, ACCH);
    vd    = _mm_packs_epi32(lo, hi);
    #endif
  }
}

template<u8 e>
auto RSP::VMADL(r128& vd, cr128& vs, cr128& vt) -> void {
  if constexpr(Accuracy::RSP::SISD) {
    cr128 vte = vt(e);
    for(u32 n : range(8)) {
      accumulatorSet(n, accumulatorGet(n) + (u32(vs.u16(n) * vte.u16(n)) >> 16));
      vd.u16(n) = accumulatorSaturate(n, 0, 0x0000, 0xffff);
    }
  }

  if constexpr(Accuracy::RSP::SIMD) {
    #if ARCHITECTURE_SUPPORTS_SSE4_1
    r128 vte = vt(e), hi, omask, nhi, nmd, shi, smd, cmask, cval;
    hi    = _mm_mulhi_epu16(vs, vte);
    omask = _mm_adds_epu16(ACCL, hi);
    ACCL  = _mm_add_epi16(ACCL, hi);
    omask = _mm_cmpeq_epi16(ACCL, omask);
    omask = _mm_cmpeq_epi16(omask, zero);
    hi    = _mm_sub_epi16(zero, omask);
    omask = _mm_adds_epu16(ACCM, hi);
    ACCM  = _mm_add_epi16(ACCM, hi);
    omask = _mm_cmpeq_epi16(ACCM, omask);
    omask = _mm_cmpeq_epi16(omask, zero);
    ACCH  = _mm_sub_epi16(ACCH, omask);
    nhi   = _mm_srai_epi16(ACCH, 15);
    nmd   = _mm_srai_epi16(ACCM, 15);
    shi   = _mm_cmpeq_epi16(nhi, ACCH);
    smd   = _mm_cmpeq_epi16(nhi, nmd);
    cmask = _mm_and_si128(smd, shi);
    cval  = _mm_cmpeq_epi16(nhi, zero);
    vd    = _mm_blendv_epi8(cval, ACCL, cmask);
    #endif
  }
}

template<u8 e>
auto RSP::VMADM(r128& vd, cr128& vs, cr128& vt) -> void {
  if constexpr(Accuracy::RSP::SISD) {
    cr128 vte = vt(e);
    for(u32 n : range(8)) {
      accumulatorSet(n, accumulatorGet(n) + vs.s16(n) * vte.u16(n));
      vd.u16(n) = accumulatorSaturate(n, 1, 0x8000, 0x7fff);
    }
  }

  if constexpr(Accuracy::RSP::SIMD) {
    #if ARCHITECTURE_SUPPORTS_SSE4_1
    r128 vte = vt(e), lo, hi, sign, vta, omask;
    lo    = _mm_mullo_epi16(vs, vte);
    hi    = _mm_mulhi_epu16(vs, vte);
    sign  = _mm_srai_epi16(vs, 15);
    vta   = _mm_and_si128(vte, sign);
    hi    = _mm_sub_epi16(hi, vta);
    omask = _mm_adds_epu16(ACCL, lo);
    ACCL  = _mm_add_epi16(ACCL, lo);
    omask = _mm_cmpeq_epi16(ACCL, omask);
    omask = _mm_cmpeq_epi16(omask, zero);
    hi    = _mm_sub_epi16(hi, omask);
    omask = _mm_adds_epu16(ACCM, hi);
    ACCM  = _mm_add_epi16(ACCM, hi);
    omask = _mm_cmpeq_epi16(ACCM, omask);
    omask = _mm_cmpeq_epi16(omask, zero);
    hi    = _mm_srai_epi16(hi, 15);
    ACCH  = _mm_add_epi16(ACCH, hi);
    ACCH  = _mm_sub_epi16(ACCH, omask);
    lo    = _mm_unpacklo_epi16(ACCM, ACCH);
    hi    = _mm_unpackhi_epi16(ACCM, ACCH);
    vd    = _mm_packs_epi32(lo, hi);
    #endif
  }
}

template<u8 e>
auto RSP::VMADN(r128& vd, cr128& vs, cr128& vt) -> void {
  if constexpr(Accuracy::RSP::SISD) {
    cr128 vte = vt(e);
    for(u32 n : range(8)) {
      accumulatorSet(n, accumulatorGet(n) + s64(vs.u16(n) * vte.s16(n)));
      vd.u16(n) = accumulatorSaturate(n, 0, 0x0000, 0xffff);
    }
  }

  if constexpr(Accuracy::RSP::SIMD) {
    #if ARCHITECTURE_SUPPORTS_SSE4_1
    r128 vte = vt(e), lo, hi, sign, vsa, omask, nhi, nmd, shi, smd, cmask, cval;
    lo    = _mm_mullo_epi16(vs, vte);
    hi    = _mm_mulhi_epu16(vs, vte);
    sign  = _mm_srai_epi16(vte, 15);
    vsa   = _mm_and_si128(vs, sign);
    hi    = _mm_sub_epi16(hi, vsa);
    omask = _mm_adds_epu16(ACCL, lo);
    ACCL  = _mm_add_epi16(ACCL, lo);
    omask = _mm_cmpeq_epi16(ACCL, omask);
    omask = _mm_cmpeq_epi16(omask, zero);
    hi    = _mm_sub_epi16(hi, omask);
    omask = _mm_adds_epu16(ACCM, hi);
    ACCM  = _mm_add_epi16(ACCM, hi);
    omask = _mm_cmpeq_epi16(ACCM, omask);
    omask = _mm_cmpeq_epi16(omask, zero);
    hi    = _mm_srai_epi16(hi, 15);
    ACCH  = _mm_add_epi16(ACCH, hi);
    ACCH  = _mm_sub_epi16(ACCH, omask);
    nhi   = _mm_srai_epi16(ACCH, 15);
    nmd   = _mm_srai_epi16(ACCM, 15);
    shi   = _mm_cmpeq_epi16(nhi, ACCH);
    smd   = _mm_cmpeq_epi16(nhi, nmd);
    cmask = _mm_and_si128(smd, shi);
    cval  = _mm_cmpeq_epi16(nhi, zero);
    vd    = _mm_blendv_epi8(cval, ACCL, cmask);
    #endif
  }
}

template<u8 e>
auto RSP::VMOV(r128& vd, u8 de, cr128& vt) -> void {
  cr128 vte = vt(e);
  vd.u16(de) = vte.u16(de);
  ACCL = vte;
}

template<u8 e>
auto RSP::VMRG(r128& vd, cr128& vs, cr128& vt) -> void {
  if constexpr(Accuracy::RSP::SISD) {
    cr128 vte = vt(e);
    for(u32 n : range(8)) {
      ACCL.u16(n) = VCCL.get(n) ? vs.u16(n) : vte.u16(n);
    }
    VCOH = zero;
    VCOL = zero;
    vd = ACCL;
  }

  if constexpr(Accuracy::RSP::SIMD) {
    #if ARCHITECTURE_SUPPORTS_SSE4_1
    ACCL = _mm_blendv_epi8(vt(e), vs, VCCL);
    VCOH = zero;
    VCOL = zero;
    vd   = ACCL;
    #endif
  }
}

template<u8 e>
auto RSP::VMUDH(r128& vd, cr128& vs, cr128& vt) -> void {
  if constexpr(Accuracy::RSP::SISD) {
    cr128 vte = vt(e);
    for(u32 n : range(8)) {
      accumulatorSet(n, s64(vs.s16(n) * vte.s16(n)) << 16);
      vd.u16(n) = accumulatorSaturate(n, 1, 0x8000, 0x7fff);
    }
  }

  if constexpr(Accuracy::RSP::SIMD) {
    #if ARCHITECTURE_SUPPORTS_SSE4_1
    r128 vte = vt(e), lo, hi;
    ACCL = zero;
    ACCM = _mm_mullo_epi16(vs, vte);
    ACCH = _mm_mulhi_epi16(vs, vte);
    lo   = _mm_unpacklo_epi16(ACCM, ACCH);
    hi   = _mm_unpackhi_epi16(ACCM, ACCH);
    vd   = _mm_packs_epi32(lo, hi);
    #endif
  }
}

template<u8 e>
auto RSP::VMUDL(r128& vd, cr128& vs, cr128& vt) -> void {
  if constexpr(Accuracy::RSP::SISD) {
    cr128 vte = vt(e);
    for(u32 n : range(8)) {
      accumulatorSet(n, u16(vs.u16(n) * vte.u16(n) >> 16));
    }
    vd = ACCL;
  }

  if constexpr(Accuracy::RSP::SIMD) {
    #if ARCHITECTURE_SUPPORTS_SSE4_1
    ACCL = _mm_mulhi_epu16(vs, vt(e));
    ACCM = zero;
    ACCH = zero;
    vd   = ACCL;
    #endif
  }
}

template<u8 e>
auto RSP::VMUDM(r128& vd, cr128& vs, cr128& vt) -> void {
  if constexpr(Accuracy::RSP::SISD) {
    cr128 vte = vt(e);
    for(u32 n : range(8)) {
      accumulatorSet(n, s32(vs.s16(n) * vte.u16(n)));
    }
    vd = ACCM;
  }

  if constexpr(Accuracy::RSP::SIMD) {
    #if ARCHITECTURE_SUPPORTS_SSE4_1
    r128 vte = vt(e), sign, vta;
    ACCL = _mm_mullo_epi16(vs, vte);
    ACCM = _mm_mulhi_epu16(vs, vte);
    sign = _mm_srai_epi16(vs, 15);
    vta  = _mm_and_si128(vte, sign);
    ACCM = _mm_sub_epi16(ACCM, vta);
    ACCH = _mm_srai_epi16(ACCM, 15);
    vd   = ACCM;
    #endif
  }
}

template<u8 e>
auto RSP::VMUDN(r128& vd, cr128& vs, cr128& vt) -> void {
  if constexpr(Accuracy::RSP::SISD) {
    cr128 vte = vt(e);
    for(u32 n : range(8)) {
      accumulatorSet(n, s32(vs.u16(n) * vte.s16(n)));
    }
    vd = ACCL;
  }

  if constexpr(Accuracy::RSP::SIMD) {
    #if ARCHITECTURE_SUPPORTS_SSE4_1
    r128 vte = vt(e), sign, vsa;
    ACCL = _mm_mullo_epi16(vs, vte);
    ACCM = _mm_mulhi_epu16(vs, vte);
    sign = _mm_srai_epi16(vte, 15);
    vsa  = _mm_and_si128(vs, sign);
    ACCM = _mm_sub_epi16(ACCM, vsa);
    ACCH = _mm_srai_epi16(ACCM, 15);
    vd   = ACCL;
    #endif
  }
}

template<bool U, u8 e>
auto RSP::VMULF(r128& vd, cr128& vs, cr128& vt) -> void {
  if constexpr(Accuracy::RSP::SISD) {
    cr128 vte = vt(e);
    for(u32 n : range(8)) {
      accumulatorSet(n, (s64)vs.s16(n) * (s64)vte.s16(n) * 2 + 0x8000);
      if constexpr(U == 0) {
        vd.u16(n) = accumulatorSaturate(n, 1, 0x8000, 0x7fff);
      }
      if constexpr(U == 1) {
        vd.u16(n) = ACCH.s16(n) < 0 ? 0x0000 : (ACCH.s16(n) ^ ACCM.s16(n)) < 0 ? 0xffff : ACCM.u16(n);
      }
    }
  }

  if constexpr(Accuracy::RSP::SIMD) {
    #if ARCHITECTURE_SUPPORTS_SSE4_1
    r128 vte = vt(e), lo, hi, round, sign1, sign2, neq, eq, neg;
    lo    = _mm_mullo_epi16(vs, vte);
    round = _mm_cmpeq_epi16(zero, zero);
    sign1 = _mm_srli_epi16(lo, 15);
    lo    = _mm_add_epi16(lo, lo);
    round = _mm_slli_epi16(round, 15);
    hi    = _mm_mulhi_epi16(vs, vte);
    sign2 = _mm_srli_epi16(lo, 15);
    ACCL  = _mm_add_epi16(round, lo);
    sign1 = _mm_add_epi16(sign1, sign2);
    hi    = _mm_slli_epi16(hi, 1);
    neq   = _mm_cmpeq_epi16(vs, vte);
    ACCM  = _mm_add_epi16(hi, sign1);
    neg   = _mm_srai_epi16(ACCM, 15);
    if constexpr(!U) {
      eq   = _mm_and_si128(neq, neg);
      ACCH = _mm_andnot_si128(neq, neg);
      vd   = _mm_add_epi16(ACCM, eq);
    } else {
      ACCH = _mm_andnot_si128(neq, neg);
      hi   = _mm_or_si128(ACCM, neg);
      vd   = _mm_andnot_si128(ACCH, hi);
    }
    #endif
  }
}

template<u8 e>
auto RSP::VMULQ(r128& vd, cr128& vs, cr128& vt) -> void {
  cr128 vte = vt(e);
  for(u32 n : range(8)) {
    s32 product = (s16)vs.element(n) * (s16)vte.element(n);
    if(product < 0) product += 31;  //round
    ACCH.element(n) = product >> 16;
    ACCM.element(n) = product >>  0;
    ACCL.element(n) = 0;
    vd.element(n) = sclamp<16>(product >> 1) & ~15;
  }
}

template<u8 e>
auto RSP::VNAND(r128& vd, cr128& vs, cr128& vt) -> void {
  if constexpr(Accuracy::RSP::SISD) {
    cr128 vte = vt(e);
    for(u32 n : range(8)) {
      ACCL.u16(n) = ~(vs.u16(n) & vte.u16(n));
    }
    vd = ACCL;
  }

  if constexpr(Accuracy::RSP::SIMD) {
    #if ARCHITECTURE_SUPPORTS_SSE4_1
    ACCL = _mm_and_si128(vs, vt(e));
    ACCL = _mm_xor_si128(ACCL, invert);
    vd   = ACCL;
    #endif
  }
}

template<u8 e>
auto RSP::VNE(r128& vd, cr128& vs, cr128& vt) -> void {
  if constexpr(Accuracy::RSP::SISD) {
    cr128 vte = vt(e);
    for(u32 n : range(8)) {
      ACCL.u16(n) = VCCL.set(n, vs.u16(n) != vte.u16(n) || VCOH.get(n)) ? vs.u16(n) : vte.u16(n);
    }
    VCCH = zero;  //unverified
    VCOL = zero;
    VCOH = zero;
    vd = ACCL;
  }

  if constexpr(Accuracy::RSP::SIMD) {
    #if ARCHITECTURE_SUPPORTS_SSE4_1
    r128 vte = vt(e), eq, ne;
    eq   = _mm_cmpeq_epi16(vs, vte);
    ne   = _mm_cmpeq_epi16(eq, zero);
    VCCL = _mm_and_si128(VCOH, eq);
    VCCL = _mm_or_si128(VCCL, ne);
    ACCL = _mm_blendv_epi8(vte, vs, VCCL);
    VCCH = zero;
    VCOH = zero;
    VCOL = zero;
    vd   = ACCL;
    #endif
  }
}

auto RSP::VNOP() -> void {
}

template<u8 e>
auto RSP::VNOR(r128& vd, cr128& vs, cr128& vt) -> void {
  if constexpr(Accuracy::RSP::SISD) {
    cr128 vte = vt(e);
    for(u32 n : range(8)) {
      ACCL.u16(n) = ~(vs.u16(n) | vte.u16(n));
    }
    vd = ACCL;
  }

  if constexpr(Accuracy::RSP::SIMD) {
    #if ARCHITECTURE_SUPPORTS_SSE4_1
    ACCL = _mm_or_si128(vs, vt(e));
    ACCL = _mm_xor_si128(ACCL, invert);
    vd   = ACCL;
    #endif
  }
}

template<u8 e>
auto RSP::VNXOR(r128& vd, cr128& vs, cr128& vt) -> void {
  if constexpr(Accuracy::RSP::SISD) {
    cr128 vte = vt(e);
    for(u32 n : range(8)) {
      ACCL.u16(n) = ~(vs.u16(n) ^ vte.u16(n));
    }
    vd = ACCL;
  }

  if constexpr(Accuracy::RSP::SIMD) {
    #if ARCHITECTURE_SUPPORTS_SSE4_1
    ACCL = _mm_xor_si128(vs, vt(e));
    ACCL = _mm_xor_si128(ACCL, invert);
    vd   = ACCL;
    #endif
  }
}

template<u8 e>
auto RSP::VOR(r128& vd, cr128& vs, cr128& vt) -> void {
  if constexpr(Accuracy::RSP::SISD) {
    cr128 vte = vt(e);
    for(u32 n : range(8)) {
      ACCL.u16(n) = vs.u16(n) | vte.u16(n);
    }
    vd = ACCL;
  }

  if constexpr(Accuracy::RSP::SIMD) {
    #if ARCHITECTURE_SUPPORTS_SSE4_1
    ACCL = _mm_or_si128(vs, vt(e));
    vd   = ACCL;
    #endif
  }
}

template<bool L, u8 e>
auto RSP::VRCP(r128& vd, u8 de, cr128& vt) -> void {
  s32 result = 0;
  s32 input = L && DIVDP ? DIVIN << 16 | vt.element(e & 7) : s16(vt.element(e & 7));
  s32 mask = input >> 31;
  s32 data = input ^ mask;
  if(input > -32768) data -= mask;
  if(data == 0) {
    result = 0x7fff'ffff;
  } else if(input == -32768) {
    result = 0xffff'0000;
  } else {
    u32 shift = countLeadingZeros(data);
    u32 index = (u64(data) << shift & 0x7fc0'0000) >> 22;
    result = reciprocals[index];
    result = (0x10000 | result) << 14;
    result = result >> 31 - shift ^ mask;
  }
  DIVDP = 0;
  DIVOUT = result >> 16;
  ACCL = vt(e);
  vd.element(de) = result;
}

template<u8 e>
auto RSP::VRCPH(r128& vd, u8 de, cr128& vt) -> void {
  ACCL  = vt(e);
  DIVDP = 1;
  DIVIN = vt.element(e & 7);
  vd.element(de) = DIVOUT;
}

template<bool D, u8 e>
auto RSP::VRND(r128& vd, u8 vs, cr128& vt) -> void {
  cr128 vte = vt(e);
  for(u32 n : range(8)) {
    s32 product = (s16)vte.element(n);
    if(vs & 1) product <<= 16;
    s64 acc = 0;
    acc |= ACCH.element(n); acc <<= 16;
    acc |= ACCM.element(n); acc <<= 16;
    acc |= ACCL.element(n); acc <<= 16;
    acc >>= 16;
    if(D == 0 && acc <  0) acc = sclip<48>(acc + product);
    if(D == 1 && acc >= 0) acc = sclip<48>(acc + product);
    ACCH.element(n) = acc >> 32;
    ACCM.element(n) = acc >> 16;
    ACCL.element(n) = acc >>  0;
    vd.element(n) = sclamp<16>(acc >> 16);
  }
}

template<bool L, u8 e>
auto RSP::VRSQ(r128& vd, u8 de, cr128& vt) -> void {
  s32 result = 0;
  s32 input = L && DIVDP ? DIVIN << 16 | vt.element(e & 7) : s16(vt.element(e & 7));
  s32 mask = input >> 31;
  s32 data = input ^ mask;
  if(input > -32768) data -= mask;
  if(data == 0) {
    result = 0x7fff'ffff;
  } else if(input == -32768) {
    result = 0xffff'0000;
  } else {
    u32 shift = countLeadingZeros(data);
    u32 index = (u64(data) << shift & 0x7fc0'0000) >> 22;
    result = inverseSquareRoots[index & 0x1fe | shift & 1];
    result = (0x10000 | result) << 14;
    result = result >> (31 - shift >> 1) ^ mask;
  }
  DIVDP = 0;
  DIVOUT = result >> 16;
  ACCL = vt(e);
  vd.element(de) = result;
}

template<u8 e>
auto RSP::VRSQH(r128& vd, u8 de, cr128& vt) -> void {
  ACCL  = vt(e);
  DIVDP = 1;
  DIVIN = vt.element(e & 7);
  vd.element(de) = DIVOUT;
}

template<u8 e>
auto RSP::VSAR(r128& vd, cr128& vs) -> void {
  switch(e) {
  case 0x8: vd = ACCH; break;
  case 0x9: vd = ACCM; break;
  case 0xa: vd = ACCL; break;
  default:  vd = zero; break;
  }
}

template<u8 e>
auto RSP::VSUB(r128& vd, cr128& vs, cr128& vt) -> void {
  if constexpr(Accuracy::RSP::SISD) {
    cr128 vte = vt(e);
    for(u32 n : range(8)) {
      s32 result = vs.s16(n) - vte.s16(n) - VCOL.get(n);
      ACCL.s16(n) = result;
      vd.s16(n) = sclamp<16>(result);
    }
    VCOL = zero;
    VCOH = zero;
  }

  if constexpr(Accuracy::RSP::SIMD) {
    #if ARCHITECTURE_SUPPORTS_SSE4_1
    r128 vte = vt(e), udiff, sdiff, ov;
    udiff = _mm_sub_epi16(vte, VCOL);
    sdiff = _mm_subs_epi16(vte, VCOL);
    ACCL  = _mm_sub_epi16(vs, udiff);
    ov    = _mm_cmpgt_epi16(sdiff, udiff);
    vd    = _mm_subs_epi16(vs, sdiff);
    vd    = _mm_adds_epi16(vd, ov);
    VCOL  = zero;
    VCOH  = zero;
    #endif
  }
}

template<u8 e>
auto RSP::VSUBC(r128& vd, cr128& vs, cr128& vt) -> void {
  if constexpr(Accuracy::RSP::SISD) {
    cr128 vte = vt(e);
    for(u32 n : range(8)) {
      u32 result = vs.u16(n) - vte.u16(n);
      ACCL.u16(n) = result;
      VCOL.set(n, result >> 16);
      VCOH.set(n, result != 0);
    }
    vd = ACCL;
  }

  if constexpr(Accuracy::RSP::SIMD) {
    #if ARCHITECTURE_SUPPORTS_SSE4_1
    r128 vte = vt(e), equal, udiff, diff0;
    udiff = _mm_subs_epu16(vs, vte);
    equal = _mm_cmpeq_epi16(vs, vte);
    diff0 = _mm_cmpeq_epi16(udiff, zero);
    VCOH  = _mm_cmpeq_epi16(equal, zero);
    VCOL  = _mm_andnot_si128(equal, diff0);
    ACCL  = _mm_sub_epi16(vs, vte);
    vd    = ACCL;
    #endif
  }
}

template<u8 e>
auto RSP::VXOR(r128& vd, cr128& vs, cr128& vt) -> void {
  if constexpr(Accuracy::RSP::SISD) {
    cr128 vte = vt(e);
    for(u32 n : range(8)) {
      ACCL.u16(n) = vs.u16(n) ^ vte.u16(n);
    }
    vd = ACCL;
  }

  if constexpr(Accuracy::RSP::SIMD) {
    #if ARCHITECTURE_SUPPORTS_SSE4_1
    ACCL = _mm_xor_si128(vs, vt(e));
    vd   = ACCL;
    #endif
  }
}

template<u8 e>
auto RSP::VZERO(r128& vd, cr128& vs, cr128& vt) -> void {
  if constexpr(Accuracy::RSP::SISD) {
    cr128 vte = vt(e);
    for(u32 n : range(8)) {
      s32 result = vs.s16(n) + vte.s16(n);
      ACCL.s16(n) = result;
      vd.s16(n) = 0;
    }
  }

  if constexpr(Accuracy::RSP::SIMD) {
    #if ARCHITECTURE_SUPPORTS_SSE4_1
    r128 vte = vt(e), sum, min, max;
    ACCL = _mm_add_epi16(vs, vte);
    vd   = _mm_xor_si128(vd, vd);
    #endif
  }
}

#undef ACCH
#undef ACCM
#undef ACCL
#undef VCOH
#undef VCOL
#undef VCCH
#undef VCCL
#undef VCE

#undef DIVIN
#undef DIVOUT
#undef DIVDP
