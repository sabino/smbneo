#include "data.h"

#include <stdint.h>
#include <stdio.h>

static uint16_t data_index(uint16_t address) {
    return (uint16_t)(address - UINT16_C(0x8000));
}

static int expect_address(
    const char *name,
    uint16_t actual,
    uint16_t expected
) {
    if (actual == expected) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected 0x%04x, got 0x%04x\n",
        name,
        (unsigned int)expected,
        (unsigned int)actual
    );
    return 1;
}

static int expect_byte(
    const char *name,
    uint8_t actual,
    uint8_t expected
) {
    if (actual == expected) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected 0x%02x, got 0x%02x\n",
        name,
        (unsigned int)expected,
        (unsigned int)actual
    );
    return 1;
}

int main(void) {
    int failed = 0;

    failed |= expect_address(
        "fireball guard shifts the next table",
        Bubble_MForceData,
        (uint16_t)(FireballXSpdData + 3u)
    );
    failed |= expect_byte(
        "fireball index-two ROM byte",
        data[data_index((uint16_t)(FireballXSpdData + 2u))],
        0x86
    );

    failed |= expect_address(
        "bubble timer follows force data",
        BubbleTimerData,
        (uint16_t)(Bubble_MForceData + 2u)
    );
    failed |= expect_address(
        "bubble guard covers every byte index",
        FlagpoleScoreMods,
        (uint16_t)(BubbleTimerData + 256u)
    );
    failed |= expect_byte(
        "bubble force observed stale index",
        data[data_index((uint16_t)(Bubble_MForceData + 145u))],
        0xf9
    );
    failed |= expect_byte(
        "bubble timer observed stale index",
        data[data_index((uint16_t)(BubbleTimerData + 145u))],
        0x04
    );
    failed |= expect_byte(
        "bubble guard final ROM byte",
        data[data_index((uint16_t)(BubbleTimerData + 255u))],
        0x02
    );

    failed |= expect_address(
        "firebar mirror and offset tables remain adjacent",
        FirebarTblOffsets,
        (uint16_t)(FirebarMirrorData + 4u)
    );
    failed |= expect_address(
        "firebar offset and Y tables remain adjacent",
        FirebarYPos,
        (uint16_t)(FirebarTblOffsets + 12u)
    );
    failed |= expect_address(
        "firebar code-byte guard shifts the next table",
        PRandomSubtracter,
        (uint16_t)(FirebarYPos + 16u)
    );
    failed |= expect_byte(
        "firebar maximum mirror index",
        data[data_index((uint16_t)(FirebarMirrorData + 31u))],
        0xd0
    );

    failed |= expect_address(
        "flying-enemy tables remain adjacent",
        FlyCCBPriority,
        (uint16_t)(PRandomSubtracter + 5u)
    );
    failed |= expect_address(
        "flying-enemy guard shifts the next table",
        LakituDiffAdj,
        (uint16_t)(FlyCCBPriority + 16u)
    );
    failed |= expect_byte(
        "flying-enemy first opcode byte",
        data[data_index((uint16_t)(FlyCCBPriority + 5u))],
        0xb5
    );
    failed |= expect_byte(
        "flying-enemy final opcode byte",
        data[data_index((uint16_t)(FlyCCBPriority + 15u))],
        0x03
    );

    if (failed != 0) {
        return 1;
    }
    puts("ROM fall-through data tests: OK");
    return 0;
}
