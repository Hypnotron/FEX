#pragma once

#include <FEXCore/Utils/CompilerDefs.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <signal.h>

namespace FEXCore {
  namespace x86_64 {
    // uc_flags flags
    ///< Has extended FP state
    constexpr uint64_t UC_FP_XSTATE         = (1ULL << 0);
    ///< Set when kernel saves SS register from 64bit code
    constexpr uint64_t UC_SIGCONTEXT_SS     = (1ULL << 1);
    ///< Set when kernel will strictly restore the SS
    constexpr uint64_t UC_STRICT_RESTORE_SS = (1ULL << 2);

    ///< Describes the signal stack
    struct FEX_PACKED stack_t {
      void *ss_sp;
      int32_t ss_flags;
      uint32_t : 32;
      size_t ss_size;
    };
    static_assert(sizeof(FEXCore::x86_64::stack_t) == 24, "This needs to be the right size");

    struct FEX_PACKED _libc_fpstate {
      // This is in FXSAVE format
      uint16_t fcw;
      uint16_t fsw;
      uint16_t ftw;
      uint16_t fop;
      uint64_t fip;
      uint64_t fdp;
      uint32_t mxcsr;
      uint32_t mxcsr_mask;
      __uint128_t _st[8];
      __uint128_t _xmm[16];
      uint32_t _res[24];
    };
    static_assert(sizeof(FEXCore::x86_64::_libc_fpstate) == 512, "This needs to be the right size");

    ///< The order of these must match the GNU ordering
    enum ContextRegs {
      FEX_REG_R8 = 0,
      FEX_REG_R9,
      FEX_REG_R10,
      FEX_REG_R11,
      FEX_REG_R12,
      FEX_REG_R13,
      FEX_REG_R14,
      FEX_REG_R15,
      FEX_REG_RDI,
      FEX_REG_RSI,
      FEX_REG_RBP,
      FEX_REG_RBX,
      FEX_REG_RDX,
      FEX_REG_RAX,
      FEX_REG_RCX,
      FEX_REG_RSP,
      FEX_REG_RIP,
      FEX_REG_EFL,
      FEX_REG_CSGSFS,
      FEX_REG_ERR,
      FEX_REG_TRAPNO,
      FEX_REG_OLDMASK,
      FEX_REG_CR2,
    };
    static_assert(FEX_REG_CR2 == 22, "Oops");

    struct FEX_PACKED mcontext_t {
      uint64_t gregs[23];
      FEXCore::x86_64::_libc_fpstate *fpregs;
      uint64_t __reserved[8];
    };
    static_assert(sizeof(FEXCore::x86_64::mcontext_t) == 256, "This needs to be the right size");

    struct FEX_PACKED sigset_t {
      uint64_t val[16];
    };
    static_assert(sizeof(FEXCore::x86_64::sigset_t) == 128, "This needs to be the right size");

    struct FEX_PACKED ucontext_t {
      uint64_t uc_flags;
      FEXCore::x86_64::ucontext_t *uc_link;
      FEXCore::x86_64::stack_t uc_stack;
      FEXCore::x86_64::mcontext_t uc_mcontext;
      FEXCore::x86_64::sigset_t uc_sigmask;
    };
    static_assert(offsetof(FEXCore::x86_64::ucontext_t, uc_mcontext) == 40, "Needs to be correct");

    static_assert(sizeof(FEXCore::x86_64::ucontext_t) == 424, "This needs to be the right size");
  }

  namespace x86 {
    // uc_flags flags
    ///< Has extended FP state
    constexpr uint64_t UC_FP_XSTATE         = (1ULL << 0);

    ///< The order of these must match the GNU ordering
    enum ContextRegs {
      FEX_REG_GS = 0,
      FEX_REG_FS,
      FEX_REG_ES,
      FEX_REG_DS,
      FEX_REG_RDI,
      FEX_REG_RSI,
      FEX_REG_RBP,
      FEX_REG_RSP,
      FEX_REG_RBX,
      FEX_REG_RDX,
      FEX_REG_RCX,
      FEX_REG_RAX,
      FEX_REG_TRAPNO,
      FEX_REG_ERR,
      FEX_REG_EIP,
      FEX_REG_CS,
      FEX_REG_EFL,
      FEX_REG_UESP,
      FEX_REG_SS
    };
    static_assert(FEX_REG_SS == 18, "Oops");

    union sigval_t {
      int sival_int;
      uint32_t sival_ptr; // XXX: Should be compat_ptr<void>
    };

    struct FEX_PACKED siginfo_t {
      int si_signo;
      int si_errno;
      int si_code;
      union {
        uint32_t pad[29];
        /* SIGILL, SIGFPE, SIGSEGV, SIBUS */
        struct {
          uint32_t addr;
        } _sigfault;
        /* SIGCHLD */
        struct {
          int32_t pid;
          int32_t uid;
          int32_t status;
          int32_t utime;
          int32_t stime;
        } _sigchld;
        /* SIGALRM, SIGVTALRM */
        struct {
          int tid;
          int overrun;
          FEXCore::x86::sigval_t sigval;
        } _timer;
      } _sifields;

