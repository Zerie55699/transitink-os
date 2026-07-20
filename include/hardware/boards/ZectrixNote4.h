#pragma once

#include "hardware/BoardProfileTypes.h"

namespace transitink::hardware {

inline constexpr BoardProfile kBoardProfile{
    "zectrix_note4",
    "Zectrix Note 4",
    {
        DisplayDriverKind::Ssd1683,
        400,      // width
        300,      // height
        10,       // DC
        11,       // chip select
        12,       // clock
        13,       // MOSI
        9,        // reset
        8,        // busy
        6,        // panel power
        1,        // panel power active HIGH
        0,        // busy active LOW
        8000000,  // SPI frequency
    },
    {
        true,
        4,  // ADC
        2,  // voltage divider multiplier
        AdcAttenuation::Db11,
        17,  // sense power
        1,   // sense power active HIGH
        2,   // charging detect
        0,   // charging active LOW
        1,   // charge-full detect
        1,   // charge-full active HIGH
        3,   // green full-charge indicator
        0,   // indicator active LOW
        8,   // ADC samples
    },
    {
        0,   // home
        39,  // volume up
        18,  // volume down
        39,  // config
        39,  // factory-reset up
        18,  // factory-reset down
        PinBias::PullUp,
        0,
        true,
        true,
        5000,
        30,
        1200,
    },
};

static_assert(kBoardProfile.display.width % 8 == 0,
              "display width must be byte-aligned");

}  // namespace transitink::hardware
