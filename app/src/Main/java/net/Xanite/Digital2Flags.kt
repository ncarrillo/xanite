package com.xanite

enum class Digital2Flags(val bit: Int) {
    CELL_PAD_CTRL_SQUARE(0),
    CELL_PAD_CTRL_CROSS(1),
    CELL_PAD_CTRL_CIRCLE(2),
    CELL_PAD_CTRL_TRIANGLE(3),
    CELL_PAD_CTRL_R1(4),
    CELL_PAD_CTRL_L1(5),
    CELL_PAD_CTRL_R2(6),
    CELL_PAD_CTRL_L2(7),
    None(0)
}