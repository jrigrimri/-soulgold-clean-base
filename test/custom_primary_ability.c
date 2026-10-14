#include "global.h"
#include "pokemon.h"
#include "save.h"
#include "new_game.h"
#include "test/test.h"
#include "constants/abilities.h"
#include "constants/species.h"

// Phase 2 regression: collision fail-closed and New Game reset; no battle or summary changes.
// Each test starts with an empty in-memory override table.
static void SetupCustomAbilityTest(void)
{
    Save_ResetSaveCounters();
    // Full-suite tests can leave occupied party slots behind. Reset all slots
    // so duplicate-identity checks are isolated from unrelated battle tests.
    memset(gPlayerParty, 0, sizeof(gPlayerParty));
    memset(gEnemyParty, 0, sizeof(gEnemyParty));
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
    // Other tests may leave a Pokemon in party slot 1; clear it explicitly.
    memset(&gPlayerParty[1], 0, sizeof(gPlayerParty[1]));
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
    for (i = 1; i <= 5; i++)
        EXPECT(CustomAbilityStorageSet(i, 12345, ABILITY_WONDER_GUARD));
    EXPECT(!CustomAbilityStorageSet(6, 12345, ABILITY_MAGIC_GUARD));
    for (i = 1; i <= 5; i++)
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
    for (i = 1; i <= 5; i++)
        EXPECT(CustomAbilityStorageSet(i, 4321, ABILITY_WONDER_GUARD));
    EXPECT(!CustomAbilityStorageSet(6, 4321, ABILITY_MAGIC_GUARD));
    EXPECT(CustomAbilityStorageClear(2, 4321));
    EXPECT(CustomAbilityStorageSet(6, 4321, ABILITY_MAGIC_GUARD));
    EXPECT(!CustomAbilityStorageGet(2, 4321, &ability));
    EXPECT(CustomAbilityStorageGet(6, 4321, &ability));
    EXPECT_EQ(ability, ABILITY_MAGIC_GUARD);
}

TEST("Custom Primary Ability actual new game initialization clears old overrides")
{
    u16 stored = ABILITY_NONE;
    SetupCustomAbilityTest();
    EXPECT(CustomAbilityStorageSet(0x11223344, 0x55667788, ABILITY_WONDER_GUARD));
    NewGameInitData();
    EXPECT(!CustomAbilityStorageGet(0x11223344, 0x55667788, &stored));
}

TEST("Custom Primary Ability duplicate identity fails closed without changing save")
{
    enum Ability ability = ABILITY_NONE;
    u32 personality;
    u32 otId;
    SetupCustomAbilityTest();
    CreateMon(&gPlayerParty[1], SPECIES_UMBREON, 30, 0, OTID_STRUCT_PLAYER_ID);
    personality = GetMonData(&gPlayerParty[0], MON_DATA_PERSONALITY);
    otId = GetMonData(&gPlayerParty[0], MON_DATA_OT_ID);
    SetMonData(&gPlayerParty[1], MON_DATA_PERSONALITY, &personality);
    SetMonData(&gPlayerParty[1], MON_DATA_OT_ID, &otId);
    // A legacy record can already exist before a clone enters the party.
    EXPECT(CustomAbilityStorageSet(personality, otId, ABILITY_WONDER_GUARD));
    EXPECT(!TryGetCustomPrimaryAbility(&gPlayerParty[0], &ability));
    EXPECT(!TryGetCustomPrimaryAbility(&gPlayerParty[1], &ability));
    EXPECT_EQ(GetEffectivePrimaryAbility(&gPlayerParty[0]), GetMonAbility(&gPlayerParty[0]));
    EXPECT_EQ(GetEffectivePrimaryAbility(&gPlayerParty[1]), GetMonAbility(&gPlayerParty[1]));
    EXPECT(!SetCustomPrimaryAbility(&gPlayerParty[0], ABILITY_MAGIC_GUARD));
    EXPECT(!SetCustomPrimaryAbility(&gPlayerParty[1], ABILITY_MAGIC_GUARD));
    EXPECT(!ClearCustomPrimaryAbility(&gPlayerParty[1]));
    EXPECT(CustomAbilityStorageGet(personality, otId, (u16 *)&ability));
    EXPECT_EQ(ability, ABILITY_WONDER_GUARD);
    // Once the duplicate leaves the party, the existing legacy record is usable.
    memset(&gPlayerParty[1], 0, sizeof(gPlayerParty[1]));
    EXPECT(TryGetCustomPrimaryAbility(&gPlayerParty[0], &ability));
    EXPECT_EQ(ability, ABILITY_WONDER_GUARD);
}

TEST("Custom Primary Ability Phase 4 keeps Wonder Guard and Intimidate independent")
{
    enum Ability ability = ABILITY_NONE;
    u32 personalityA, personalityB;
    SetupCustomAbilityTest();
    CreateMon(&gPlayerParty[1], SPECIES_COMBUSKEN, 20, 0, OTID_STRUCT_PLAYER_ID);
    personalityA = GetMonData(&gPlayerParty[0], MON_DATA_PERSONALITY);
    personalityB = personalityA ^ 0x01000001;
    SetMonData(&gPlayerParty[1], MON_DATA_PERSONALITY, &personalityB);

    EXPECT(SetCustomPrimaryAbility(&gPlayerParty[0], ABILITY_WONDER_GUARD));
    EXPECT(SetCustomPrimaryAbility(&gPlayerParty[1], ABILITY_INTIMIDATE));
    EXPECT_EQ(GetEffectivePrimaryAbility(&gPlayerParty[0]), ABILITY_WONDER_GUARD);
    EXPECT_EQ(GetEffectivePrimaryAbility(&gPlayerParty[1]), ABILITY_INTIMIDATE);

    EXPECT(SetCustomPrimaryAbility(&gPlayerParty[0], ABILITY_MAGIC_GUARD));
    EXPECT_EQ(GetEffectivePrimaryAbility(&gPlayerParty[0]), ABILITY_MAGIC_GUARD);
    EXPECT_EQ(GetEffectivePrimaryAbility(&gPlayerParty[1]), ABILITY_INTIMIDATE);
    EXPECT(ClearCustomPrimaryAbility(&gPlayerParty[0]));
    EXPECT_EQ(GetEffectivePrimaryAbility(&gPlayerParty[0]), GetMonAbility(&gPlayerParty[0]));
    EXPECT(TryGetCustomPrimaryAbility(&gPlayerParty[1], &ability));
    EXPECT_EQ(ability, ABILITY_INTIMIDATE);
}

TEST("Custom Primary Ability Phase 4 refuses enemy identity even when player has Intimidate")
{
    u32 pid, otId;
    SetupCustomAbilityTest();
    pid = GetMonData(&gPlayerParty[0], MON_DATA_PERSONALITY);
    otId = GetMonData(&gPlayerParty[0], MON_DATA_OT_ID);
    EXPECT(SetCustomPrimaryAbility(&gPlayerParty[0], ABILITY_INTIMIDATE));
    CreateMon(&gEnemyParty[0], SPECIES_UMBREON, 30, 0, OTID_STRUCT_PLAYER_ID);
    SetMonData(&gEnemyParty[0], MON_DATA_PERSONALITY, &pid);
    SetMonData(&gEnemyParty[0], MON_DATA_OT_ID, &otId);
    EXPECT_EQ(GetEffectivePrimaryAbility(&gEnemyParty[0]), GetMonAbility(&gEnemyParty[0]));
    EXPECT_EQ(GetEffectivePrimaryAbility(&gPlayerParty[0]), ABILITY_INTIMIDATE);
}
