/*
$info$
tags: backend|interpreter
$end_info$
*/

#include "Interface/Core/Interpreter/InterpreterClass.h"
#include "Interface/Core/Interpreter/InterpreterOps.h"
#include "Interface/Core/Interpreter/InterpreterDefines.h"

#include <FEXCore/Utils/BitUtils.h>

#include <cstdint>

namespace FEXCore::CPU {
#define DEF_OP(x) void InterpreterOps::Op_##x(IR::IROp_Header *IROp, IROpData *Data, IR::NodeID Node)

DEF_OP(TruncElementPair) {
  auto Op = IROp->C<IR::IROp_TruncElementPair>();

  switch (IROp->Size) {
    case 4: {
      uint64_t *Src = GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[0]);
      uint64_t Result{};
      Result = Src[0] & ~0U;
      Result |= Src[1] << 32;
      GD = Result;
      break;
    }
    default: LOGMAN_MSG_A_FMT("Unhandled Truncation size: {}", IROp->Size); break;
  }
}

DEF_OP(Constant) {
  auto Op = IROp->C<IR::IROp_Constant>();
  GD = Op->Constant;
}

DEF_OP(EntrypointOffset) {
  auto Op = IROp->C<IR::IROp_EntrypointOffset>();
  uint64_t Mask = ~0ULL;
  uint8_t OpSize = IROp->Size;
  if (OpSize == 4) {
    Mask = 0xFFFF'FFFFULL;
  }

  GD = (Data->CurrentEntry + Op->Offset) & Mask;
}

DEF_OP(InlineConstant) {
  //nop
}

DEF_OP(InlineEntrypointOffset) {
  //nop
}

DEF_OP(CycleCounter) {
#ifdef DEBUG_CYCLES
  GD = 0;
#else
  timespec time;
  clock_gettime(CLOCK_REALTIME, &time);
  GD = time.tv_nsec + time.tv_sec * 1000000000;
#endif
}

#define GRS(Node) (IROp->Size <= 4 ? GetReg<RA_32>(Node) : GetReg<RA_64>(Node))

DEF_OP(Add) {
  auto Op = IROp->C<IR::IROp_Add>();
  uint8_t OpSize = IROp->Size;

  void *Src1 = GetSrc<void*>(Data->SSAData, Op->Header.Args[0]);
  void *Src2 = GetSrc<void*>(Data->SSAData, Op->Header.Args[1]);
  auto Func = [](auto a, auto b) { return a + b; };

  switch (OpSize) {
    DO_OP(4, uint32_t, Func)
    DO_OP(8, uint64_t, Func)
    default: LOGMAN_MSG_A_FMT("Unknown size: {}", OpSize); break;
  }
}

DEF_OP(Sub) {
  auto Op = IROp->C<IR::IROp_Sub>();
  uint8_t OpSize = IROp->Size;

  void *Src1 = GetSrc<void*>(Data->SSAData, Op->Header.Args[0]);
  void *Src2 = GetSrc<void*>(Data->SSAData, Op->Header.Args[1]);
  auto Func = [](auto a, auto b) { return a - b; };

  switch (OpSize) {
    DO_OP(4, uint32_t, Func)
    DO_OP(8, uint64_t, Func)
    default: LOGMAN_MSG_A_FMT("Unknown size: {}", OpSize); break;
  }
}

DEF_OP(Neg) {
  auto Op = IROp->C<IR::IROp_Neg>();
  uint8_t OpSize = IROp->Size;

  uint64_t Src = *GetSrc<int64_t*>(Data->SSAData, Op->Header.Args[0]);
  switch (OpSize) {
    case 4:
      GD = -static_cast<int32_t>(Src);
      break;
    case 8:
      GD = -static_cast<int64_t>(Src);
      break;
    default: LOGMAN_MSG_A_FMT("Unknown NEG Size: {}\n", OpSize); break;
  }
}

DEF_OP(Mul) {
  auto Op = IROp->C<IR::IROp_Mul>();
  uint8_t OpSize = IROp->Size;

  uint64_t Src1 = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[0]);
  uint64_t Src2 = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[1]);

  switch (OpSize) {
    case 4:
      GD = static_cast<int64_t>(static_cast<int32_t>(Src1)) * static_cast<int64_t>(static_cast<int32_t>(Src2));
      break;
    case 8:
      GD = static_cast<int64_t>(Src1) * static_cast<int64_t>(Src2);
      break;
    case 16: {
      __int128_t Tmp = static_cast<__int128_t>(static_cast<int64_t>(Src1)) * static_cast<__int128_t>(static_cast<int64_t>(Src2));
      memcpy(GDP, &Tmp, 16);
      break;
    }
    default: LOGMAN_MSG_A_FMT("Unknown Mul Size: {}\n", OpSize); break;
  }
}

DEF_OP(UMul) {
  auto Op = IROp->C<IR::IROp_UMul>();
  uint8_t OpSize = IROp->Size;

  uint64_t Src1 = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[0]);
  uint64_t Src2 = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[1]);

  switch (OpSize) {
    case 4:
      GD = static_cast<uint32_t>(Src1) * static_cast<uint32_t>(Src2);
      break;
    case 8:
      GD = static_cast<uint64_t>(Src1) * static_cast<uint64_t>(Src2);
      break;
    case 16: {
      __uint128_t Tmp = static_cast<__uint128_t>(static_cast<uint64_t>(Src1)) * static_cast<__uint128_t>(static_cast<uint64_t>(Src2));
      memcpy(GDP, &Tmp, 16);
      break;
    }
    default: LOGMAN_MSG_A_FMT("Unknown UMul Size: {}\n", OpSize); break;
  }
}

