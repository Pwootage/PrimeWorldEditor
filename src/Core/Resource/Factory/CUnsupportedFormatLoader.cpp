#include "CUnsupportedFormatLoader.h"
#include "Core/Resource/ParticleParameters.h"

CDependencyGroup* CUnsupportedFormatLoader::LoadCSNG(IInputStream& rCSNG, CResourceEntry *pEntry)
{
    u32 Magic = rCSNG.ReadLong();
    ASSERT(Magic == 0x2);
    rCSNG.Seek(0x8, SEEK_CUR);

    CDependencyGroup *pGroup = new CDependencyGroup(pEntry);
    pGroup->AddDependency(rCSNG.ReadLong());
    return pGroup;
}

CDependencyGroup* CUnsupportedFormatLoader::LoadEVNT(IInputStream& rEVNT, CResourceEntry *pEntry)
{
    u32 Version = rEVNT.ReadLong();
    ASSERT(Version == 1 || Version == 2);

    CDependencyGroup *pGroup = new CDependencyGroup(pEntry);

    // Loop Events
    u32 NumLoopEvents = rEVNT.ReadLong();

    for (u32 iLoop = 0; iLoop < NumLoopEvents; iLoop++)
    {
        rEVNT.Seek(0x2, SEEK_CUR);
        rEVNT.ReadString();
        rEVNT.Seek(0x1C, SEEK_CUR);
    }

    // User Events
    u32 NumUserEvents = rEVNT.ReadLong();

    for (u32 iUser = 0; iUser < NumUserEvents; iUser++)
    {
        rEVNT.Seek(0x2, SEEK_CUR);
        rEVNT.ReadString();
        rEVNT.Seek(0x1F, SEEK_CUR);
        rEVNT.ReadString();
    }

    // Effect Events
    u32 NumEffectEvents = rEVNT.ReadLong();

    for (u32 iFX = 0; iFX < NumEffectEvents; iFX++)
    {
        rEVNT.Seek(0x2, SEEK_CUR);
        rEVNT.ReadString();
        rEVNT.Seek(0x23, SEEK_CUR);
        pGroup->AddDependency(rEVNT.ReadLong());
        rEVNT.ReadString();
        rEVNT.Seek(0x8, SEEK_CUR);
    }

    return pGroup;
}

CDependencyGroup* CUnsupportedFormatLoader::LoadHINT(IInputStream& rHINT, CResourceEntry *pEntry)
{
    u32 Magic = rHINT.ReadLong();
    ASSERT(Magic == 0x00BADBAD);

    // Determine version
    u32 Version = rHINT.ReadLong();
    EGame Game;

    if (Version == 0x1) Game = ePrime;
    else if (Version == 0x3) Game = eCorruption;

    else
    {
        Log::Error("Unrecognized HINT version: " + TString::FromInt32(Version, 0, 10));
        return nullptr;
    }

    EIDLength IDLength = (Game <= eEchoes ? e32Bit : e64Bit);

    // Read main file
    CDependencyGroup *pGroup = new CDependencyGroup(pEntry);
    u32 NumHints = rHINT.ReadLong();

    for (u32 iHint = 0; iHint < NumHints; iHint++)
    {
        rHINT.ReadString(); // Skip hint name
        rHINT.Seek(0x8, SEEK_CUR); // Skip unknown + appear time
        pGroup->AddDependency( CAssetID(rHINT, IDLength) ); // Pop-up STRG
        rHINT.Seek(0x8, SEEK_CUR); // Skip unknowns

        if (Game <= eEchoes)
        {
            pGroup->AddDependency( CAssetID(rHINT, IDLength) ); // Target MLVL
            pGroup->AddDependency( CAssetID(rHINT, IDLength) ); // Target MREA
            rHINT.Seek(0x4, SEEK_CUR); // Skip target room index
            pGroup->AddDependency( CAssetID(rHINT, IDLength) ); // Map STRG
        }
    }

    return pGroup;
}

CDependencyGroup* CUnsupportedFormatLoader::LoadMAPW(IInputStream& rMAPW, CResourceEntry *pEntry)
{
    u32 Magic = rMAPW.ReadLong();
    ASSERT(Magic == 0xDEADF00D);

    u32 Version = rMAPW.ReadLong();
    ASSERT(Version == 1);

    u32 NumAreas = rMAPW.ReadLong();

    // Version check
    u32 AreasStart = rMAPW.Tell();
    rMAPW.Seek(NumAreas * 4, SEEK_CUR);
    EIDLength IDLength = (rMAPW.EoF() || rMAPW.ReadLong() == 0xFFFFFFFF ? e32Bit : e64Bit);
    rMAPW.Seek(AreasStart, SEEK_SET);

    // Read MAPA IDs
    CDependencyGroup *pGroup = new CDependencyGroup(pEntry);

    for (u32 iArea = 0; iArea < NumAreas; iArea++)
        pGroup->AddDependency( CAssetID(rMAPW, IDLength) );

    return pGroup;
}

CDependencyGroup* CUnsupportedFormatLoader::LoadMAPU(IInputStream& rMAPU, CResourceEntry *pEntry)
{
    u32 Magic = rMAPU.ReadLong();
    ASSERT(Magic == 0xABCDEF01);

    u32 Version = rMAPU.ReadLong();
    ASSERT(Version == 0x1);

    CDependencyGroup *pGroup = new CDependencyGroup(pEntry);
    pGroup->AddDependency(rMAPU.ReadLong());

    // Read worlds
    u32 NumWorlds = rMAPU.ReadLong();

    for (u32 iWorld = 0; iWorld < NumWorlds; iWorld++)
    {
        rMAPU.ReadString(); // Skip world name
        pGroup->AddDependency(rMAPU.ReadLong()); // World MLVL
        rMAPU.Seek(0x30, SEEK_CUR); // Skip world map transform
        u32 NumHexagons = rMAPU.ReadLong();
        rMAPU.Seek(NumHexagons * 0x30, SEEK_CUR); // Skip hexagon transforms
        rMAPU.Seek(0x10, SEEK_CUR); // Skip world color
    }

    return pGroup;
}

CDependencyGroup* CUnsupportedFormatLoader::LoadRULE(IInputStream& rRULE, CResourceEntry *pEntry)
{
    // RULE files can contain a reference to another RULE file, but has no other dependencies.
    u32 Magic = rRULE.ReadLong();
    ASSERT(CFourCC(Magic) == "RULE");

    CDependencyGroup *pGroup = new CDependencyGroup(pEntry);
    rRULE.Seek(0x1, SEEK_CUR);

    // Version test
    u32 IDOffset = rRULE.Tell();
    rRULE.Seek(0x4, SEEK_CUR);
    u32 RuleSetCount = rRULE.ReadLong();
    EIDLength IDLength = (RuleSetCount > 0xFF ? e64Bit : e32Bit);
    rRULE.Seek(IDOffset, SEEK_SET);

    // Read rule ID
    CAssetID RuleID(rRULE, IDLength);
    pGroup->AddDependency(RuleID);
    return pGroup;
}
