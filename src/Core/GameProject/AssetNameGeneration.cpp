#include "AssetNameGeneration.h"
#include "CGameProject.h"
#include "CResourceIterator.h"
#include "Core/Resource/CFont.h"
#include "Core/Resource/CScan.h"
#include "Core/Resource/CWorld.h"
#include "Core/Resource/Script/CScriptLayer.h"
#include <Math/MathUtil.h>

#define PROCESS_WORLDS 1
#define PROCESS_AREAS 1
#define PROCESS_MODELS 1
#define PROCESS_AUDIO_GROUPS 1
#define PROCESS_ANIM_CHAR_SETS 1
#define PROCESS_STRINGS 1
#define PROCESS_SCANS 1
#define PROCESS_FONTS 1

void ApplyGeneratedName(CResourceEntry *pEntry, const TWideString& rkDir, const TWideString& rkName)
{
    TWideString SanitizedName = FileUtil::SanitizeName(rkName, false);
    TWideString SanitizedDir = FileUtil::SanitizePath(rkDir, true);
    if (SanitizedName.IsEmpty()) return;

    CVirtualDirectory *pNewDir = pEntry->ResourceStore()->GetVirtualDirectory(SanitizedDir, false, true);
    if (pEntry->Directory() == pNewDir && pEntry->Name() == SanitizedName) return;

    TWideString Name = SanitizedName;
    int AppendNum = 0;

    while (pNewDir->FindChildResource(Name, pEntry->ResourceType()) != nullptr)
    {
        Name = TWideString::Format(L"%s_%d", *SanitizedName, AppendNum);
        AppendNum++;
    }

    bool Success = pEntry->Move(SanitizedDir, Name);
    ASSERT(Success);
}

TWideString MakeWorldName(EGame Game, TWideString RawName)
{
    // MP1 demo - Remove ! from the beginning
    if (Game == ePrimeDemo)
    {
        if (RawName.StartsWith(L'!'))
            RawName = RawName.ChopFront(1);
    }

    // MP1 - Remove prefix characters and ending date
    else if (Game == ePrime)
    {
        RawName = RawName.ChopFront(2);
        bool StartedDate = false;

        while (!RawName.IsEmpty())
        {
            wchar_t Chr = RawName.Back();

            if (!StartedDate && Chr >= L'0' && Chr <= L'9')
                StartedDate = true;
            else if (StartedDate && Chr != L'_' && (Chr < L'0' || Chr > L'9'))
                break;

            RawName = RawName.ChopBack(1);
        }
    }

    // MP2 demo - Use text between the first and second underscores
    else if (Game == eEchoesDemo)
    {
        u32 UnderscoreA = RawName.IndexOf(L'_');
        u32 UnderscoreB = RawName.IndexOf(L'_', UnderscoreA + 1);
        RawName = RawName.SubString(UnderscoreA + 1, UnderscoreB - UnderscoreA - 1);
    }

    // MP3 proto - Remove ! from the beginning and all text after last underscore
    else if (Game == eCorruptionProto)
    {
        if (RawName.StartsWith(L'!'))
            RawName = RawName.ChopFront(1);

        u32 LastUnderscore = RawName.LastIndexOf(L"_");
        RawName = RawName.ChopBack(RawName.Size() - LastUnderscore);
    }

    // MP2/3 - Remove text before first underscore and after last underscore, strip remaining underscores (except multiplayer maps, which have one underscore)
    else if (Game == eEchoes || Game == eCorruption)
    {
        u32 FirstUnderscore = RawName.IndexOf(L'_');
        u32 LastUnderscore = RawName.LastIndexOf(L"_");

        if (FirstUnderscore != LastUnderscore)
        {
            RawName = RawName.ChopBack(RawName.Size() - LastUnderscore);
            RawName = RawName.ChopFront(FirstUnderscore + 1);
        }

        RawName.Remove(L'_');
    }

    // DKCR - Remove text after second-to-last underscore
    else if (Game == eReturns)
    {
        u32 Underscore = RawName.LastIndexOf(L"_");
        RawName = RawName.ChopBack(RawName.Size() - Underscore);
        Underscore = RawName.LastIndexOf(L"_");
        RawName = RawName.ChopBack(RawName.Size() - Underscore);
    }

    return RawName;
}

