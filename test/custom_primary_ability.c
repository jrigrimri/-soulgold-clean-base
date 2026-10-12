#include "global.h"
#include "pokemon.h"
#include "save.h"
#include "test/test.h"
#include "constants/abilities.h"
#include "constants/species.h"

// Phase 2: exercise the API without modifying battle or summary behavior.
// Each test starts with an empty in-memory override table.
static void SetupCustomAbilityTest(void)
{
    Save_ResetSaveCounters();
    CreateMon(&gPlayerParty[0], SPECIES_UMBREON, 30, 0, OTID_STRUCT_PLAYER_ID);
}

TEST("Custom Primary Ability falls back to species when no override exists")
{
    enum Ability normal;
    enum Ability resolved;
    SetupCustomAbilityTest();
    normal = GetMonAbility(&gPlayerParty[0]);
    resolved = GetEffectivePrimaryAbility(&gPlayerParty[0]);
    EXPECT_EQ(resolved, normal);
}

TEST("Custom Primary Ability sets reads and clears Wonder Guard")
{
    enum Ability ability = ABILITY_NONE;
    SetupCustomAbilityTest();
    EXPECT(SetCustomPrimaryAbility(&gPlayerParty[0], ABILITY_WONDER_GUARD));
    EXPECT(TryGetCustomPrimaryAbility(&gPlayerParty[0], &ability));
    EXPECT_EQ(ability, ABILITY_WONDER_GUARD);
    EXPECT_EQ(GetEffectivePrimaryAbility(&gPlayerParty[0]), ABILITY_WONDER_GUARD);
    EXPECT(ClearCustomPrimaryAbility(&gPlayerParty[0]));
    EXPECT(!TryGetCustomPrimaryAbility(&gPlayerParty[0], &ability));
    EXPECT_EQ(GetEffectivePrimaryAbility(&gPlayerParty[0]), GetMonAbility(&gPlayerParty[0]));
}

TEST("Custom Primary Ability rejects invalid IDs")
{
    SetupCustomAbilityTest();
    EXPECT(!SetCustomPrimaryAbility(&gPlayerParty[0], ABILITY_NONE));
    EXPECT(!SetCustomPrimaryAbility(&gPlayerParty[0], ABILITIES_COUNT));
    EXPECT_EQ(GetEffectivePrimaryAbility(&gPlayerParty[0]), GetMonAbility(&gPlayerParty[0]));
}

TEST("Custom Primary Ability never changes an enemy Pokemon")
{
    enum Ability ability = ABILITY_NONE;
    SetupCustomAbilityTest();
    CreateMon(&gEnemyParty[0], SPECIES_UMBREON, 30, 0, OTID_STRUCT_PLAYER_ID);
    EXPECT(!SetCustomPrimaryAbility(&gEnemyParty[0], ABILITY_WONDER_GUARD));
    EXPECT(!TryGetCustomPrimaryAbility(&gEnemyParty[0], &ability));
    EXPECT_EQ(GetEffectivePrimaryAbility(&gEnemyParty[0]), GetMonAbility(&gEnemyParty[0]));
}

TEST("Custom Primary Ability rejects eggs and empty party slots")
{
    bool8 egg = TRUE;
    SetupCustomAbilityTest();
    EXPECT(!SetCustomPrimaryAbility(&gPlayerParty[1], ABILITY_WONDER_GUARD));
    SetMonData(&gPlayerParty[0], MON_DATA_IS_EGG, &egg);
    EXPECT(!SetCustomPrimaryAbility(&gPlayerParty[0], ABILITY_WONDER_GUARD));
}

TEST("Custom Primary Ability storage updates without duplicate entries")
{
    u32 pid;
    u32 otId;
    u16 stored = ABILITY_NONE;
    SetupCustomAbilityTest();
    pid = GetMonData(&gPlayerParty[0], MON_DATA_PERSONALITY);
    otId = GetMonData(&gPlayerParty[0], MON_DATA_OT_ID);
    EXPECT(CustomAbilityStorageSet(pid, otId, ABILITY_WONDER_GUARD));
    EXPECT(CustomAbilityStorageSet(pid, otId, ABILITY_MAGIC_GUARD));
    EXPECT(CustomAbilityStorageGet(pid, otId, &stored));
    EXPECT_EQ(stored, ABILITY_MAGIC_GUARD);
    EXPECT(CustomAbilityStorageClear(pid, otId));
    EXPECT(!CustomAbilityStorageGet(pid, otId, &stored));
}

TEST("Custom Primary Ability storage rejects overflow without evicting")
{
    u32 i;
    u16 stored = ABILITY_NONE;
    SetupCustomAbilityTest();
    for (i = 1; i <= 4; i++)
        EXPECT(CustomAbilityStorageSet(i, 12345, ABILITY_WONDER_GUARD));
    EXPECT(!CustomAbilityStorageSet(5, 12345, ABILITY_MAGIC_GUARD));
    for (i = 1; i <= 4; i++)
    {
        EXPECT(CustomAbilityStorageGet(i, 12345, &stored));
        EXPECT_EQ(stored, ABILITY_WONDER_GUARD);
    }
}

TEST("Custom Primary Ability reset clears in-memory storage")
{
    u16 stored = ABILITY_NONE;
    SetupCustomAbilityTest();
    EXPECT(CustomAbilityStorageSet(123, 456, ABILITY_WONDER_GUARD));
    Save_ResetSaveCounters();
    EXPECT(!CustomAbilityStorageGet(123, 456, &stored));
}

TEST("Custom Primary Ability reset prevents stale identity reuse")
{
    u32 pid;
    u32 otId;
    u16 stored = ABILITY_NONE;
    SetupCustomAbilityTest();
    pid = GetMonData(&gPlayerParty[0], MON_DATA_PERSONALITY);
    otId = GetMonData(&gPlayerParty[0], MON_DATA_OT_ID);
    EXPECT(SetCustomPrimaryAbility(&gPlayerParty[0], ABILITY_WONDER_GUARD));
    EXPECT_EQ(GetEffectivePrimaryAbility(&gPlayerParty[0]), ABILITY_WONDER_GUARD);
    // New-game reset must clear the previous save's in-memory override table.
    Save_ResetSaveCounters();
    CreateMon(&gPlayerParty[0], SPECIES_UMBREON, 30, 0, OTID_STRUCT_PLAYER_ID);
    EXPECT(!CustomAbilityStorageGet(pid, otId, &stored));
    EXPECT_EQ(GetEffectivePrimaryAbility(&gPlayerParty[0]), GetMonAbility(&gPlayerParty[0]));
}

TEST("Custom Primary Ability storage capacity recovers after clearing")
{
    u32 i;
    u16 ability = ABILITY_NONE;
    SetupCustomAbilityTest();
    for (i = 1; i <= 4; i++)
        EXPECT(CustomAbilityStorageSet(i, 4321, ABILITY_WONDER_GUARD));
    EXPECT(!CustomAbilityStorageSet(5, 4321, ABILITY_MAGIC_GUARD));
    EXPECT(CustomAbilityStorageClear(2, 4321));
    EXPECT(CustomAbilityStorageSet(5, 4321, ABILITY_MAGIC_GUARD));
    EXPECT(!CustomAbilityStorageGet(2, 4321, &ability));
    EXPECT(CustomAbilityStorageGet(5, 4321, &ability));
    EXPECT_EQ(ability, ABILITY_MAGIC_GUARD);
}
