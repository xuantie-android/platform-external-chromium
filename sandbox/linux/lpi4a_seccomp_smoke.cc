// Exercise the real Chromium RISC-V syscall thunk, policy compiler and trap.
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <memory>
#include <cpuinfo.h>

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/bpf_dsl/policy_compiler.h"
#include "sandbox/linux/seccomp-bpf-helpers/baseline_policy_android.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/seccomp-bpf/syscall.h"
#include "sandbox/linux/seccomp-bpf/trap.h"
#include "sandbox/linux/system_headers/linux_seccomp.h"
#include "sandbox/linux/system_headers/linux_filter.h"

namespace {
constexpr intptr_t kArgs[] = {0x100000001, 0x200000002, 0x300000003,
                             0x400000004, 0x500000005, 0x600000006};
constexpr intptr_t kReply = 0x13579;
volatile sig_atomic_t trapped = 0;

intptr_t TestTrap(const arch_seccomp_data& data, void* expected_ip) {
  bool valid = data.nr == __NR_getppid && data.arch == AUDIT_ARCH_RISCV64 &&
               data.instruction_pointer == reinterpret_cast<uintptr_t>(expected_ip);
  valid = valid && data.args[0] == static_cast<uint64_t>(kArgs[0]) &&
          data.args[1] == static_cast<uint64_t>(kArgs[1]) &&
          data.args[2] == static_cast<uint64_t>(kArgs[2]) &&
          data.args[3] == static_cast<uint64_t>(kArgs[3]) &&
          data.args[4] == static_cast<uint64_t>(kArgs[4]) &&
          data.args[5] == static_cast<uint64_t>(kArgs[5]);
  trapped = valid ? 1 : 2;
  return valid ? kReply : -EFAULT;
}

class TestPolicy final : public sandbox::bpf_dsl::Policy {
 public:
  sandbox::bpf_dsl::ResultExpr EvaluateSyscall(int nr) const override {
    using namespace sandbox::bpf_dsl;
    if (nr == __NR_getppid) {
      const Arg<uint64_t> first(0);
      const Arg<uint64_t> last(5);
      return If(AllOf(first == static_cast<uint64_t>(kArgs[0]),
                      last == static_cast<uint64_t>(kArgs[5])),
                Trap(TestTrap, reinterpret_cast<const void*>(sandbox::Syscall::Call(-1))))
          .Else(Error(EPERM));
    }
    return Allow();
  }
};
}  // namespace

int main() {
  using sandbox::SandboxBPF;
  using sandbox::Syscall;
  if (!cpuinfo_initialize() || cpuinfo_get_processors_count() == 0 ||
      cpuinfo_has_riscv_v()) return 7;
  printf("CPUINFO_C910_PASS processors=%u standard-rvv=0\n", cpuinfo_get_processors_count());
  errno = ERANGE;
  if (Syscall::Call(__NR_getpid) != getpid() || Syscall::InvalidCall() != -ENOSYS ||
      errno != ERANGE || Syscall::Call(-1) == 0) return 1;
  // Match SeccompStarterAndroid and Chromium's Android test runner: Bionic
  // installs a SIGSYS handler before main which Chromium must take over.
  if (signal(SIGSYS, SIG_DFL) == SIG_ERR) return 6;
  for (unsigned variant = 0; variant != 8; ++variant) {
    sandbox::BaselinePolicyAndroid::RuntimeOptions options;
    options.allow_userfaultfd_ioctls = (variant & 1) != 0;
    options.should_restrict_renderer_syscalls = (variant & 2) != 0;
    options.should_restrict_clone_params = (variant & 4) != 0;
    sandbox::BaselinePolicyAndroid baseline(options);
    sandbox::bpf_dsl::PolicyCompiler compiler(&baseline, sandbox::Trap::Registry());
    if (compiler.Compile().empty()) return 8;
  }
  puts("ANDROID_BASELINE_POLICIES_PASS variants=8");
  SandboxBPF sandbox(std::make_unique<TestPolicy>());
  if (!sandbox.StartSandbox(SandboxBPF::SeccompLevel::SINGLE_THREADED)) return 2;
  errno = ERANGE;
  intptr_t value = Syscall::Call(__NR_getppid, kArgs[0], kArgs[1], kArgs[2],
                               kArgs[3], kArgs[4], kArgs[5]);
  if (value != kReply || trapped != 1 || errno != ERANGE) {
    fprintf(stderr, "trap mismatch value=%ld trapped=%d errno=%d\n",
            static_cast<long>(value), static_cast<int>(trapped), errno);
    return 3;
  }
  trapped = 0;
  value = Syscall::Call(__NR_getppid, kArgs[0], kArgs[1], kArgs[2],
                       kArgs[3], kArgs[4], kArgs[5] ^ (intptr_t{1} << 32));
  if (value != -EPERM || trapped != 0 || errno != ERANGE) return 4;
  if (Syscall::Call(__NR_getpid) != getpid() || Syscall::InvalidCall() != -ENOSYS) return 5;
  puts("CHROMIUM_RISCV_SECCOMP_PASS thunk/IP/arch/6args/trap/return/64bit-reject/errno");
  return 0;
}
