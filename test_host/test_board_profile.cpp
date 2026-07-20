#include <cassert>
#include <cstring>

#include "hardware/BoardProfile.h"

int main() {
    using transitink::hardware::AdcAttenuation;
    using transitink::hardware::DisplayDriverKind;
    using transitink::hardware::PinBias;
    using transitink::hardware::kBoardProfile;

    assert(std::strcmp(kBoardProfile.id, "zectrix_note4") == 0);
    assert(std::strcmp(kBoardProfile.displayName, "Zectrix Note 4") == 0);

    assert(kBoardProfile.display.driver == DisplayDriverKind::Ssd1683);
    assert(kBoardProfile.display.width == 400);
    assert(kBoardProfile.display.height == 300);
    assert(kBoardProfile.display.dcPin == 10);
    assert(kBoardProfile.display.chipSelectPin == 11);
    assert(kBoardProfile.display.clockPin == 12);
    assert(kBoardProfile.display.mosiPin == 13);
    assert(kBoardProfile.display.resetPin == 9);
    assert(kBoardProfile.display.busyPin == 8);
    assert(kBoardProfile.display.powerPin == 6);
    assert(kBoardProfile.display.busyActiveLevel == 0);

    assert(kBoardProfile.battery.available);
    assert(kBoardProfile.battery.adcPin == 4);
    assert(kBoardProfile.battery.voltageMultiplier == 2);
    assert(kBoardProfile.battery.attenuation == AdcAttenuation::Db11);
    assert(kBoardProfile.battery.sensePowerPin == 17);
    assert(kBoardProfile.battery.chargeDetectPin == 2);
    assert(kBoardProfile.battery.chargeDetectActiveLevel == 0);
    assert(kBoardProfile.battery.chargeFullPin == 1);
    assert(kBoardProfile.battery.chargeFullActiveLevel == 1);
    assert(kBoardProfile.battery.chargeIndicatorPin == 3);
    assert(kBoardProfile.battery.chargeIndicatorActiveLevel == 0);

    assert(kBoardProfile.buttons.homePin == 0);
    assert(kBoardProfile.buttons.upPin == 39);
    assert(kBoardProfile.buttons.downPin == 18);
    assert(kBoardProfile.buttons.configPin == 39);
    assert(kBoardProfile.buttons.factoryResetUpPin == 39);
    assert(kBoardProfile.buttons.factoryResetDownPin == 18);
    assert(kBoardProfile.buttons.bias == PinBias::PullUp);
    assert(kBoardProfile.buttons.pressedLevel == 0);
    assert(kBoardProfile.buttons.homeSupportsGpioWake);
    assert(kBoardProfile.buttons.factoryResetHoldMs == 5000);
    assert(kBoardProfile.buttons.configDebounceMs == 30);
    assert(kBoardProfile.buttons.configMaxClickMs == 1200);
    return 0;
}
