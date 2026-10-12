#pragma once
#include "Settings.h"
#include <array>
#include <cstdint>

namespace ms
{
    struct Value { bool available = false; uint64_t value = 0; };
    struct Snapshot
    {
        std::array<Value, static_cast<size_t>(StatId::Count)> values{};
        uint64_t generation = 0;
    };

    const Snapshot& CurrentSnapshot();
    bool RefreshStatistics();
    void ClearStatistics();
}
