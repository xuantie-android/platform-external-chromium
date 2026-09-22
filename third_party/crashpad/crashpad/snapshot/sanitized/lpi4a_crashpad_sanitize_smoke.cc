// Copyright 2026 The Crashpad Authors
// Licensed under the Apache License, Version 2.0.

#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

#include "base/logging.h"
#include "snapshot/sanitized/thread_snapshot_sanitized.h"
#include "snapshot/test/test_thread_snapshot.h"

namespace {
class Memory final : public crashpad::MemorySnapshot {
 public:
  explicit Memory(std::vector<uint8_t> bytes) : bytes_(std::move(bytes)) {}
  uint64_t Address() const override { return 0x2000; }
  size_t Size() const override { return bytes_.size(); }
  bool Read(Delegate* delegate) const override {
    auto copy = bytes_;
    return delegate->MemorySnapshotDelegateRead(copy.data(), copy.size());
  }
  const MemorySnapshot* MergeWithOtherSnapshot(const MemorySnapshot*) const override {
    return nullptr;
  }
 private:
  std::vector<uint8_t> bytes_;
};

class Reader final : public crashpad::MemorySnapshot::Delegate {
 public:
  std::vector<uint8_t> bytes;
  bool MemorySnapshotDelegateRead(void* data, size_t size) override {
    auto* p = static_cast<uint8_t*>(data);
    bytes.assign(p, p + size);
    return true;
  }
};

template <typename Word>
void Test(bool is_64_bit) {
  using crashpad::internal::MemorySnapshotSanitized;
  const Word original[] = {static_cast<Word>(0x10088), 7,
                           static_cast<Word>(0x6162636465666768ULL)};
  std::vector<uint8_t> bytes(sizeof(original));
  std::memcpy(bytes.data(), original, sizeof(original));
  crashpad::test::TestThreadSnapshot thread;
  thread.MutableContext()->architecture = is_64_bit
      ? crashpad::kCPUArchitectureX86_64 : crashpad::kCPUArchitectureX86;
  thread.SetStack(std::make_unique<Memory>(bytes));
  thread.AddExtraMemory(std::make_unique<Memory>(bytes));
  crashpad::RangeSet ranges;
  ranges.Insert(0x10000, 0x11000);
  crashpad::internal::ThreadSnapshotSanitized sanitized(&thread, &ranges);
  auto extra = sanitized.ExtraMemory();
  CHECK(extra.size() == 1u);
  CHECK(extra[0]->Address() == 0x2000u);
  CHECK(extra[0]->Size() == sizeof(original));
  CHECK(extra[0] != thread.ExtraMemory()[0]);
  CHECK(extra[0] == sanitized.ExtraMemory()[0]);
  Reader reader;
  CHECK(extra[0]->Read(&reader));
  Word actual[3];
  CHECK(reader.bytes.size() == sizeof(actual));
  std::memcpy(actual, reader.bytes.data(), sizeof(actual));
  CHECK(actual[0] == original[0]);
  CHECK(actual[1] == 7u);
  CHECK(actual[2] == static_cast<Word>(MemorySnapshotSanitized::kDefaced));
  Reader raw;
  CHECK(thread.ExtraMemory()[0]->Read(&raw));
  CHECK(raw.bytes == bytes);
  std::printf("CRASHPAD_EXTRA_MEMORY_%zu_BIT_REDACTION_PASS\n", sizeof(Word) * 8);
}
}  // namespace

int main() {
  Test<uint32_t>(false);
  Test<uint64_t>(true);
  return 0;
}