DEF_OP(Div) {
  auto Op = IROp->C<IR::IROp_Div>();
  uint8_t OpSize = IROp->Size;
  uint64_t Src1 = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[0]);
  uint64_t Src2 = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[1]);

  switch (OpSize) {
    case 1:
      GD = static_cast<int64_t>(static_cast<int8_t>(Src1)) / static_cast<int64_t>(static_cast<int8_t>(Src2));
      break;
    case 2:
      GD = static_cast<int64_t>(static_cast<int16_t>(Src1)) / static_cast<int64_t>(static_cast<int16_t>(Src2));
      break;
    case 4:
      GD = static_cast<int64_t>(static_cast<int32_t>(Src1)) / static_cast<int64_t>(static_cast<int32_t>(Src2));
      break;
    case 8:
      GD = static_cast<int64_t>(Src1) / static_cast<int64_t>(Src2);
      break;
    case 16: {
      __int128_t Tmp = *GetSrc<__int128_t*>(Data->SSAData, Op->Header.Args[0]) / *GetSrc<__int128_t*>(Data->SSAData, Op->Header.Args[1]);
      memcpy(GDP, &Tmp, 16);
      break;
    }
    default: LOGMAN_MSG_A_FMT("Unknown Mul Size: {}\n", OpSize); break;
  }
}

DEF_OP(UDiv) {
  auto Op = IROp->C<IR::IROp_UDiv>();
  uint8_t OpSize = IROp->Size;

  uint64_t Src1 = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[0]);
  uint64_t Src2 = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[1]);

  switch (OpSize) {
    case 1:
      GD = static_cast<uint64_t>(static_cast<uint8_t>(Src1)) / static_cast<uint64_t>(static_cast<uint8_t>(Src2));
      break;
    case 2:
      GD = static_cast<uint64_t>(static_cast<uint16_t>(Src1)) / static_cast<uint64_t>(static_cast<uint16_t>(Src2));
      break;
    case 4:
      GD = static_cast<uint64_t>(static_cast<uint32_t>(Src1)) / static_cast<uint64_t>(static_cast<uint32_t>(Src2));
      break;
    case 8:
      GD = static_cast<uint64_t>(Src1) / static_cast<uint64_t>(Src2);
      break;
    case 16: {
      __uint128_t Tmp = *GetSrc<__uint128_t*>(Data->SSAData, Op->Header.Args[0]) / *GetSrc<__uint128_t*>(Data->SSAData, Op->Header.Args[1]);
      memcpy(GDP, &Tmp, 16);
      break;
    }
    default: LOGMAN_MSG_A_FMT("Unknown Mul Size: {}\n", OpSize); break;
  }
}

DEF_OP(Rem) {
  auto Op = IROp->C<IR::IROp_Rem>();
  uint8_t OpSize = IROp->Size;

  uint64_t Src1 = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[0]);
  uint64_t Src2 = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[1]);

  switch (OpSize) {
    case 1:
      GD = static_cast<int64_t>(static_cast<int8_t>(Src1)) % static_cast<int64_t>(static_cast<int8_t>(Src2));
      break;
    case 2:
      GD = static_cast<int64_t>(static_cast<int16_t>(Src1)) % static_cast<int64_t>(static_cast<int16_t>(Src2));
      break;
    case 4:
      GD = static_cast<int64_t>(static_cast<int32_t>(Src1)) % static_cast<int64_t>(static_cast<int32_t>(Src2));
      break;
    case 8:
      GD = static_cast<int64_t>(Src1) % static_cast<int64_t>(Src2);
      break;
    case 16: {
      __int128_t Tmp = *GetSrc<__int128_t*>(Data->SSAData, Op->Header.Args[0]) % *GetSrc<__int128_t*>(Data->SSAData, Op->Header.Args[1]);
      memcpy(GDP, &Tmp, 16);
      break;
    }
    default: LOGMAN_MSG_A_FMT("Unknown Mul Size: {}\n", OpSize); break;
  }
}

DEF_OP(URem) {
  auto Op = IROp->C<IR::IROp_URem>();
  uint8_t OpSize = IROp->Size;

  uint64_t Src1 = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[0]);
  uint64_t Src2 = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[1]);

  switch (OpSize) {
    case 1:
      GD = static_cast<uint64_t>(static_cast<uint8_t>(Src1)) % static_cast<uint64_t>(static_cast<uint8_t>(Src2));
      break;
    case 2:
      GD = static_cast<uint64_t>(static_cast<uint16_t>(Src1)) % static_cast<uint64_t>(static_cast<uint16_t>(Src2));
      break;
    case 4:
      GD = static_cast<uint64_t>(static_cast<uint32_t>(Src1)) % static_cast<uint64_t>(static_cast<uint32_t>(Src2));
      break;
    case 8:
      GD = static_cast<uint64_t>(Src1) % static_cast<uint64_t>(Src2);
      break;
    case 16: {
      __uint128_t Tmp = *GetSrc<__uint128_t*>(Data->SSAData, Op->Header.Args[0]) % *GetSrc<__uint128_t*>(Data->SSAData, Op->Header.Args[1]);
      memcpy(GDP, &Tmp, 16);
      break;
    }
    default: LOGMAN_MSG_A_FMT("Unknown Mul Size: {}\n", OpSize); break;
  }
}

