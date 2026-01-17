#pragma once

#include <functional>

#include "activities/Activity.h"

class ConfirmActionActivity final : public Activity {
  // Pointers must remain valid for the lifetime of this Activity (use static/constexpr strings)
  const char* title;
  const char* line1;
  const char* line2;
  const std::function<void()> onConfirm;
  const std::function<void()> onCancel;
  int selection = 1;  // Default to "No" for safety

  void render() const;

 public:
  explicit ConfirmActionActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const char* title,
                                 const char* line1, const char* line2, std::function<void()> onConfirm,
                                 std::function<void()> onCancel)
      : Activity("ConfirmAction", renderer, mappedInput),
        title(title),
        line1(line1),
        line2(line2),
        onConfirm(std::move(onConfirm)),
        onCancel(std::move(onCancel)) {}

  void onEnter() override;
  void loop() override;
};
