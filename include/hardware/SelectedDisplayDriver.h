#pragma once

#include "hardware/BoardProfile.h"
#include "hardware/displays/Ssd1683DisplayDriver.h"

namespace transitink::hardware {

template <DisplayDriverKind Kind>
struct DisplayDriverFor;

template <>
struct DisplayDriverFor<DisplayDriverKind::Ssd1683> {
    using Type = Ssd1683DisplayDriver;
};

using SelectedDisplayDriver =
    typename DisplayDriverFor<kBoardProfile.display.driver>::Type;

}  // namespace transitink::hardware
