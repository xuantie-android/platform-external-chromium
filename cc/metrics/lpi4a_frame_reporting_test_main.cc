// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/task_environment.h"
#include "base/test/test_suite.h"
#include "mojo/core/embedder/embedder.h"

namespace {
// Match CCTestSuite's task/Mojo setup without initializing an unrelated GL
// display for these CPU-only frame-reporting and trace-analysis tests.
class FrameReportingTestSuite : public base::TestSuite {
 public:
  FrameReportingTestSuite(int argc, char** argv) : TestSuite(argc, argv) {}
 protected:
  void Initialize() override {
    TestSuite::Initialize();
    task_environment_ = std::make_unique<base::test::TaskEnvironment>();
  }
  void Shutdown() override {
    task_environment_.reset();
    TestSuite::Shutdown();
  }
 private:
  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
};
}  // namespace

int main(int argc, char** argv) {
  FrameReportingTestSuite suite(argc, argv);
  mojo::core::Init();
  return base::LaunchUnitTests(
      argc, argv, base::BindOnce(&base::TestSuite::Run, base::Unretained(&suite)));
}