DEF_OP(MulH) {
  auto Op = IROp->C<IR::IROp_MulH>();
  uint8_t OpSize = IROp->Size;

  uint64_t Src1 = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[0]);
  uint64_t Src2 = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[1]);

  switch (OpSize) {
    case 4: {
      int64_t Tmp = static_cast<int64_t>(static_cast<int32_t>(Src1)) * static_cast<int64_t>(static_cast<int32_t>(Src2));
      GD = Tmp >> 32;
      break;
    }
    case 8: {
      __int128_t Tmp = static_cast<__int128_t>(static_cast<int64_t>(Src1)) * static_cast<__int128_t>(static_cast<int64_t>(Src2));
      GD = Tmp >> 64;
      break;
    }
    default: LOGMAN_MSG_A_FMT("Unknown MulH Size: {}\n", OpSize); break;
  }
}

DEF_OP(UMulH) {
  auto Op = IROp->C<IR::IROp_UMulH>();
  uint8_t OpSize = IROp->Size;

  uint64_t Src1 = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[0]);
  uint64_t Src2 = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[1]);
  switch (OpSize) {
    case 4:
      GD = static_cast<uint64_t>(Src1) * static_cast<uint64_t>(Src2);
      GD >>= 32;
      break;
    case 8: {
      __uint128_t Tmp = static_cast<__uint128_t>(Src1) * static_cast<__uint128_t>(Src2);
      GD = Tmp >> 64;
      break;
    }
    case 16: {
      // XXX: This is incorrect
      __uint128_t Tmp = static_cast<__uint128_t>(Src1) * static_cast<__uint128_t>(Src2);
      GD = Tmp >> 64;
      break;
    }
    default: LOGMAN_MSG_A_FMT("Unknown UMulH Size: {}\n", OpSize); break;
  }
}

DEF_OP(Or) {
  auto Op = IROp->C<IR::IROp_Or>();
  uint8_t OpSize = IROp->Size;

  void *Src1 = GetSrc<void*>(Data->SSAData, Op->Header.Args[0]);
  void *Src2 = GetSrc<void*>(Data->SSAData, Op->Header.Args[1]);
  auto Func = [](auto a, auto b) { return a | b; };

  switch (OpSize) {
    DO_OP(1, uint8_t,  Func)
    DO_OP(2, uint16_t, Func)
    DO_OP(4, uint32_t, Func)
    DO_OP(8, uint64_t, Func)
    DO_OP(16, __uint128_t, Func)
    default: LOGMAN_MSG_A_FMT("Unknown size: {}", OpSize); break;
  }
}

DEF_OP(And) {
  auto Op = IROp->C<IR::IROp_And>();
  uint8_t OpSize = IROp->Size;

  void *Src1 = GetSrc<void*>(Data->SSAData, Op->Header.Args[0]);
  void *Src2 = GetSrc<void*>(Data->SSAData, Op->Header.Args[1]);
  auto Func = [](auto a, auto b) { return a & b; };

  switch (OpSize) {
    DO_OP(1, uint8_t,  Func)
    DO_OP(2, uint16_t, Func)
    DO_OP(4, uint32_t, Func)
    DO_OP(8, uint64_t, Func)
    default: LOGMAN_MSG_A_FMT("Unknown size: {}", OpSize); break;
  }
}

DEF_OP(Andn) {
  auto Op = IROp->C<IR::IROp_Andn>();
  const uint8_t OpSize = IROp->Size;

  void *Src1 = GetSrc<void*>(Data->SSAData, Op->Header.Args[0]);
  void *Src2 = GetSrc<void*>(Data->SSAData, Op->Header.Args[1]);
  constexpr auto Func = [](auto a, auto b) {
    using Type = decltype(a);
    return static_cast<Type>(a & static_cast<Type>(~b));
  };

  switch (OpSize) {
    DO_OP(1, uint8_t,  Func)
    DO_OP(2, uint16_t, Func)
    DO_OP(4, uint32_t, Func)
    DO_OP(8, uint64_t, Func)
    default: LOGMAN_MSG_A_FMT("Unknown size: {}", OpSize); break;
  }
}

DEF_OP(Xor) {
  auto Op = IROp->C<IR::IROp_Xor>();
  uint8_t OpSize = IROp->Size;

  void *Src1 = GetSrc<void*>(Data->SSAData, Op->Header.Args[0]);
  void *Src2 = GetSrc<void*>(Data->SSAData, Op->Header.Args[1]);
  auto Func = [](auto a, auto b) { return a ^ b; };

  switch (OpSize) {
    DO_OP(1, uint8_t,  Func)
    DO_OP(2, uint16_t, Func)
    DO_OP(4, uint32_t, Func)
    DO_OP(8, uint64_t, Func)
    default: LOGMAN_MSG_A_FMT("Unknown size: {}", OpSize); break;
  }
}

DEF_OP(Lshl) {
  auto Op = IROp->C<IR::IROp_Lshl>();
  uint8_t OpSize = IROp->Size;

  uint64_t Src1 = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[0]);
  uint64_t Src2 = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[1]);
  uint8_t Mask = OpSize * 8 - 1;
  switch (OpSize) {
    case 4:
      GD = static_cast<uint32_t>(Src1) << (Src2 & Mask);
      break;
    case 8:
      GD = static_cast<uint64_t>(Src1) << (Src2 & Mask);
      break;
    default: LOGMAN_MSG_A_FMT("Unknown LSHL Size: {}\n", OpSize); break;
  }
}

