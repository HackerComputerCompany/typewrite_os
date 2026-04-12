/**
 * Test sound engine - replicates the crash
 */
#include "linux-typewrite-x11/src/tw_sound.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

int main(int argc, char **argv) {
    printf("=== Sound Test ===\n");

    printf("1. Initializing sound (no external path)...\n");
    bool init_result = TwSoundInit(NULL);
    printf("   init_result=%d\n", init_result);

    printf("2. Calling TwSoundInit again (simulating --sounds option)...\n");
    TwSoundSetBasePath("/some/path");
    init_result = TwSoundInit("sounds.assets");
    printf("   init_result=%d\n", init_result);

    printf("3. Before playing - checking gSound state...\n");
    // Access internal state via public API - just try to play
    printf("   About to play sound for font 0...\n");
    
    printf("4. Playing sound for font 0...\n");
    bool result = TwPlaySoundForFont(0);
    printf("   result=%d\n", result);

    printf("=== Test Complete ===\n");
    return 0;
}
