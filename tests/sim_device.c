/* Simulated CEMS device with a line protocol on stdin/stdout.
 *
 * This wraps the firmware logic and the simulated platform in a small program
 * that behaves like a device on the end of a debug probe: a test runner sends
 * commands, the device applies them and reports state.
 *
 * The Python HIL runner drives this the same way it would drive a real board
 * over SWD/JTAG, so one set of scenarios can target either backend.
 *
 * Protocol (one command per line):
 *   RESET                 re-initialise the device            -> OK
 *   SET LATCH <0|1>       buckle switch                       -> OK
 *   SET PULL <counts>     tacho counts added per 10 ms step   -> OK
 *   SET CURRENT <amps>    motor current                       -> OK
 *   SET HAPTIC <0|1>      haptic warning button               -> OK
 *   SET MODE <OFF|SOFT|MED|HARD>  comfort mode                -> OK
 *   STEP <ms>             advance time, running the logic     -> OK
 *   GET STATE                                                 -> STATE <name>
 *   GET DUTY                                                  -> DUTY <value>
 *   QUIT                                                      -> BYE
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "cems.h"
#include "belt.h"
#include "platform.h"
#include "sim.h"

#define STEP_MS 10U

static int32_t pull_per_step;      /* tacho counts added on each step */

static const char *state_name(cems_state_t s)
{
    switch (s)
    {
        case CEMS_UNLATCHED: return "UNLATCHED";
        case CEMS_BLA:       return "BLA";
        case CEMS_BSR:       return "BSR";
        case CEMS_COMFORT:   return "COMFORT";
        case CEMS_HAPTIC:    return "HAPTIC";
        case CEMS_PARKING:   return "PARKING";
        case CEMS_FAULT:     return "FAULT";
        default:             return "UNKNOWN";
    }
}

static void advance(uint32_t ms)
{
    uint32_t t;
    for (t = 0U; t < ms; t += STEP_MS)
    {
        sim_advance_ms(STEP_MS);
        if (pull_per_step != 0)
        {
            sim_pull_webbing(pull_per_step);
        }
        cems_step();
    }
}

static void do_reset(void)
{
    sim_reset();
    cems_init();
    pull_per_step = 0;
}

int main(void)
{
    char line[128];

    do_reset();
    setvbuf(stdout, NULL, _IONBF, 0);      /* unbuffered: the runner reads live */

    while (fgets(line, (int)sizeof(line), stdin) != NULL)
    {
        char *nl = strchr(line, '\n');
        if (nl != NULL) { *nl = '\0'; }

        if (strcmp(line, "RESET") == 0)
        {
            do_reset();
            printf("OK\n");
        }
        else if (strncmp(line, "SET LATCH ", 10) == 0)
        {
            sim_set_latch((uint8_t)atoi(line + 10));
            printf("OK\n");
        }
        else if (strncmp(line, "SET PULL ", 9) == 0)
        {
            pull_per_step = (int32_t)atoi(line + 9);
            printf("OK\n");
        }
        else if (strncmp(line, "SET CURRENT ", 12) == 0)
        {
            sim_set_current((float)atof(line + 12));
            printf("OK\n");
        }
        else if (strncmp(line, "SET HAPTIC ", 11) == 0)
        {
            sim_press_haptic_button((uint8_t)atoi(line + 11));
            printf("OK\n");
        }
        else if (strncmp(line, "SET MODE ", 9) == 0)
        {
            const char *m = line + 9;
            if      (strcmp(m, "OFF")  == 0) { cems_set_comfort_mode(MODE_OFF);  }
            else if (strcmp(m, "SOFT") == 0) { cems_set_comfort_mode(MODE_SOFT); }
            else if (strcmp(m, "MED")  == 0) { cems_set_comfort_mode(MODE_MED);  }
            else if (strcmp(m, "HARD") == 0) { cems_set_comfort_mode(MODE_HARD); }
            else { printf("ERR unknown mode\n"); continue; }
            printf("OK\n");
        }
        else if (strncmp(line, "STEP ", 5) == 0)
        {
            advance((uint32_t)atoi(line + 5));
            printf("OK\n");
        }
        else if (strcmp(line, "GET STATE") == 0)
        {
            printf("STATE %s\n", state_name(cems_state()));
        }
        else if (strcmp(line, "GET DUTY") == 0)
        {
            printf("DUTY %.6f\n", (double)sim_duty_now());
        }
        else if (strcmp(line, "QUIT") == 0)
        {
            printf("BYE\n");
            break;
        }
        else if (line[0] == '\0')
        {
            /* ignore blank lines */
        }
        else
        {
            printf("ERR unknown command\n");
        }
    }

    return 0;
}