DEF_OP(Lshr) {
  auto Op = IROp->C<IR::IROp_Lshr>();
  uint8_t OpSize = IROp->Size;

  uint64_t Src1 = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[0]);
  uint64_t Src2 = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[1]);
  uint8_t Mask = OpSize * 8 - 1;
  switch (OpSize) {
    case 4:
      GD = static_cast<uint32_t>(Src1) >> (Src2 & Mask);
      break;
    case 8:
      GD = static_cast<uint64_t>(Src1) >> (Src2 & Mask);
      break;
    default: LOGMAN_MSG_A_FMT("Unknown LSHR Size: {}\n", OpSize); break;
  }
}

DEF_OP(Ashr) {
  auto Op = IROp->C<IR::IROp_Ashr>();
  uint8_t OpSize = IROp->Size;

  uint64_t Src1 = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[0]);
  uint64_t Src2 = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[1]);
  uint8_t Mask = OpSize * 8 - 1;
  switch (OpSize) {
    case 4:
      GD = (uint32_t)(static_cast<int32_t>(Src1) >> (Src2 & Mask));
      break;
    case 8:
      GD = (uint64_t)(static_cast<int64_t>(Src1) >> (Src2 & Mask));
      break;
    default: LOGMAN_MSG_A_FMT("Unknown ASHR Size: {}\n", OpSize); break;
  }
}

DEF_OP(Ror) {
  auto Op = IROp->C<IR::IROp_Ror>();
  uint8_t OpSize = IROp->Size;

  uint64_t Src1 = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[0]);
  uint64_t Src2 = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[1]);
  auto Ror = [] (auto In, auto R) {
    auto RotateMask = sizeof(In) * 8 - 1;
    R &= RotateMask;
    return (In >> R) | (In << (sizeof(In) * 8 - R));
  };

  switch (OpSize) {
    case 4:
      GD = Ror(static_cast<uint32_t>(Src1), static_cast<uint32_t>(Src2));
      break;
    case 8: {
      GD = Ror(static_cast<uint64_t>(Src1), static_cast<uint64_t>(Src2));
      break;
    }
    default: LOGMAN_MSG_A_FMT("Unknown ROR Size: {}\n", OpSize); break;
  }
}

DEF_OP(Extr) {
  auto Op = IROp->C<IR::IROp_Extr>();
  uint8_t OpSize = IROp->Size;

  uint64_t Src1 = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[0]);
  uint64_t Src2 = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[1]);
  auto Extr = [] (auto Src1, auto Src2, uint8_t lsb) -> decltype(Src1) {
    __uint128_t Result{};
    Result = Src1;
    Result <<= sizeof(Src1) * 8;
    Result |= Src2;
    Result >>= lsb;
    return Result;
  };

  switch (OpSize) {
    case 4:
      GD = Extr(static_cast<uint32_t>(Src1), static_cast<uint32_t>(Src2), Op->LSB);
      break;
    case 8: {
      GD = Extr(static_cast<uint64_t>(Src1), static_cast<uint64_t>(Src2), Op->LSB);
      break;
    }
    default: LOGMAN_MSG_A_FMT("Unknown EXTR Size: {}\n", OpSize); break;
  }
}

DEF_OP(PDep) {
  const auto Op = IROp->C<IR::IROp_PExt>();
  const auto OpSize = IROp->Size;

  if (OpSize != 4 && OpSize != 8) {
    LOGMAN_MSG_A_FMT("Unknown PDep Size: {}\n", OpSize);
    return;
  }

  const uint64_t Input = OpSize == 4 ? *GetSrc<uint32_t*>(Data->SSAData, Op->Args(0))
                                     : *GetSrc<uint64_t*>(Data->SSAData, Op->Args(0));
  uint64_t Mask = OpSize == 4 ? *GetSrc<uint32_t*>(Data->SSAData, Op->Args(1))
                              : *GetSrc<uint64_t*>(Data->SSAData, Op->Args(1));

  uint64_t Result = 0;
  for (uint64_t Index = 0; Mask > 0; Index++) {
    const uint64_t Offset = std::countr_zero(Mask);
    Mask &= Mask - 1;
    Result |= ((Input >> Index) & 1) << Offset;
  }

  GD = Result;
}

DEF_OP(PExt) {
  const auto Op = IROp->C<IR::IROp_PExt>();
  const auto OpSize = IROp->Size;

  if (OpSize != 4 && OpSize != 8) {
    LOGMAN_MSG_A_FMT("Unknown PExt Size: {}\n", OpSize);
    return;
  }

  const uint64_t Input = OpSize == 4 ? *GetSrc<uint32_t*>(Data->SSAData, Op->Args(0))
                                     : *GetSrc<uint64_t*>(Data->SSAData, Op->Args(0));
  uint64_t Mask = OpSize == 4 ? *GetSrc<uint32_t*>(Data->SSAData, Op->Args(1))
                              : *GetSrc<uint64_t*>(Data->SSAData, Op->Args(1));

  uint64_t Result = 0;
  for (uint64_t Offset = 0; Mask > 0; Offset++) {
    const uint64_t Index = std::countr_zero(Mask);
    Mask &= Mask - 1;
    Result |= ((Input >> Index) & 1) << Offset;
  }

  GD = Result;
}

