#include "code.h"
#include "constants.h"
#include "cpu.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

    cpu_init();
    memset(ram, 0, sizeof(ram));

    x = 0;
    ram[Enemy_ID] = Bloober;
    ram[Enemy_State] = 0;
    ram[Enemy_MovingDir] = 2;
    ram[Enemy_PageLoc] = 2;
    ram[Enemy_X_Position] = 0x80;
    ram[Enemy_Y_Position] = 0x87;
    ram[Enemy_Y_MoveForce] = 0;
    ram[BlooperMoveCounter] = 2;
    ram[BlooperMoveSpeed] = 0;
    ram[EnemyIntervalTimer] = 0;
    ram[Player_Y_Position] = 0x98;
    ram[PseudoRandomBitReg + 1] = 1;
    ram[FrameCounter] = 0xd7;

    /*
     * The original JumpEngine starts with ASL, so selector 7 clears carry
     * before entering MoveBloober. Poisoning carry exposes a C dispatcher
     * that forgets that side effect: 0x87 + 0x10 + carry reaches the player
     * and clears the movement counter one frame too early.
     */
    carry_flag = true;
    EnemyMovementSubs();

    failed |= expect_byte(
        "first-frame BlooperMoveCounter",
        ram[BlooperMoveCounter],
        2
    );
    failed |= expect_byte(
        "first-frame BlooperMoveSpeed",
        ram[BlooperMoveSpeed],
        0
    );
    failed |= expect_byte(
        "first-frame Enemy_Y_Position",
        ram[Enemy_Y_Position],
        0x87
    );

    /*
     * On the next eighth-frame boundary, the faulty path accelerates from
     * zero to one. The correctly dispatched float-down path keeps speed zero.
     */
    ram[FrameCounter] = 0xd8;
    carry_flag = true;
    EnemyMovementSubs();

    failed |= expect_byte(
        "second-frame BlooperMoveCounter",
        ram[BlooperMoveCounter],
        2
    );
    failed |= expect_byte(
        "second-frame BlooperMoveSpeed",
        ram[BlooperMoveSpeed],
        0
    );
    failed |= expect_byte(
        "second-frame Enemy_X_Position",
        ram[Enemy_X_Position],
        0x80
    );
    failed |= expect_byte(
        "second-frame Enemy_Y_Position",
        ram[Enemy_Y_Position],
        0x88
    );

    if (failed != 0) {
        return 1;
    }
    puts("jump-engine dispatch carry regression: PASS");
    return 0;
}