void GenerateAssetNames(CGameProject *pProj)
{
    // todo: CAUD/CSMP
    CResourceStore *pStore = pProj->ResourceStore();

    // Generate names for package named resources first
    for (u32 iPkg = 0; iPkg < pProj->NumPackages(); iPkg++)
    {
        CPackage *pPkg = pProj->PackageByIndex(iPkg);

        for (u32 iCol = 0; iCol < pPkg->NumCollections(); iCol++)
        {
            CResourceCollection *pCol = pPkg->CollectionByIndex(iCol);

            for (u32 iRes = 0; iRes < pCol->NumResources(); iRes++)
            {
                const SNamedResource& rkRes = pCol->ResourceByIndex(iRes);
                if (rkRes.Name.EndsWith("NODEPEND")) continue;

                CResourceEntry *pRes = pStore->FindEntry(rkRes.ID);
                ApplyGeneratedName(pRes, pPkg->Name().ToUTF16(), rkRes.Name.ToUTF16());
            }
        }
    }

#if PROCESS_WORLDS
    // Generate world/area names
    const TWideString kWorldsRoot = L"Worlds\\";

    for (TResourceIterator<eWorld> It(pStore); It; ++It)
    {
        // Generate world name
        TWideString WorldName = MakeWorldName(pProj->Game(), It->Name());
        TWideString WorldDir = kWorldsRoot + WorldName + L'\\';

        TWideString WorldMasterName = L"!" + WorldName + L"_Master";
        TWideString WorldMasterDir = WorldDir + WorldMasterName + L'\\';
        ApplyGeneratedName(*It, WorldMasterDir, WorldMasterName);

        // Move world stuff
        const TWideString WorldNamesDir = L"Strings\\Worlds\\General\\";
        const TWideString AreaNamesDir = TWideString::Format(L"Strings\\Worlds\\%s\\", *WorldName);

        CWorld *pWorld = (CWorld*) It->Load();
        CModel *pSkyModel = pWorld->DefaultSkybox();
        CStringTable *pWorldNameTable = pWorld->WorldName();
        CStringTable *pDarkWorldNameTable = pWorld->DarkWorldName();
        CResource *pSaveWorld = pWorld->SaveWorld();
        CResource *pMapWorld = pWorld->MapWorld();

        if (pSaveWorld)
            ApplyGeneratedName(pSaveWorld->Entry(), WorldMasterDir, WorldMasterName);

        if (pMapWorld)
            ApplyGeneratedName(pMapWorld->Entry(), WorldMasterDir, WorldMasterName);

        if (pSkyModel && !pSkyModel->Entry()->IsCategorized())
        {
            // Move sky model
            CResourceEntry *pSkyEntry = pSkyModel->Entry();
            ApplyGeneratedName(pSkyEntry, WorldDir + L"sky\\cooked\\", L"sky");

            // Move sky textures
            for (u32 iSet = 0; iSet < pSkyModel->GetMatSetCount(); iSet++)
            {
                CMaterialSet *pSet = pSkyModel->GetMatSet(iSet);

                for (u32 iMat = 0; iMat < pSet->NumMaterials(); iMat++)
                {
                    CMaterial *pMat = pSet->MaterialByIndex(iMat);

                    for (u32 iPass = 0; iPass < pMat->PassCount(); iPass++)
                    {
                        CMaterialPass *pPass = pMat->Pass(iPass);

                        if (pPass->Texture() && !pPass->Texture()->Entry()->IsCategorized())
                            ApplyGeneratedName(pPass->Texture()->Entry(), WorldDir + L"sky\\sourceimages\\", pPass->Texture()->Entry()->Name());
                    }
                }
            }
        }

        if (pWorldNameTable)
        {
            CResourceEntry *pNameEntry = pWorldNameTable->Entry();
            ApplyGeneratedName(pNameEntry, WorldNamesDir, WorldName);
        }

        if (pDarkWorldNameTable)
        {
            CResourceEntry *pDarkNameEntry = pDarkWorldNameTable->Entry();
            ApplyGeneratedName(pDarkNameEntry, WorldNamesDir, WorldName + L"Dark");
        }

        // Areas
        for (u32 iArea = 0; iArea < pWorld->NumAreas(); iArea++)
        {
            // Determine area name
            TWideString AreaName = pWorld->AreaInternalName(iArea).ToUTF16();
            CAssetID AreaID = pWorld->AreaResourceID(iArea);

            if (AreaName.IsEmpty())
                AreaName = AreaID.ToString().ToUTF16();

            // Rename area stuff
            CResourceEntry *pAreaEntry = pStore->FindEntry(AreaID);
            ASSERT(pAreaEntry != nullptr);
            ApplyGeneratedName(pAreaEntry, WorldMasterDir, AreaName);

            CStringTable *pAreaNameTable = pWorld->AreaName(iArea);
            if (pAreaNameTable)
                ApplyGeneratedName(pAreaNameTable->Entry(), AreaNamesDir, AreaName);

            if (pMapWorld)
            {
                ASSERT(pMapWorld->Type() == eDependencyGroup);
                CDependencyGroup *pGroup = static_cast<CDependencyGroup*>(pMapWorld);
                CAssetID MapID = pGroup->DependencyByIndex(iArea);
                CResourceEntry *pMapEntry = pStore->FindEntry(MapID);
                ASSERT(pMapEntry != nullptr);

                ApplyGeneratedName(pMapEntry, WorldMasterDir, AreaName);
            }

#if PROCESS_AREAS
            // Move area dependencies
            TWideString AreaCookedDir = WorldDir + AreaName + L"\\cooked\\";
            CGameArea *pArea = (CGameArea*) pAreaEntry->Load();

            // Area lightmaps
            u32 LightmapNum = 0;
            CMaterialSet *pMaterials = pArea->Materials();

            for (u32 iMat = 0; iMat < pMaterials->NumMaterials(); iMat++)
            {
                CMaterial *pMat = pMaterials->MaterialByIndex(iMat);

                if (pMat->Options().HasFlag(CMaterial::eLightmap))
                {
                    CTexture *pLightmapTex = pMat->Pass(0)->Texture();
                    CResourceEntry *pTexEntry = pLightmapTex->Entry();
                    if (pTexEntry->IsCategorized()) continue;

                    TWideString TexName = TWideString::Format(L"%s_lit_lightmap%d", *AreaName, LightmapNum);
                    ApplyGeneratedName(pTexEntry, AreaCookedDir, TexName);
                    pTexEntry->SetHidden(true);
                    LightmapNum++;
                }
            }

            // Generate names from script instance names
            for (u32 iLyr = 0; iLyr < pArea->NumScriptLayers(); iLyr++)
            {
                CScriptLayer *pLayer = pArea->ScriptLayer(iLyr);

                for (u32 iInst = 0; iInst < pLayer->NumInstances(); iInst++)
                {
                    CScriptObject *pInst = pLayer->InstanceByIndex(iInst);

                    if (pInst->ObjectTypeID() == 0x42 || pInst->ObjectTypeID() == FOURCC("POIN"))
                    {
                        TString Name = pInst->InstanceName();

                        if (Name.EndsWith(".SCAN", false))
                        {
                            TIDString ScanIDString = (pProj->Game() <= ePrime ? "0x4:0x0" : "0xBDBEC295:0xB94E9BE7");
                            TAssetProperty *pScanProperty = TPropCast<TAssetProperty>(pInst->PropertyByIDString(ScanIDString));
                            ASSERT(pScanProperty); // Temporary assert to remind myself later to update this code when uncooked properties are added to the template

                            if (pScanProperty)
                            {
                                CAssetID ScanID = pScanProperty->Get();
                                CResourceEntry *pEntry = pStore->FindEntry(ScanID);

                                if (pEntry && !pEntry->IsNamed())
                                {
                                    TWideString ScanName = Name.ToUTF16().ChopBack(5);

                                    if (ScanName.StartsWith(L"POI_"))
                                        ScanName = ScanName.ChopFront(4);

                                    ApplyGeneratedName(pEntry, pEntry->DirectoryPath(), ScanName);

                                    CScan *pScan = (CScan*) pEntry->Load();
                                    if (pScan && pScan->ScanText())
                                    {
                                        CResourceEntry *pStringEntry = pScan->ScanText()->Entry();
                                        ApplyGeneratedName(pStringEntry, pStringEntry->DirectoryPath(), ScanName);
                                    }
                                }
                            }
                        }
                    }

                    else if (pInst->ObjectTypeID() == 0x17 || pInst->ObjectTypeID() == FOURCC("MEMO"))
                    {
                        TString Name = pInst->InstanceName();

                        if (Name.EndsWith(".STRG", false))
                        {
                            u32 StringPropID = (pProj->Game() <= ePrime ? 0x4 : 0x9182250C);
                            TAssetProperty *pStringProperty = TPropCast<TAssetProperty>(pInst->Properties()->PropertyByID(StringPropID));
                            ASSERT(pStringProperty); // Temporary assert to remind myself later to update this code when uncooked properties are added to the template

                            if (pStringProperty)
                            {
                                CAssetID StringID = pStringProperty->Get();
                                CResourceEntry *pEntry = pStore->FindEntry(StringID);

                                if (pEntry && !pEntry->IsNamed())
                                {
                                    TWideString StringName = Name.ToUTF16().ChopBack(5);

                                    if (StringName.StartsWith(L"HUDMemo - "))
                                        StringName = StringName.ChopFront(10);

                                    ApplyGeneratedName(pEntry, pEntry->DirectoryPath(), StringName);
                                }
                            }
                        }
                    }

                    // Look for lightmapped models - these are going to be unique to this area
                    else if (pInst->ObjectTypeID() == 0x0 || pInst->ObjectTypeID() == FOURCC("ACTR") ||
                             pInst->ObjectTypeID() == 0x8 || pInst->ObjectTypeID() == FOURCC("PLAT"))
                    {
                        u32 ModelPropID = (pProj->Game() <= ePrime ? (pInst->ObjectTypeID() == 0x0 ? 0xA : 0x6) : 0xC27FFA8F);
                        TAssetProperty *pModelProperty = TPropCast<TAssetProperty>(pInst->Properties()->PropertyByID(ModelPropID));
                        ASSERT(pModelProperty); // Temporary assert to remind myself later to update this code when uncooked properties are added to the template

                        if (pModelProperty)
                        {
                            CAssetID ModelID = pModelProperty->Get();
                            CResourceEntry *pEntry = pStore->FindEntry(ModelID);

                            if (pEntry && !pEntry->IsCategorized())
                            {
                                CModel *pModel = (CModel*) pEntry->Load();

                                if (pModel->IsLightmapped())
                                    ApplyGeneratedName(pEntry, AreaCookedDir, pEntry->Name());
                            }
                        }
                    }
                }
            }

            // Other area assets
            CResourceEntry *pPathEntry = pStore->FindEntry(pArea->PathID());
            CResourceEntry *pPoiMapEntry = pArea->PoiToWorldMap() ? pArea->PoiToWorldMap()->Entry() : nullptr;
            CResourceEntry *pPortalEntry = pStore->FindEntry(pArea->PortalAreaID());

            if (pPathEntry)
                ApplyGeneratedName(pPathEntry, WorldMasterDir, AreaName);

            if (pPoiMapEntry)
                ApplyGeneratedName(pPoiMapEntry, WorldMasterDir, AreaName);

            if (pPortalEntry)
                ApplyGeneratedName(pPortalEntry, WorldMasterDir, AreaName);

            pStore->DestroyUnreferencedResources();
#endif
        }
    }
#endif

#if PROCESS_MODELS
    // Generate Model Lightmap names
    for (TResourceIterator<eModel> It(pStore); It; ++It)
    {
        CModel *pModel = (CModel*) It->Load();
        u32 LightmapNum = 0;

        for (u32 iSet = 0; iSet < pModel->GetMatSetCount(); iSet++)
        {
            CMaterialSet *pSet = pModel->GetMatSet(iSet);

            for (u32 iMat = 0; iMat < pSet->NumMaterials(); iMat++)
            {
                CMaterial *pMat = pSet->MaterialByIndex(iMat);

                if (pMat->Options().HasFlag(CMaterial::eLightmap))
                {
                    CTexture *pLightmapTex = pMat->Pass(0)->Texture();
                    CResourceEntry *pTexEntry = pLightmapTex->Entry();
                    if (pTexEntry->IsNamed() || pTexEntry->IsCategorized()) continue;

                    TWideString TexName = TWideString::Format(L"%s_lightmap%d", *It->Name(), LightmapNum);
                    ApplyGeneratedName(pTexEntry, pModel->Entry()->DirectoryPath(), TexName);
                    pTexEntry->SetHidden(true);
                    LightmapNum++;
                }
            }
        }

        pStore->DestroyUnreferencedResources();
    }
#endif

#if PROCESS_AUDIO_GROUPS
    // Generate Audio Group names
    for (TResourceIterator<eAudioGroup> It(pStore); It; ++It)
    {
        CAudioGroup *pGroup = (CAudioGroup*) It->Load();
        TWideString GroupName = pGroup->GroupName().ToUTF16();
        ApplyGeneratedName(*It, L"AudioGrp\\", GroupName);
    }
#endif

#if PROCESS_ANIM_CHAR_SETS
    // Generate animation format names
    for (TResourceIterator<eAnimSet> It(pStore); It; ++It)
    {
        TWideString SetDir = It->DirectoryPath();
        TWideString NewSetName;
        CAnimSet *pSet = (CAnimSet*) It->Load();

        for (u32 iChar = 0; iChar < pSet->NumCharacters(); iChar++)
        {
            const SSetCharacter *pkChar = pSet->Character(iChar);

            TWideString CharName = pkChar->Name.ToUTF16();
            if (iChar == 0) NewSetName = CharName;

            if (pkChar->pModel)     ApplyGeneratedName(pkChar->pModel->Entry(), SetDir, CharName);
            if (pkChar->pSkeleton)  ApplyGeneratedName(pkChar->pSkeleton->Entry(), SetDir, CharName);
            if (pkChar->pSkin)      ApplyGeneratedName(pkChar->pSkin->Entry(), SetDir, CharName);

            if (pkChar->IceModel.IsValid() || pkChar->IceSkin.IsValid())
            {
                TWideString IceName = TWideString::Format(L"%s_IceOverlay", *CharName);

                if (pkChar->IceModel.IsValid())
                {
                    CResourceEntry *pIceModelEntry = pStore->FindEntry(pkChar->IceModel);
                    ApplyGeneratedName(pIceModelEntry, SetDir, IceName);
                }
                if (pkChar->IceSkin.IsValid())
                {
                    CResourceEntry *pIceSkinEntry = pStore->FindEntry(pkChar->IceSkin);
                    ApplyGeneratedName(pIceSkinEntry, SetDir, IceName);
                }
            }
        }

        if (!NewSetName.IsEmpty())
            ApplyGeneratedName(*It, SetDir, NewSetName);

        std::set<CAnimPrimitive> AnimPrimitives;
        pSet->GetUniquePrimitives(AnimPrimitives);

        for (auto It = AnimPrimitives.begin(); It != AnimPrimitives.end(); It++)
        {
            const CAnimPrimitive& rkPrim = *It;
            CAnimation *pAnim = rkPrim.Animation();

            if (pAnim)
            {
                ApplyGeneratedName(pAnim->Entry(), SetDir, rkPrim.Name().ToUTF16());
                CAnimEventData *pEvents = pAnim->EventData();

                if (pEvents)
                    ApplyGeneratedName(pEvents->Entry(), SetDir, rkPrim.Name().ToUTF16());
            }
        }
    }
#endif

#if PROCESS_STRINGS
    // Generate string names
    for (TResourceIterator<eStringTable> It(pStore); It; ++It)
    {
        CStringTable *pString = (CStringTable*) It->Load();
        if (pString->Entry()->IsNamed()) continue;
        TWideString String;

        for (u32 iStr = 0; iStr < pString->NumStrings() && String.IsEmpty(); iStr++)
            String = CStringTable::StripFormatting( pString->String("ENGL", iStr) ).Trimmed();

        if (!String.IsEmpty())
        {
            TWideString Name = String.SubString(0, Math::Min<u32>(String.Size(), 50)).Trimmed();
            Name.Replace(L"\n", L" ");

            while (Name.EndsWith(L".") || TWideString::IsWhitespace(Name.Back()))
                Name = Name.ChopBack(1);

            ApplyGeneratedName(pString->Entry(), pString->Entry()->DirectoryPath(), Name);
        }
    }
#endif

#if PROCESS_SCANS
    // Generate scan names
    for (TResourceIterator<eScan> It(pStore); It; ++It)
    {
        if (It->IsNamed()) continue;
        CScan *pScan = (CScan*) It->Load();
        TWideString ScanName;

        if (pProj->Game() >= eEchoesDemo)
        {
            CAssetID DisplayAsset = pScan->LogbookDisplayAssetID();
            CResourceEntry *pEntry = pStore->FindEntry(DisplayAsset);
            if (pEntry && pEntry->IsNamed()) ScanName = pEntry->Name();
        }

        if (ScanName.IsEmpty())
        {
            CStringTable *pString = pScan->ScanText();
            if (pString) ScanName = pString->Entry()->Name();
        }

        ApplyGeneratedName(pScan->Entry(), It->DirectoryPath(), ScanName);

        if (!ScanName.IsEmpty() && pProj->Game() <= ePrime)
        {
            CAssetID FrameID = pScan->GuiFrame();
            CResourceEntry *pEntry = pStore->FindEntry(FrameID);
            if (pEntry) ApplyGeneratedName(pEntry, pEntry->DirectoryPath(), L"ScanFrame");

            for (u32 iImg = 0; iImg < 4; iImg++)
            {
                CAssetID ImageID = pScan->ScanImage(iImg);
                CResourceEntry *pImgEntry = pStore->FindEntry(ImageID);
                if (pImgEntry) ApplyGeneratedName(pImgEntry, pImgEntry->DirectoryPath(), TWideString::Format(L"%s_Image%d", *ScanName, iImg));
            }
        }
    }
#endif

#if PROCESS_FONTS
    // Generate font names
    for (TResourceIterator<eFont> It(pStore); It; ++It)
    {
        CFont *pFont = (CFont*) It->Load();

        if (pFont)
        {
            ApplyGeneratedName(pFont->Entry(), pFont->Entry()->DirectoryPath(), pFont->FontName().ToUTF16());

            CTexture *pFontTex = pFont->Texture();

            if (pFontTex)
                ApplyGeneratedName(pFontTex->Entry(), pFont->Entry()->DirectoryPath(), pFont->Entry()->Name());
        }
    }
#endif

    pStore->ConditionalSaveStore();
}