DEF_OP(LDiv) {
  auto Op = IROp->C<IR::IROp_LDiv>();
  uint8_t OpSize = IROp->Size;

  // Each source is OpSize in size
  // So you can have up to a 128bit divide from x86-64
  switch (OpSize) {
    case 2: {
      uint16_t SrcLow = *GetSrc<uint16_t*>(Data->SSAData, Op->Header.Args[0]);
      uint16_t SrcHigh = *GetSrc<uint16_t*>(Data->SSAData, Op->Header.Args[1]);
      int16_t Divisor = *GetSrc<uint16_t*>(Data->SSAData, Op->Header.Args[2]);
      int32_t Source = (static_cast<uint32_t>(SrcHigh) << 16) | SrcLow;
      int32_t Res = Source / Divisor;

      // We only store the lower bits of the result
      GD = static_cast<int16_t>(Res);
      break;
    }
    case 4: {
      uint32_t SrcLow = *GetSrc<uint32_t*>(Data->SSAData, Op->Header.Args[0]);
      uint32_t SrcHigh = *GetSrc<uint32_t*>(Data->SSAData, Op->Header.Args[1]);
      int32_t Divisor = *GetSrc<uint32_t*>(Data->SSAData, Op->Header.Args[2]);
      int64_t Source = (static_cast<uint64_t>(SrcHigh) << 32) | SrcLow;
      int64_t Res = Source / Divisor;

      // We only store the lower bits of the result
      GD = static_cast<int32_t>(Res);
      break;
    }
    case 8: {
      uint64_t SrcLow = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[0]);
      uint64_t SrcHigh = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[1]);
      int64_t Divisor = *GetSrc<int64_t*>(Data->SSAData, Op->Header.Args[2]);
      __int128_t Source = (static_cast<__int128_t>(SrcHigh) << 64) | SrcLow;
      __int128_t Res = Source / Divisor;

      // We only store the lower bits of the result
      memcpy(GDP, &Res, OpSize);
      break;
    }
    default: LOGMAN_MSG_A_FMT("Unknown LDIV Size: {}", OpSize); break;
  }
}

DEF_OP(LUDiv) {
  auto Op = IROp->C<IR::IROp_LUDiv>();
  uint8_t OpSize = IROp->Size;

  // Each source is OpSize in size
  // So you can have up to a 128bit divide from x86-64
  switch (OpSize) {
    case 2: {
      uint16_t SrcLow = *GetSrc<uint16_t*>(Data->SSAData, Op->Header.Args[0]);
      uint16_t SrcHigh = *GetSrc<uint16_t*>(Data->SSAData, Op->Header.Args[1]);
      uint16_t Divisor = *GetSrc<uint16_t*>(Data->SSAData, Op->Header.Args[2]);
      uint32_t Source = (static_cast<uint32_t>(SrcHigh) << 16) | SrcLow;
      uint32_t Res = Source / Divisor;

      // We only store the lower bits of the result
      GD = static_cast<uint16_t>(Res);
      break;
    }
    case 4: {
      uint32_t SrcLow = *GetSrc<uint32_t*>(Data->SSAData, Op->Header.Args[0]);
      uint32_t SrcHigh = *GetSrc<uint32_t*>(Data->SSAData, Op->Header.Args[1]);
      uint32_t Divisor = *GetSrc<uint32_t*>(Data->SSAData, Op->Header.Args[2]);
      uint64_t Source = (static_cast<uint64_t>(SrcHigh) << 32) | SrcLow;
      uint64_t Res = Source / Divisor;

      // We only store the lower bits of the result
      GD = static_cast<uint32_t>(Res);
      break;
    }
    case 8: {
      uint64_t SrcLow = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[0]);
      uint64_t SrcHigh = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[1]);
      uint64_t Divisor = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[2]);
      __uint128_t Source = (static_cast<__uint128_t>(SrcHigh) << 64) | SrcLow;
      __uint128_t Res = Source / Divisor;

      // We only store the lower bits of the result
      memcpy(GDP, &Res, OpSize);
      break;
    }
    default: LOGMAN_MSG_A_FMT("Unknown LUDIV Size: {}", OpSize); break;
  }
}

DEF_OP(LRem) {
  auto Op = IROp->C<IR::IROp_LRem>();
  uint8_t OpSize = IROp->Size;

  // Each source is OpSize in size
  // So you can have up to a 128bit Remainder from x86-64
  switch (OpSize) {
    case 2: {
      uint16_t SrcLow = *GetSrc<uint16_t*>(Data->SSAData, Op->Header.Args[0]);
      uint16_t SrcHigh = *GetSrc<uint16_t*>(Data->SSAData, Op->Header.Args[1]);
      int16_t Divisor = *GetSrc<uint16_t*>(Data->SSAData, Op->Header.Args[2]);
      int32_t Source = (static_cast<uint32_t>(SrcHigh) << 16) | SrcLow;
      int32_t Res = Source % Divisor;

      // We only store the lower bits of the result
      GD = static_cast<int16_t>(Res);
      break;
    }
    case 4: {
      uint32_t SrcLow = *GetSrc<uint32_t*>(Data->SSAData, Op->Header.Args[0]);
      uint32_t SrcHigh = *GetSrc<uint32_t*>(Data->SSAData, Op->Header.Args[1]);
      int32_t Divisor = *GetSrc<uint32_t*>(Data->SSAData, Op->Header.Args[2]);
      int64_t Source = (static_cast<uint64_t>(SrcHigh) << 32) | SrcLow;
      int64_t Res = Source % Divisor;

      // We only store the lower bits of the result
      GD = static_cast<int32_t>(Res);
      break;
    }
    case 8: {
      uint64_t SrcLow = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[0]);
      uint64_t SrcHigh = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[1]);
      int64_t Divisor = *GetSrc<int64_t*>(Data->SSAData, Op->Header.Args[2]);
      __int128_t Source = (static_cast<__int128_t>(SrcHigh) << 64) | SrcLow;
      __int128_t Res = Source % Divisor;
      // We only store the lower bits of the result
      memcpy(GDP, &Res, OpSize);
      break;
    }
    default: LOGMAN_MSG_A_FMT("Unknown LREM Size: {}", OpSize); break;
  }
}

