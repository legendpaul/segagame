#include "genesis.h"
#include "scene_match.h"
#include "game_state.h"
#include "teams.h"
#include "input_mgr.h"
#include "player.h"
#include "ball.h"
#include "ai_mgr.h"
#include "sound_mgr.h"
#include "sprites_data.h"

#define SLOT_PLAYER 0
#define SLOT_CPU    1
#define SLOT_BALL   2

typedef enum {
    MS_ANNOUNCE = 0,
    MS_PLAYER_HOLD,
    MS_CPU_HOLD,
    MS_FLY_TO_B,
    MS_FLY_TO_A,
    MS_ROUND_END
} MatchState;

static MatchState state;
static Player playerA;     /* human */
static Player playerB;     /* CPU */
static Ball ball;

static u16 announceTimer;
static u16 aiDelay;
static u16 roundEndTimer;
static u8  server;          /* 0 = player serves, 1 = CPU serves */
static u8  roundWinnerIsA;

static void draw_hud(void)
{
    VDP_drawTextFill(teamNames[gTeamAIndex], 1, 1, 14);
    VDP_drawTextFill(teamNames[gTeamBIndex], 25, 1, 14);

    char buf[8];
    intToStr(gScoreA, buf, 1);
    VDP_drawTextFill(buf, 18, 1, 2);
    VDP_drawText("-", 20, 1);
    intToStr(gScoreB, buf, 1);
    VDP_drawTextFill(buf, 21, 1, 2);

    intToStr(playerA.lives, buf, 1);
    VDP_drawText("LIVES", 1, 2);
    VDP_drawTextFill(buf, 7, 2, 2);

    intToStr(playerB.lives, buf, 1);
    VDP_drawText("LIVES", 33, 2);
    VDP_drawTextFill(buf, 39, 2, 1);
}

static void begin_announce(void)
{
    const char *team = server ? teamNames[gTeamBIndex] : teamNames[gTeamAIndex];

    VDP_clearTextLine(12);
    VDP_drawTextFill(team, (40 - (strlen(team) + 7)) / 2, 12, strlen(team) + 7);
    VDP_drawText("SERVES", (40 - (strlen(team) + 7)) / 2 + strlen(team) + 1, 12);

    announceTimer = 60;
    state = MS_ANNOUNCE;

    if (server == 0)
        ball_init(&ball, SLOT_BALL, playerA.x, playerA.y - 8, BALL_HELD_A);
    else
        ball_init(&ball, SLOT_BALL, playerB.x, playerB.y + 16, BALL_HELD_B);
}

static void start_round(void)
{
    playerA.lives = START_LIVES;
    playerB.lives = START_LIVES;
    draw_hud();
    begin_announce();
}

void scene_match_enter(void)
{
    VDP_clearPlane(VDP_BG_A, TRUE);
    VDP_clearPlane(VDP_BG_B, TRUE);
    VDP_clearSprites();
    VDP_setTextPalette(PAL0);
    /* VDP_clearPlane alone can leave stale text tiles behind on a scene
     * change; explicitly clearing the text area guarantees a clean slate. */
    VDP_clearTextArea(0, 0, 40, 28);

    VDP_drawText("----------------------------------------", 0, 12);

    player_init(&playerA, SCREEN_W / 2, COURT_BOTTOM_Y, SLOT_PLAYER, PAL_TEAM_A, START_LIVES);
    player_init(&playerB, SCREEN_W / 2, COURT_TOP_Y, SLOT_CPU, PAL_TEAM_B, START_LIVES);

    server = 0;
    start_round();
}

static void go_round_end(u8 winnerIsA)
{
    roundWinnerIsA = winnerIsA;
    sound_mgr_score();

    if (winnerIsA) gScoreA++;
    else gScoreB++;

    draw_hud();

    const char *team = winnerIsA ? teamNames[gTeamAIndex] : teamNames[gTeamBIndex];
    VDP_clearTextLine(12);
    VDP_drawTextFill(team, (40 - (strlen(team) + 7)) / 2, 12, strlen(team) + 7);
    VDP_drawText("SCORES", (40 - (strlen(team) + 7)) / 2 + strlen(team) + 1, 12);

    /* loser serves next, to keep matches competitive */
    server = winnerIsA ? 1 : 0;
    roundEndTimer = 90;
    state = MS_ROUND_END;
}

