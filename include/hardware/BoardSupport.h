#pragma once

namespace transitink::hardware {

void configureButtons();
bool homeButtonPressed();
bool configButtonPressed();
bool factoryResetUpButtonPressed();
bool factoryResetDownButtonPressed();
void configureHomeWakeup();
void disableHomeWakeup();
void configureChargeWakeup();
void disableChargeWakeup();

}  // namespace transitink::hardware