DEF_OP(LURem) {
  auto Op = IROp->C<IR::IROp_LURem>();
  uint8_t OpSize = IROp->Size;

  // Each source is OpSize in size
  // So you can have up to a 128bit Remainder from x86-64
  switch (OpSize) {
    case 2: {
      uint16_t SrcLow = *GetSrc<uint16_t*>(Data->SSAData, Op->Header.Args[0]);
      uint16_t SrcHigh = *GetSrc<uint16_t*>(Data->SSAData, Op->Header.Args[1]);
      uint16_t Divisor = *GetSrc<uint16_t*>(Data->SSAData, Op->Header.Args[2]);
      uint32_t Source = (static_cast<uint32_t>(SrcHigh) << 16) | SrcLow;
      uint32_t Res = Source % Divisor;

      // We only store the lower bits of the result
      GD = static_cast<uint16_t>(Res);
      break;
    }
    case 4: {
      uint32_t SrcLow = *GetSrc<uint32_t*>(Data->SSAData, Op->Header.Args[0]);
      uint32_t SrcHigh = *GetSrc<uint32_t*>(Data->SSAData, Op->Header.Args[1]);
      uint32_t Divisor = *GetSrc<uint32_t*>(Data->SSAData, Op->Header.Args[2]);
      uint64_t Source = (static_cast<uint64_t>(SrcHigh) << 32) | SrcLow;
      uint64_t Res = Source % Divisor;

      // We only store the lower bits of the result
      GD = static_cast<uint32_t>(Res);
      break;
    }
    case 8: {
      uint64_t SrcLow = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[0]);
      uint64_t SrcHigh = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[1]);
      uint64_t Divisor = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[2]);
      __uint128_t Source = (static_cast<__uint128_t>(SrcHigh) << 64) | SrcLow;
      __uint128_t Res = Source % Divisor;
      // We only store the lower bits of the result
      memcpy(GDP, &Res, OpSize);
      break;
    }
    default: LOGMAN_MSG_A_FMT("Unknown LUREM Size: {}", OpSize); break;
  }
}

DEF_OP(Not) {
  auto Op = IROp->C<IR::IROp_Not>();
  uint8_t OpSize = IROp->Size;

  uint64_t Src = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[0]);
  const uint64_t mask[9]= { 0, 0xFF, 0xFFFF, 0, 0xFFFFFFFF, 0, 0, 0, 0xFFFFFFFFFFFFFFFFULL };
  uint64_t Mask = mask[OpSize];
  GD = (~Src) & Mask;
}

DEF_OP(Popcount) {
  auto Op = IROp->C<IR::IROp_Popcount>();
  uint64_t Src = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[0]);
  GD = std::popcount(Src);
}

DEF_OP(FindLSB) {
  auto Op = IROp->C<IR::IROp_FindLSB>();
  uint64_t Src = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[0]);
  uint64_t Result = FindFirstSetBit(Src);
  GD = Result - 1;
}

DEF_OP(FindMSB) {
  auto Op = IROp->C<IR::IROp_FindMSB>();
  uint8_t OpSize = IROp->Size;

  switch (OpSize) {
    case 1: GD = (OpSize * 8 - std::countl_zero(*GetSrc<uint8_t*>(Data->SSAData, Op->Header.Args[0]))) - 1; break;
    case 2: GD = (OpSize * 8 - std::countl_zero(*GetSrc<uint16_t*>(Data->SSAData, Op->Header.Args[0]))) - 1; break;
    case 4: GD = (OpSize * 8 - std::countl_zero(*GetSrc<uint32_t*>(Data->SSAData, Op->Header.Args[0]))) - 1; break;
    case 8: GD = (OpSize * 8 - std::countl_zero(*GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[0]))) - 1; break;
    default: LOGMAN_MSG_A_FMT("Unknown FindMSB size: {}", OpSize); break;
  }
}

DEF_OP(FindTrailingZeros) {
  auto Op = IROp->C<IR::IROp_FindTrailingZeros>();
  uint8_t OpSize = IROp->Size;

  switch (OpSize) {
    case 1: {
      auto Src = *GetSrc<uint8_t*>(Data->SSAData, Op->Header.Args[0]);
      GD = std::countr_zero(Src);
      break;
    }
    case 2: {
      auto Src = *GetSrc<uint16_t*>(Data->SSAData, Op->Header.Args[0]);
      GD = std::countr_zero(Src);
      break;
    }
    case 4: {
      auto Src = *GetSrc<uint32_t*>(Data->SSAData, Op->Header.Args[0]);
      GD = std::countr_zero(Src);
      break;
    }
    case 8: {
      auto Src = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[0]);
      GD = std::countr_zero(Src);
      break;
    }
    default: LOGMAN_MSG_A_FMT("Unknown size: {}", OpSize); break;
  }
}

