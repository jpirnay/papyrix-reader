#include "MappedInputManager.h"

#include "CrossPointSettings.h"
#include "ThemeManager.h"

decltype(InputManager::BTN_BACK) MappedInputManager::mapButton(const Button button) const {
  const auto frontLayout = static_cast<FrontButtonLayout>(THEME.frontButtonLayout);
  const auto sideLayout = static_cast<CrossPointSettings::SIDE_BUTTON_LAYOUT>(SETTINGS.sideButtonLayout);

  switch (button) {
    case Button::Back:
      switch (frontLayout) {
        case FRONT_LRBC:
          return InputManager::BTN_LEFT;
        case FRONT_BCLR:
        default:
          return InputManager::BTN_BACK;
      }
    case Button::Confirm:
      switch (frontLayout) {
        case FRONT_LRBC:
          return InputManager::BTN_RIGHT;
        case FRONT_BCLR:
        default:
          return InputManager::BTN_CONFIRM;
      }
    case Button::Left:
      switch (frontLayout) {
        case FRONT_LRBC:
          return InputManager::BTN_BACK;
        case FRONT_BCLR:
        default:
          return InputManager::BTN_LEFT;
      }
    case Button::Right:
      switch (frontLayout) {
        case FRONT_LRBC:
          return InputManager::BTN_CONFIRM;
        case FRONT_BCLR:
        default:
          return InputManager::BTN_RIGHT;
      }
    case Button::Up:
      return InputManager::BTN_UP;
    case Button::Down:
      return InputManager::BTN_DOWN;
    case Button::Power:
      return InputManager::BTN_POWER;
    case Button::PageBack:
      switch (sideLayout) {
        case CrossPointSettings::NEXT_PREV:
          return InputManager::BTN_DOWN;
        case CrossPointSettings::PREV_NEXT:
        default:
          return InputManager::BTN_UP;
      }
    case Button::PageForward:
      switch (sideLayout) {
        case CrossPointSettings::NEXT_PREV:
          return InputManager::BTN_UP;
        case CrossPointSettings::PREV_NEXT:
        default:
          return InputManager::BTN_DOWN;
      }
  }

  return InputManager::BTN_BACK;
}

bool MappedInputManager::wasPressed(const Button button) const { return inputManager.wasPressed(mapButton(button)); }

bool MappedInputManager::wasReleased(const Button button) const { return inputManager.wasReleased(mapButton(button)); }

bool MappedInputManager::isPressed(const Button button) const { return inputManager.isPressed(mapButton(button)); }

bool MappedInputManager::wasAnyPressed() const { return inputManager.wasAnyPressed(); }

bool MappedInputManager::wasAnyReleased() const { return inputManager.wasAnyReleased(); }

unsigned long MappedInputManager::getHeldTime() const { return inputManager.getHeldTime(); }

MappedInputManager::Labels MappedInputManager::mapLabels(const char* back, const char* confirm, const char* previous,
                                                         const char* next) const {
  const auto layout = static_cast<FrontButtonLayout>(THEME.frontButtonLayout);

  switch (layout) {
    case FRONT_LRBC:
      return {previous, next, back, confirm};
    case FRONT_BCLR:
    default:
      return {back, confirm, previous, next};
  }
}