void scene_match_update(void)
{
    input_mgr_update();

    switch (state)
    {
        case MS_ANNOUNCE:
        {
            player_moveHuman(&playerA);
            if (announceTimer > 0) announceTimer--;
            else
            {
                VDP_clearTextLine(12);
                VDP_drawText("----------------------------------------", 0, 12);
                if (server == 0) state = MS_PLAYER_HOLD;
                else { state = MS_CPU_HOLD; aiDelay = ai_pickThrowDelay(); }
            }
            break;
        }

        case MS_PLAYER_HOLD:
        {
            player_moveHuman(&playerA);
            ball.x = playerA.x;
            ball.y = playerA.y - 8;

            if (input_pressed(BUTTON_A))
            {
                sound_mgr_throw();
                ball_startThrow(&ball, playerA.x, playerB.y + 4, BALL_FLYING_TO_B);
                state = MS_FLY_TO_B;
            }
            break;
        }

        case MS_CPU_HOLD:
        {
            if (aiDelay > 0) aiDelay--;
            else
            {
                s16 targetX = ai_pickTargetX(playerA.x);
                sound_mgr_throw();
                ball_startThrow(&ball, targetX, playerA.y - 4, BALL_FLYING_TO_A);
                state = MS_FLY_TO_A;
            }
            break;
        }

        case MS_FLY_TO_B:
        {
            player_moveHuman(&playerA);

            /* CPU tries to get under the incoming ball */
            if (playerB.x < ball.targetX) playerB.x += PLAYER_SPEED;
            else if (playerB.x > ball.targetX) playerB.x -= PLAYER_SPEED;

            if (ball_update(&ball))
            {
                bool inRange = abs(playerB.x - ball.targetX) <= CATCH_WINDOW_X;

                if (inRange && ai_willCatch())
                {
                    sound_mgr_catch();
                    ball_init(&ball, SLOT_BALL, playerB.x, playerB.y + 16, BALL_HELD_B);
                    state = MS_CPU_HOLD;
                    aiDelay = ai_pickThrowDelay();
                }
                else
                {
                    sound_mgr_hit();
                    playerB.lives--;
                    draw_hud();

                    if (playerB.lives == 0)
                        go_round_end(TRUE);
                    else
                    {
                        ball_init(&ball, SLOT_BALL, playerA.x, playerA.y - 8, BALL_HELD_A);
                        state = MS_PLAYER_HOLD;
                    }
                }
            }
            break;
        }

        case MS_FLY_TO_A:
        {
            player_moveHuman(&playerA);

            if (ball_update(&ball))
            {
                bool inRange = abs(playerA.x - ball.targetX) <= CATCH_WINDOW_X;
                bool caught = inRange && input_held(BUTTON_A);

                if (caught)
                {
                    sound_mgr_catch();
                    ball_init(&ball, SLOT_BALL, playerA.x, playerA.y - 8, BALL_HELD_A);
                    state = MS_PLAYER_HOLD;
                }
                else
                {
                    sound_mgr_hit();
                    playerA.lives--;
                    draw_hud();

                    if (playerA.lives == 0)
                        go_round_end(FALSE);
                    else
                    {
                        ball_init(&ball, SLOT_BALL, playerB.x, playerB.y + 16, BALL_HELD_B);
                        state = MS_CPU_HOLD;
                        aiDelay = ai_pickThrowDelay();
                    }
                }
            }
            break;
        }

        case MS_ROUND_END:
        {
            if (roundEndTimer > 0) roundEndTimer--;
            else
            {
                if ((gScoreA >= WIN_SCORE) || (gScoreB >= WIN_SCORE))
                {
                    gCurrentScene = GS_GAMEOVER;
                    return;
                }

                VDP_clearTextLine(12);
                VDP_drawText("----------------------------------------", 0, 12);
                start_round();
            }
            break;
        }
    }

    player_draw(&playerA);
    player_draw(&playerB);
    ball_draw(&ball);
}