DEF_OP(CountLeadingZeroes) {
  auto Op = IROp->C<IR::IROp_CountLeadingZeroes>();
  uint8_t OpSize = IROp->Size;

  switch (OpSize) {
    case 1: {
      auto Src = *GetSrc<uint8_t*>(Data->SSAData, Op->Header.Args[0]);
      GD = std::countl_zero(Src);
      break;
    }
    case 2: {
      auto Src = *GetSrc<uint16_t*>(Data->SSAData, Op->Header.Args[0]);
      GD = std::countl_zero(Src);
      break;
    }
    case 4: {
      auto Src = *GetSrc<uint32_t*>(Data->SSAData, Op->Header.Args[0]);
      GD = std::countl_zero(Src);
      break;
    }
    case 8: {
      auto Src = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[0]);
      GD = std::countl_zero(Src);
      break;
    }
    default: LOGMAN_MSG_A_FMT("Unknown size: {}", OpSize); break;
  }
}

DEF_OP(Rev) {
  auto Op = IROp->C<IR::IROp_Rev>();
  uint8_t OpSize = IROp->Size;

  switch (OpSize) {
    case 2: GD = BSwap16(*GetSrc<uint16_t*>(Data->SSAData, Op->Header.Args[0])); break;
    case 4: GD = BSwap32(*GetSrc<uint32_t*>(Data->SSAData, Op->Header.Args[0])); break;
    case 8: GD = BSwap64(*GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[0])); break;
    default: LOGMAN_MSG_A_FMT("Unknown REV size: {}", OpSize); break;
  }
}

DEF_OP(Bfi) {
  auto Op = IROp->C<IR::IROp_Bfi>();
  uint64_t SourceMask = (1ULL << Op->Width) - 1;
  if (Op->Width == 64)
    SourceMask = ~0ULL;
  uint64_t DestMask = ~(SourceMask << Op->lsb);
  uint64_t Src1 = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[0]);
  uint64_t Src2 = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[1]);
  uint64_t Res = (Src1 & DestMask) | ((Src2 & SourceMask) << Op->lsb);
  GD = Res;
}

DEF_OP(Bfe) {
  auto Op = IROp->C<IR::IROp_Bfe>();

  LOGMAN_THROW_A_FMT(IROp->Size <= 8, "OpSize is too large for BFE: {}", IROp->Size);
  uint64_t SourceMask = (1ULL << Op->Width) - 1;
  if (Op->Width == 64)
    SourceMask = ~0ULL;
  SourceMask <<= Op->lsb;
  uint64_t Src = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[0]);
  GD = (Src & SourceMask) >> Op->lsb;
}

DEF_OP(Sbfe) {
  auto Op = IROp->C<IR::IROp_Sbfe>();

  LOGMAN_THROW_A_FMT(IROp->Size <= 8, "OpSize is too large for SBFE: {}", IROp->Size);
  int64_t Src = *GetSrc<int64_t*>(Data->SSAData, Op->Header.Args[0]);
  uint64_t ShiftLeftAmount = (64 - (Op->Width + Op->lsb));
  uint64_t ShiftRightAmount = ShiftLeftAmount + Op->lsb;
  Src <<= ShiftLeftAmount;
  Src >>= ShiftRightAmount;
  GD = Src;
}

DEF_OP(Select) {
  auto Op = IROp->C<IR::IROp_Select>();
  uint8_t OpSize = IROp->Size;

  uint64_t Src1 = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[0]);
  uint64_t Src2 = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[1]);

  uint64_t ArgTrue;
  uint64_t ArgFalse;

  if (OpSize == 4) {
    ArgTrue = *GetSrc<uint32_t*>(Data->SSAData, Op->Header.Args[2]);
    ArgFalse = *GetSrc<uint32_t*>(Data->SSAData, Op->Header.Args[3]);
  } else {
    ArgTrue = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[2]);
    ArgFalse = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[3]);
  }

  bool CompResult;

  if (Op->CompareSize == 4)
    CompResult = IsConditionTrue<uint32_t, int32_t, float>(Op->Cond.Val, Src1, Src2);
  else
    CompResult = IsConditionTrue<uint64_t, int64_t, double>(Op->Cond.Val, Src1, Src2);

  GD = CompResult ? ArgTrue : ArgFalse;
}

DEF_OP(VExtractToGPR) {
  auto Op = IROp->C<IR::IROp_VExtractToGPR>();

  uint32_t SourceSize = GetOpSize(Data->CurrentIR, Op->Header.Args[0]);

  LOGMAN_THROW_A_FMT(IROp->Size <= 16, "OpSize is too large for VExtractToGPR: {}", IROp->Size);

  if (SourceSize == 16) {
    __uint128_t SourceMask = (1ULL << (Op->Header.ElementSize * 8)) - 1;
    uint64_t Shift = Op->Header.ElementSize * Op->Index * 8;
    if (Op->Header.ElementSize == 8)
      SourceMask = ~0ULL;

    __uint128_t Src = *GetSrc<__uint128_t*>(Data->SSAData, Op->Header.Args[0]);
    Src >>= Shift;
    Src &= SourceMask;
    memcpy(GDP, &Src, Op->Header.ElementSize);
  }
  else {
    uint64_t SourceMask = (1ULL << (Op->Header.ElementSize * 8)) - 1;
    uint64_t Shift = Op->Header.ElementSize * Op->Index * 8;
    if (Op->Header.ElementSize == 8)
      SourceMask = ~0ULL;

    uint64_t Src = *GetSrc<uint64_t*>(Data->SSAData, Op->Header.Args[0]);
    Src >>= Shift;
    Src &= SourceMask;
    GD = Src;
  }
}

