#pragma once

#include <functional>

#include "activities/Activity.h"

class SystemInfoActivity final : public Activity {
  const std::function<void()> onComplete;

  void render() const;

 public:
  explicit SystemInfoActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::function<void()> onComplete)
      : Activity("SystemInfo", renderer, mappedInput), onComplete(std::move(onComplete)) {}

  void onEnter() override;
  void loop() override;
};
