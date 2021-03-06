/* -*- Mode: C++; tab-width: 8; c-basic-offset: 2; indent-tabs-mode: nil; -*- */

#ifndef RR_MONKEYPATCHER_H_
#define RR_MONKEYPATCHER_H_

#include <unordered_set>
#include <vector>

#include "preload/preload_interface.h"

#include "remote_ptr.h"
#include "remote_code_ptr.h"

namespace rr {

class RecordTask;
class ScopedFd;
class Task;

/**
 * A class encapsulating patching state. There is one instance of this
 * class per tracee address space. Currently this class performs the following
 * tasks:
 *
 * 1) Patch the VDSO's user-space-only implementation of certain system calls
 * (e.g. gettimeofday) to do a proper kernel system call instead, so rr can
 * trap and record it (x86-64 only).
 *
 * 2) Patch the VDSO __kernel_vsyscall fast-system-call stub to redirect to
 * our syscall hook in the preload library (x86 only).
 *
 * 3) Patch syscall instructions whose following instructions match a known
 * pattern to call the syscall hook.
 *
 * Monkeypatcher only runs during recording, never replay.
 */
class Monkeypatcher {
public:
  Monkeypatcher() : stub_buffer_allocated(0) {}
  Monkeypatcher(const Monkeypatcher&) = default;

  /**
   * Apply any necessary patching immediately after exec.
   * In this hook we patch everything that doesn't depend on the preload
   * library being loaded.
   */
  void patch_after_exec(RecordTask* t);

  /**
   * During librrpreload initialization, apply patches that require the
   * preload library to be initialized.
   */
  void patch_at_preload_init(RecordTask* t);

  /**
   * Try to patch the syscall instruction that |t| just entered. If this
   * returns false, patching failed and the syscall should be processed
   * as normal. If this returns true, patching succeeded and the syscall
   * was aborted; ip() has been reset to the start of the patched syscall,
   * and execution should resume normally to execute the patched code.
   * Zero or more mapping operations are also recorded to the trace and must
   * be replayed.
   */
  bool try_patch_syscall(RecordTask* t);

  void init_dynamic_syscall_patching(
      RecordTask* t, int syscall_patch_hook_count,
      remote_ptr<syscall_patch_hook> syscall_patch_hooks,
      remote_ptr<void> stub_buffer, remote_ptr<void> stub_buffer_end,
      remote_ptr<void> syscall_hook_trampoline);

  /**
   * Try to allocate a stub from the sycall patching stub buffer. Returns null
   * if there's no buffer or we've run out of free stubs.
   */
  remote_ptr<uint8_t> allocate_stub(RecordTask* t, size_t bytes);

  /**
   * Apply any necessary patching immediately after an mmap. We use this to
   * patch libpthread.so.
   */
  void patch_after_mmap(RecordTask* t, remote_ptr<void> start, size_t size,
                        size_t offset_pages, int child_fd);

  remote_ptr<void> x86_sysenter_vsyscall;
  /**
   * The list of pages we've allocated to hold our extended jumps.
   */
  struct ExtendedJumpPage {
    ExtendedJumpPage(remote_ptr<uint8_t> addr) : addr(addr), allocated(0) {}
    remote_ptr<uint8_t> addr;
    size_t allocated;
  };
  std::vector<ExtendedJumpPage> extended_jump_pages;

  /**
   * Returns true if the instruction at address p should be considered
   * "not part of the syscallbuf code", i.e. it's safe to deliver signals there
   * without affecting the syscall buffering logic. If not sure, return false.
   */
  bool is_syscallbuf_excluded_instruction(remote_ptr<void> p) {
    return p >= syscall_hook_trampoline && p < stub_buffer_end;
  }

private:
  /**
   * The list of supported syscall patches obtained from the preload
   * library. Each one matches a specific byte signature for the instruction(s)
   * after a syscall instruction.
   */
  std::vector<syscall_patch_hook> syscall_hooks;
  /**
   * The addresses of the instructions following syscalls that we've tried
   * (or are currently trying) to patch.
   */
  std::unordered_set<remote_code_ptr> tried_to_patch_syscall_addresses;
  /**
   * Writable executable memory where we can generate stubs.
   */
  remote_ptr<void> stub_buffer;
  remote_ptr<void> stub_buffer_end;
  remote_ptr<void> syscall_hook_trampoline;
  size_t stub_buffer_allocated;
};

} // namespace rr

#endif /* RR_MONKEYPATCHER_H_ */
