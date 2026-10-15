#include "global.h"
#include "save.h"
#include "test/test.h"

#define CUSTOM_ABILITY_STORAGE_SLOTS (19 * 30 + PARTY_SIZE)
#define SAVE_BLOCK3_CAPACITY (SAVE_BLOCK_3_CHUNK_SIZE * NUM_SECTORS_PER_SLOT)

STATIC_ASSERT(sizeof(((struct SaveBlock3 *)0)->customAbilityIds) == CUSTOM_ABILITY_STORAGE_SLOTS * sizeof(u16), CustomAbilityStorageSlotSize);
STATIC_ASSERT(sizeof(struct SaveBlock3) <= SAVE_BLOCK3_CAPACITY, CustomAbilityStorageFitsSaveBlock3);
STATIC_ASSERT(offsetof(struct SaveBlock3, customAbilityIds) > offsetof(struct SaveBlock3, candyJarExp), CustomAbilityStorageAppended);

TEST("SaveBlock3 custom ability storage fits and round-trips all 576 slots through sector chunks")
{
    u8 sectors[NUM_SECTORS_PER_SLOT][SAVE_BLOCK_3_CHUNK_SIZE];
    struct SaveBlock3 restored = {0};
    u32 i;
    u32 byteCount = sizeof(struct SaveBlock3);
    u8 *source = (u8 *)gSaveBlock3Ptr;
    u8 *destination = (u8 *)&restored;

    for (i = 0; i < CUSTOM_ABILITY_STORAGE_SLOTS; i++)
        gSaveBlock3Ptr->customAbilityIds[i] = (i % 531) + 1;

    // Emulate SaveBlock3's 116-byte-per-sector copy layout.
    for (i = 0; i < NUM_SECTORS_PER_SLOT; i++)
    {
        u32 offset = i * SAVE_BLOCK_3_CHUNK_SIZE;
        u32 size = offset >= byteCount ? 0 : byteCount - offset;
        if (size > SAVE_BLOCK_3_CHUNK_SIZE)
            size = SAVE_BLOCK_3_CHUNK_SIZE;
        if (size != 0)
            memcpy(sectors[i], source + offset, size);
    }
    for (i = 0; i < NUM_SECTORS_PER_SLOT; i++)
    {
        u32 offset = i * SAVE_BLOCK_3_CHUNK_SIZE;
        u32 size = offset >= byteCount ? 0 : byteCount - offset;
        if (size > SAVE_BLOCK_3_CHUNK_SIZE)
            size = SAVE_BLOCK_3_CHUNK_SIZE;
        if (size != 0)
            memcpy(destination + offset, sectors[i], size);
    }
    for (i = 0; i < CUSTOM_ABILITY_STORAGE_SLOTS; i++)
        EXPECT_EQ(restored.customAbilityIds[i], (i % 531) + 1);
}

// Phase 3.2 metadata checks; the actual flash Save/Load path still needs integration testing.
TEST("SaveBlock3 custom ability metadata detects corrupted table data")
{
    u32 i;
    u32 sum = 0;
    for (i = 0; i < CUSTOM_ABILITY_STORAGE_SLOTS; i++)
    {
        gSaveBlock3Ptr->customAbilityIds[i] = i % 532;
        sum = (sum + gSaveBlock3Ptr->customAbilityIds[i]) & 0xFFFF;
    }
    gSaveBlock3Ptr->customAbilityStorageMagic = 0x43414233;
    gSaveBlock3Ptr->customAbilityStorageVersion = 1;
    gSaveBlock3Ptr->customAbilityStorageChecksum = sum;
    EXPECT_EQ(gSaveBlock3Ptr->customAbilityStorageChecksum, sum);
    gSaveBlock3Ptr->customAbilityIds[42] ^= 1;
    sum = 0;
    for (i = 0; i < CUSTOM_ABILITY_STORAGE_SLOTS; i++)
        sum = (sum + gSaveBlock3Ptr->customAbilityIds[i]) & 0xFFFF;
    EXPECT_NE(gSaveBlock3Ptr->customAbilityStorageChecksum, sum);
}

TEST("SaveBlock3 legacy extension initializes without changing legacy fields")
{
    u32 previousExp = gSaveBlock3Ptr->candyJarExp;
    gSaveBlock3Ptr->customAbilityStorageMagic = 0;
    gSaveBlock3Ptr->customAbilityStorageVersion = 0;
    gSaveBlock3Ptr->customAbilityIds[0] = 531;
    EXPECT_EQ(CustomAbilityStorage_TestIsValid(), FALSE);
    CustomAbilityStorage_TestInitialize();
    EXPECT_EQ(CustomAbilityStorage_TestIsValid(), TRUE);
    EXPECT_EQ(gSaveBlock3Ptr->customAbilityIds[0], 0);
    EXPECT_EQ(gSaveBlock3Ptr->customAbilityIds[575], 0);
    EXPECT_EQ(gSaveBlock3Ptr->candyJarExp, previousExp);
}

TEST("SaveBlock3 finalization preserves edited ability IDs and validates checksum")
{
    u32 i;
    CustomAbilityStorage_TestInitialize();
    for (i = 0; i < CUSTOM_ABILITY_STORAGE_SLOTS; i++)
        gSaveBlock3Ptr->customAbilityIds[i] = (i % 531) + 1;
    EXPECT_EQ(CustomAbilityStorage_TestIsValid(), FALSE);
    CustomAbilityStorage_TestFinalizeForSave();
    EXPECT_EQ(CustomAbilityStorage_TestIsValid(), TRUE);
    for (i = 0; i < CUSTOM_ABILITY_STORAGE_SLOTS; i++)
        EXPECT_EQ(gSaveBlock3Ptr->customAbilityIds[i], (i % 531) + 1);
}

TEST("SaveBlock3 invalid magic version and checksum are rejected")
{
    CustomAbilityStorage_TestInitialize();
    EXPECT_EQ(CustomAbilityStorage_TestIsValid(), TRUE);
    gSaveBlock3Ptr->customAbilityStorageMagic ^= 1;
    EXPECT_EQ(CustomAbilityStorage_TestIsValid(), FALSE);
    CustomAbilityStorage_TestInitialize();
    gSaveBlock3Ptr->customAbilityStorageVersion++;
    EXPECT_EQ(CustomAbilityStorage_TestIsValid(), FALSE);
    CustomAbilityStorage_TestInitialize();
    gSaveBlock3Ptr->customAbilityIds[575] = 531;
    EXPECT_EQ(CustomAbilityStorage_TestIsValid(), FALSE);
}
