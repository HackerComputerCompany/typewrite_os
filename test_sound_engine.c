/**
 * Unit tests for Typewrite OS sound engine
 */

#include "linux-typewrite-x11/src/tw_sound.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  Running %s... ", #name); \
    test_##name(); \
    printf("PASSED\n"); \
    tests_passed++; \
} while(0)

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("FAILED: %s\n", msg); \
        tests_failed++; \
        return; \
    } \
} while(0)

TEST(sound_init_no_assets) {
    bool result = TwSoundInit(NULL);
    ASSERT(result == true, "init should succeed");
    TwSoundShutdown();
}

TEST(sound_init_twice) {
    TwSoundInit(NULL);
    bool second = TwSoundInit(NULL);
    ASSERT(second == true, "second init should return true (already initialized)");
    TwSoundShutdown();
}

TEST(sound_for_font_mapping) {
    TwSoundInit(NULL);
    
    TwSoundId id = TwSoundForFont(0);
    ASSERT(id == SOUND_VIRGIL_PENCIL, "font 0 should map to virgil pencil");
    
    id = TwSoundForFont(1);
    ASSERT(id == SOUND_UI_TAP, "font 1 should map to ui tap");
    
    id = TwSoundForFont(2);
    ASSERT(id == SOUND_TYPEWRITER_KEY, "font 2 should map to typewriter key");
    
    id = TwSoundForFont(4);
    ASSERT(id == SOUND_TERMINAL_BLIP, "font 4 (VT323) should map to terminal blip");
    
    id = TwSoundForFont(6);
    ASSERT(id == SOUND_IBM_KEYBOARD, "font 6 (IBM Plex) should map to ibm keyboard");
    
    id = TwSoundForFont(7);
    ASSERT(id == SOUND_ARCADE_BLIP, "font 7 (Press Start 2P) should map to arcade blip");
    
    TwSoundShutdown();
}

TEST(sound_play_invalid) {
    TwSoundInit(NULL);
    
    bool result = TwPlaySound(SOUND_NONE);
    ASSERT(result == false, "SOUND_NONE should fail");
    
    TwSoundShutdown();
}

TEST(sound_carriage_and_bell) {
    TwSoundInit(NULL);
    
    TwSoundId carriage = TwSoundForFontCarriage(2);
    ASSERT(carriage == SOUND_TYPEWRITER_CARRIAGE, "font 2 (Special Elite) should have carriage return");
    
    TwSoundId bell = TwSoundForFontBell(2);
    ASSERT(bell == SOUND_TYPEWRITER_BELL, "font 2 (Special Elite) should have bell");
    
    TwSoundShutdown();
}

TEST(sound_set_base_path) {
    TwSoundSetBasePath("/tmp/test");
    TwSoundInit(NULL);
    TwSoundShutdown();
    
    TwSoundSetBasePath(NULL);
    TwSoundInit(NULL);
    TwSoundShutdown();
}

TEST(sound_id_enum) {
    ASSERT(SOUND_NONE == 0, "SOUND_NONE should be 0");
    ASSERT(SOUND_TYPEWRITER_KEY == 1, "SOUND_TYPEWRITER_KEY should be 1");
    ASSERT(SOUND_COUNT > SOUND_SIMPLE_BLIP, "SOUND_COUNT should be greater than last sound");
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    
    printf("=== Sound Engine Unit Tests ===\n");
    
    RUN_TEST(sound_init_no_assets);
    RUN_TEST(sound_init_twice);
    RUN_TEST(sound_for_font_mapping);
    RUN_TEST(sound_play_invalid);
    RUN_TEST(sound_carriage_and_bell);
    RUN_TEST(sound_set_base_path);
    RUN_TEST(sound_id_enum);
    
    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}