DEF_OP(Float_ToGPR_ZS) {
  auto Op = IROp->C<IR::IROp_Float_ToGPR_ZS>();
  uint16_t Conv = (IROp->Size << 8) | Op->SrcElementSize;
  switch (Conv) {
    case 0x0804: { // int64_t <- float
      int64_t Dst = (int64_t)std::trunc(*GetSrc<float*>(Data->SSAData, Op->Header.Args[0]));
      memcpy(GDP, &Dst, IROp->Size);
      break;
    }
    case 0x0808: { // int64_t <- double
      int64_t Dst = (int64_t)std::trunc(*GetSrc<double*>(Data->SSAData, Op->Header.Args[0]));
      memcpy(GDP, &Dst, IROp->Size);
      break;
    }
    case 0x0404: { // int32_t <- float
      int32_t Dst = (int32_t)std::trunc(*GetSrc<float*>(Data->SSAData, Op->Header.Args[0]));
      memcpy(GDP, &Dst, IROp->Size);
      break;
    }
    case 0x0408: { // int32_t <- double
      int32_t Dst = (int32_t)std::trunc(*GetSrc<double*>(Data->SSAData, Op->Header.Args[0]));
      memcpy(GDP, &Dst, IROp->Size);
      break;
    }
  }
}

DEF_OP(Float_ToGPR_S) {
  auto Op = IROp->C<IR::IROp_Float_ToGPR_S>();
  uint16_t Conv = (IROp->Size << 8) | Op->SrcElementSize;
  switch (Conv) {
    case 0x0804: { // int64_t <- float
      int64_t Dst = (int64_t)std::nearbyint(*GetSrc<float*>(Data->SSAData, Op->Header.Args[0]));
      memcpy(GDP, &Dst, IROp->Size);
      break;
    }
    case 0x0808: { // int64_t <- double
      int64_t Dst = (int64_t)std::nearbyint(*GetSrc<double*>(Data->SSAData, Op->Header.Args[0]));
      memcpy(GDP, &Dst, IROp->Size);
      break;
    }
    case 0x0404: { // int32_t <- float
      int32_t Dst = (int32_t)std::nearbyint(*GetSrc<float*>(Data->SSAData, Op->Header.Args[0]));
      memcpy(GDP, &Dst, IROp->Size);
      break;
    }
    case 0x0408: { // int32_t <- double
      int32_t Dst = (int32_t)std::nearbyint(*GetSrc<double*>(Data->SSAData, Op->Header.Args[0]));
      memcpy(GDP, &Dst, IROp->Size);
      break;
    }
  }
}

DEF_OP(FCmp) {
  auto Op = IROp->C<IR::IROp_FCmp>();
  uint32_t ResultFlags{};
  if (Op->ElementSize == 4) {
    float Src1 = *GetSrc<float*>(Data->SSAData, Op->Header.Args[0]);
    float Src2 = *GetSrc<float*>(Data->SSAData, Op->Header.Args[1]);
    bool Unordered = std::isnan(Src1) || std::isnan(Src2);
    if (Op->Flags & (1 << IR::FCMP_FLAG_LT)) {
      if (Unordered || (Src1 < Src2)) {
        ResultFlags |= (1 << IR::FCMP_FLAG_LT);
      }
    }
    if (Op->Flags & (1 << IR::FCMP_FLAG_UNORDERED)) {
      if (Unordered) {
        ResultFlags |= (1 << IR::FCMP_FLAG_UNORDERED);
      }
    }
    if (Op->Flags & (1 << IR::FCMP_FLAG_EQ)) {
      if (Unordered || (Src1 == Src2)) {
        ResultFlags |= (1 << IR::FCMP_FLAG_EQ);
      }
    }
  }
  else {
    double Src1 = *GetSrc<double*>(Data->SSAData, Op->Header.Args[0]);
    double Src2 = *GetSrc<double*>(Data->SSAData, Op->Header.Args[1]);
    bool Unordered = std::isnan(Src1) || std::isnan(Src2);
    if (Op->Flags & (1 << IR::FCMP_FLAG_LT)) {
      if (Unordered || (Src1 < Src2)) {
        ResultFlags |= (1 << IR::FCMP_FLAG_LT);
      }
    }
    if (Op->Flags & (1 << IR::FCMP_FLAG_UNORDERED)) {
      if (Unordered) {
        ResultFlags |= (1 << IR::FCMP_FLAG_UNORDERED);
      }
    }
    if (Op->Flags & (1 << IR::FCMP_FLAG_EQ)) {
      if (Unordered || (Src1 == Src2)) {
        ResultFlags |= (1 << IR::FCMP_FLAG_EQ);
      }
    }
  }

  GD = ResultFlags;
}

#undef DEF_OP

} // namespace FEXCore::CPU
