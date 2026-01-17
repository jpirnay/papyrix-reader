#pragma once

#include <functional>

#include "activities/ActivityWithSubactivity.h"

class StorageActivity final : public ActivityWithSubactivity {
  const std::function<void()> onComplete;
  int selectedIndex = 0;
  static constexpr int MENU_COUNT = 3;

  void render() const;
  void executeAction(int actionIndex);

 public:
  explicit StorageActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::function<void()> onComplete)
      : ActivityWithSubactivity("Cleanup", renderer, mappedInput), onComplete(std::move(onComplete)) {}

  void onEnter() override;
  void loop() override;
};