      union HostSigInfo_t {
        // This anonymous struct needs to match the host definition
        struct {
          uint32_t si_signo;
          uint32_t si_errno;
          uint32_t si_code;

          uint32_t __pad0;

          // Pad[28] is a union for all the sifields
          uint32_t _pad[28];
        } FEXDef;
        ::siginfo_t host{};
      };
      static_assert(sizeof(HostSigInfo_t) == 128, "This needs to be the right size");

      siginfo_t() = delete;

      operator ::siginfo_t() const {
        // The definition of siginfo_t changes depending on the host environment
        // It is guaranteed to be 128 bytes and the kernel interface is the same for all of them
        // Since we only run on Linux
        HostSigInfo_t val{};

        val.FEXDef.si_signo = si_signo;
        val.FEXDef.si_errno = si_errno;
        val.FEXDef.si_code = si_code;

        // Host siginfo has a pad member that is set to zeros
        val.FEXDef.__pad0 = 0;

        // Copy over the union
        // The union is different sizes on 64-bit versus 32-bit
        memcpy(val.FEXDef._pad, _sifields.pad, std::min(sizeof(val.FEXDef._pad), sizeof(_sifields.pad)));

        return val.host;
      }

      siginfo_t(::siginfo_t val) {
        HostSigInfo_t host;
        host.host = val;

        si_signo = host.FEXDef.si_signo;
        si_errno = host.FEXDef.si_errno;
        si_code = host.FEXDef.si_code;

        // Copy over the union
        // The union is different sizes on 64-bit versus 32-bit
        memcpy(_sifields.pad, host.FEXDef._pad, std::min(sizeof(host.FEXDef._pad), sizeof(_sifields.pad)));
      }
      static_assert(offsetof(::siginfo_t, si_signo) == offsetof(HostSigInfo_t, FEXDef.si_signo), "si_signo in wrong location?");
      static_assert(offsetof(::siginfo_t, si_errno) == offsetof(HostSigInfo_t, FEXDef.si_errno), "si_errno in wrong location?");
      static_assert(offsetof(::siginfo_t, si_code) == offsetof(HostSigInfo_t, FEXDef.si_code), "si_code in wrong location?");
    };
    static_assert(sizeof(FEXCore::x86::siginfo_t) == 128, "This needs to be the right size");

    struct FEX_PACKED stack_t {
      uint32_t ss_sp; // XXX: should be compat_ptr<void>
      int ss_flags;
      uint32_t ss_size;
    };

    static_assert(sizeof(FEXCore::x86::stack_t) == 12, "This needs to be the right size");

    struct FEX_PACKED mcontext_t {
      uint32_t gregs[19];
      uint32_t fpregs; // XXX: should be compat_ptr<FEXCore::x86::_libc_fpstate>
      uint32_t oldmask;
      uint32_t cr2;
    };
    static_assert(sizeof(FEXCore::x86::mcontext_t) == 88, "This needs to be the right size");

    struct _libc_fpreg {
      uint16_t significand[4];
      uint16_t exponent;
    };
    static_assert(sizeof(FEXCore::x86::_libc_fpreg) == 10, "This needs to be the right size");

    enum fpstate_magic {
      // Legacy fpstate
      MAGIC_FPU = 0xFFFF'0000,
      // Contains extended state information
      MAGIC_XFPSTATE = 0x0,
    };
    struct FEX_PACKED _libc_fpstate {
      uint32_t fcw;
      uint32_t fsw;
      uint32_t ftw;
      uint32_t fop;
      uint32_t cssel;
      uint32_t dataoff;
      uint32_t datasel;
      FEXCore::x86::_libc_fpreg _st[8];
      uint32_t status;

      // Extended FPU data
      uint32_t pad[6]; // Ignored FXSR data
      uint32_t mxcsr;
      uint32_t reserved;
      __uint128_t _st_pad[8]; // Ignored st data
      __uint128_t _xmm[8]; // First 8 XMM registers
      uint32_t pad2[44]; // Second 8 XMM registers plus padding
      uint32_t pad3[12]; // extended state encoding
    };
    static_assert(sizeof(FEXCore::x86::_libc_fpstate) == 624, "This needs to be the right size");

    struct FEX_PACKED ucontext_t {
      uint32_t uc_flags;
      uint32_t uc_link; // XXX: should be a compat_ptr<FEXCore::x86::ucontext_t>
      FEXCore::x86::stack_t uc_stack;
      FEXCore::x86::mcontext_t uc_mcontext;
      FEXCore::x86_64::sigset_t uc_sigmask; // This matches across architectures
    };
    static_assert(sizeof(FEXCore::x86::ucontext_t) == 236, "This needs to be the right size");

  }
}
