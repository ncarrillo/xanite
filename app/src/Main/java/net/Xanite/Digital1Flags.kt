package com.xanite

enum class Digital1Flags(val bit: Int) {
    CELL_PAD_CTRL_LEFT(0),
    CELL_PAD_CTRL_DOWN(1),
    CELL_PAD_CTRL_RIGHT(2),
    CELL_PAD_CTRL_UP(3),
    CELL_PAD_CTRL_START(4),
    CELL_PAD_CTRL_R3(5),
    CELL_PAD_CTRL_L3(6),
    CELL_PAD_CTRL_SELECT(7),
    CELL_PAD_CTRL_PS(8),
    None(0)
}