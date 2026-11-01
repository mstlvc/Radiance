// Share codes: a whole look in a short, typeable string ("RD-…"), so friends can
// pass looks around without any server.
#pragma once

#include "core/Model.h"

#include <optional>
#include <string>
#include <string_view>

namespace radiance::sharecode
{
    std::string Encode(const ColourLook& look);
    // Accepts lower case, spaces and dashes; rejects anything that fails its checksum.
    std::optional<ColourLook> Decode(std::string_view code);
}
