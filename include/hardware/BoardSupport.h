#pragma once

namespace transitink::hardware {

void configureButtons();
bool startButtonMonitoring();
bool homeButtonPressed();
bool factoryResetUpButtonPressed();
bool factoryResetDownButtonPressed();
bool takeConfigClick();
void clearPendingConfigClick();
bool takeWidgetPageClick();
void clearPendingWidgetPageClick();
bool takeFactoryResetHold();
bool takeHomePress();
void clearPendingHomePress();
void configureHomeWakeup();
void disableHomeWakeup();

}  // namespace transitink::hardware
