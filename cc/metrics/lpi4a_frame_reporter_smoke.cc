// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdio>
#include <memory>
#include <optional>

#include "base/at_exit.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "cc/metrics/compositor_frame_reporting_controller.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/frame_sorter.h"
#include "cc/scheduler/scheduler.h"
#include "components/viz/common/frame_timing_details.h"
#include "ui/events/types/event_type.h"

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);
  base::FeatureList::InitInstance("", "");
  cc::FrameSorter frame_sorter;
  cc::CompositorFrameReportingController controller(false, 1, false);
  controller.SetFrameSorter(&frame_sorter);
  viz::BeginFrameArgs first;
  first.frame_id = {2, 54888};
  first.frame_time = base::TimeTicks::Now();
  first.interval = base::Milliseconds(16);
  controller.WillBeginImplFrame(first, false);
  controller.WillBeginMainFrame(first);
  controller.OnFinishImplFrame(first.frame_id, true);
  controller.DidNotProduceFrame(first.frame_id,
                               cc::FrameSkippedReason::kWaitingOnMain);

  // The renderer main thread is still busy, so the synchronous compositor
  // finishes another BeginFrame without a draw. A later DemandDraw can still
  // submit it once the older main frame activates.
  auto second = first;
  second.frame_id.sequence_number++;
  second.frame_time = base::TimeTicks::Now();
  controller.WillBeginImplFrame(second, false);
  controller.OnFinishImplFrame(second.frame_id, true);
  controller.DidNotProduceFrame(second.frame_id,
                               cc::FrameSkippedReason::kWaitingOnMain);
  controller.BeginMainFrameStarted(base::TimeTicks::Now());
  controller.NotifyReadyToCommit(nullptr);
  controller.WillCommit();
  controller.DidCommit();
  controller.WillActivate();
  controller.DidActivate();

  const auto now = base::TimeTicks::Now();
  auto event = cc::EventMetrics::CreateForTesting(
      ui::EventType::kTouchPressed, now, now,
      base::DefaultTickClock::GetInstance(), std::nullopt);
  CHECK(event);
  event->SetDispatchStageTimestamp(
      cc::EventMetrics::DispatchStage::kRendererCompositorStarted);
  event->SetDispatchStageTimestamp(
      cc::EventMetrics::DispatchStage::kRendererCompositorFinished);
  cc::SubmitInfo submit(1, base::TimeTicks::Now());
  submit.events_metrics.impl_event_metrics.push_back(std::move(event));
  std::puts("SUBMIT_AFTER_WAITING_ON_MAIN");
  std::fflush(stdout);
  controller.DidSubmitCompositorFrame(submit, second.frame_id, first.frame_id);
  viz::FrameTimingDetails details;
  details.presentation_feedback.timestamp = base::TimeTicks::Now();
  controller.DidPresentCompositorFrame(1, details);
  std::puts("FRAME_REPORTER_LATE_DRAW_PASS");
  return 0;
}
