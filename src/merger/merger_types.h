#pragma once
#include <cstdint>

enum class SelectionGranularity : uint8_t
{
    OBJECT,
    FACE,
    EDGE,
    VERTEX
};

enum class ToolMode
{
    PLACE,
    SELECT,
    GRAB,
    SCALE,
    ROTATE
};