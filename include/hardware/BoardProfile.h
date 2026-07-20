#pragma once

#if defined(TRANSITINK_BOARD_ZECTRIX_NOTE4)
#include "hardware/boards/ZectrixNote4.h"
#else
#error "No TransitInk board profile selected. Define a TRANSITINK_BOARD_* build flag."
#endif
