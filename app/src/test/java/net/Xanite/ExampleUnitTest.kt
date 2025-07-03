package com.xanite

import org.junit.Assert.assertEquals
import org.junit.Test

class ExampleUnitTest {

    @Test
    fun testAddition_isCorrect() {
        val result = 2 + 2
        assertEquals("Addition should be correct", 4, result)
    }
    
    @Test
    fun testAddition() {
        assertEquals(8, 5 + 3)
    }
}
