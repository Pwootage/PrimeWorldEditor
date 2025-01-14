#include "CAnimationLoader.h"
#include <Common/Macros.h>
#include <Common/Log.h>
#include <Common/Math/MathUtil.h>

bool CAnimationLoader::UncompressedCheckEchoes()
{
    // The best way we have to tell this is an Echoes ANIM is to try to parse it as an
    // Echoes ANIM and see whether we read the file correctly. The formatting has to be
    // a little weird because we have to make sure we don't try to seek or read anything
    // past the end of the file. The +4 being added to each size we test is to account
    // for the next size value of the next array.
    uint32 End = mpInput->Size();

    uint32 NumRotIndices = mpInput->ReadLong();
    if (mpInput->Tell() + NumRotIndices + 4 >= End) return false;
    mpInput->Seek(NumRotIndices, SEEK_CUR);

    uint32 NumTransIndices = mpInput->ReadLong();
    if (mpInput->Tell() + NumTransIndices + 4 >= End) return false;
    mpInput->Seek(NumTransIndices, SEEK_CUR);

    uint32 NumScaleIndices = mpInput->ReadLong();
    if (mpInput->Tell() + NumScaleIndices + 4 >= End) return false;
    mpInput->Seek(NumScaleIndices, SEEK_CUR);

    uint32 ScaleKeysSize = mpInput->ReadLong() * 0xC;
    if (mpInput->Tell() + ScaleKeysSize + 4 >= End) return false;
    mpInput->Seek(ScaleKeysSize, SEEK_CUR);

    uint32 RotKeysSize = mpInput->ReadLong() * 0x10;
    if (mpInput->Tell() + RotKeysSize + 4 >= End) return false;
    mpInput->Seek(RotKeysSize, SEEK_CUR);

    uint32 TransKeysSize = mpInput->ReadLong() * 0xC;
    return (mpInput->Tell() + TransKeysSize == End);
}

EGame CAnimationLoader::UncompressedCheckVersion()
{
    // Call this function after the bone channel index array
    // No version number, so this is how we have to determine the version...
    uint32 Start = mpInput->Tell();
    bool Echoes = UncompressedCheckEchoes();
    mpInput->Seek(Start, SEEK_SET);
    return (Echoes ? EGame::Echoes : EGame::Prime);
}

void CAnimationLoader::ReadUncompressedANIM()
{
    mpAnim->mDuration = mpInput->ReadFloat();
    mpInput->Seek(0x4, SEEK_CUR); // Skip differential state
    mpAnim->mTickInterval = mpInput->ReadFloat();
    mpInput->Seek(0x4, SEEK_CUR); // Skip differential state

    mpAnim->mNumKeys = mpInput->ReadLong();
    mpInput->Seek(0x4, SEEK_CUR); // Skip root bone ID

    // Read bone channel info
    uint32 NumBoneChannels = 0;
    uint32 NumScaleChannels = 0;
    uint32 NumRotationChannels = 0;
    uint32 NumTranslationChannels = 0;

    // Bone channel list
    uint32 NumBoneIndices = mpInput->ReadLong();
    ASSERT(NumBoneIndices == 100);
    std::vector<uint8> BoneIndices(NumBoneIndices);

    for (uint32 iChan = 0; iChan < NumBoneIndices; iChan++)
    {
        BoneIndices[iChan] = mpInput->ReadByte();

        if (BoneIndices[iChan] != 0xFF)
            NumBoneChannels++;
    }

    if (mGame == EGame::Invalid)
        mGame = UncompressedCheckVersion();

    // Echoes only - rotation channel indices
    std::vector<uint8> RotationIndices;

    if (mGame >= EGame::EchoesDemo)
    {
        uint32 NumRotationIndices = mpInput->ReadLong();
        RotationIndices.resize(NumRotationIndices);

        for (uint32 iRot = 0; iRot < NumRotationIndices; iRot++)
        {
            RotationIndices[iRot] = mpInput->ReadByte();

            if (RotationIndices[iRot] != 0xFF)
                NumRotationChannels++;
        }
    }

    else
    {
        // In MP1 every bone channel has a rotation, so just copy the valid channels from the bone channel list.
        RotationIndices.resize(NumBoneChannels);

        for (uint32 iBone = 0; iBone < BoneIndices.size(); iBone++)
        {
            if (BoneIndices[iBone] != 0xFF)
            {
                RotationIndices[NumRotationChannels] = BoneIndices[iBone];
                NumRotationChannels++;
            }
        }
    }

    // Translation channel indices
    uint32 NumTransIndices = mpInput->ReadLong();
    std::vector<uint8> TransIndices(NumTransIndices);

    for (uint32 iTrans = 0; iTrans < NumTransIndices; iTrans++)
    {
        TransIndices[iTrans] = mpInput->ReadByte();

        if (TransIndices[iTrans] != 0xFF)
            NumTranslationChannels++;
    }

    // Echoes only - scale channel indices
    std::vector<uint8> ScaleIndices;

    if (mGame >= EGame::EchoesDemo)
    {
        uint32 NumScaleIndices = mpInput->ReadLong();
        ScaleIndices.resize(NumScaleIndices);

        for (uint32 iScale = 0; iScale < NumScaleIndices; iScale++)
        {
            ScaleIndices[iScale] = mpInput->ReadByte();

            if (ScaleIndices[iScale] != 0xFF)
                NumScaleChannels++;
        }
    }

    // Set up bone channel info
    for (uint32 iBone = 0, iChan = 0; iBone < NumBoneIndices; iBone++)
    {
        uint8 BoneIdx = BoneIndices[iBone];

        if (BoneIdx != 0xFF)
        {
            mpAnim->mBoneInfo[iBone].TranslationChannelIdx = (TransIndices.empty() ? 0xFF : TransIndices[iChan]);
            mpAnim->mBoneInfo[iBone].RotationChannelIdx = (RotationIndices.empty() ? 0xFF : RotationIndices[iChan]);
            mpAnim->mBoneInfo[iBone].ScaleChannelIdx = (ScaleIndices.empty() ? 0xFF : ScaleIndices[iChan]);
            iChan++;
        }

        else
        {
            mpAnim->mBoneInfo[iBone].TranslationChannelIdx = 0xFF;
            mpAnim->mBoneInfo[iBone].RotationChannelIdx = 0xFF;
            mpAnim->mBoneInfo[iBone].ScaleChannelIdx = 0xFF;
        }
    }

    // Read bone transforms
    if (mGame >= EGame::EchoesDemo)
    {
        mpInput->Seek(0x4, SEEK_CUR); // Skipping scale key count
        mpAnim->mScaleChannels.resize(NumScaleChannels);

        for (uint32 iScale = 0; iScale < NumScaleChannels; iScale++)
        {
            mpAnim->mScaleChannels[iScale].resize(mpAnim->mNumKeys);

            for (uint32 iKey = 0; iKey < mpAnim->mNumKeys; iKey++)
                mpAnim->mScaleChannels[iScale][iKey] = CVector3f(*mpInput);
        }
    }

    mpInput->Seek(0x4, SEEK_CUR); // Skipping rotation key count
    mpAnim->mRotationChannels.resize(NumRotationChannels);

    for (uint32 iRot = 0; iRot < NumRotationChannels; iRot++)
    {
        mpAnim->mRotationChannels[iRot].resize(mpAnim->mNumKeys);

        for (uint32 iKey = 0; iKey < mpAnim->mNumKeys; iKey++)
            mpAnim->mRotationChannels[iRot][iKey] = CQuaternion(*mpInput);
    }

    mpInput->Seek(0x4, SEEK_CUR); // Skipping translation key count
    mpAnim->mTranslationChannels.resize(NumTranslationChannels);

    for (uint32 iTrans = 0; iTrans < NumTranslationChannels; iTrans++)
    {
        mpAnim->mTranslationChannels[iTrans].resize(mpAnim->mNumKeys);

        for (uint32 iKey = 0; iKey < mpAnim->mNumKeys; iKey++)
            mpAnim->mTranslationChannels[iTrans][iKey] = CVector3f(*mpInput);
    }

    if (mGame == EGame::Prime)
    {
        mpAnim->mpEventData = gpResourceStore->LoadResource<CAnimEventData>(mpInput->ReadLong());
    }
}

void CAnimationLoader::ReadCompressedANIM()
{
    // Header
    mpInput->Seek(0x4, SEEK_CUR); // Skip alloc size

    // Version check
    mGame = (mpInput->PeekShort() == 0x0101 ? EGame::Echoes : EGame::Prime);

    // Check the ANIM resource's game instead of the version check we just determined.
    // The Echoes demo has some ANIMs that use MP1's format, but don't have the EVNT reference.
    if (mpAnim->Game() <= EGame::Prime)
    {
        mpAnim->mpEventData = gpResourceStore->LoadResource<CAnimEventData>(mpInput->ReadLong());
    }

    mpInput->Seek(mGame <= EGame::Prime ? 4 : 2, SEEK_CUR); // Skip unknowns
    mpAnim->mDuration = mpInput->ReadFloat();
    mpAnim->mTickInterval = mpInput->ReadFloat();
    mpInput->Seek(0x8, SEEK_CUR); // Skip two unknown values

    mRotationDivisor = mpInput->ReadLong();
    mTranslationMultiplier = mpInput->ReadFloat();
    if (mGame >= EGame::EchoesDemo) mScaleMultiplier = mpInput->ReadFloat();
    uint32 NumBoneChannels = mpInput->ReadLong();
    mpInput->Seek(0x4, SEEK_CUR); // Skip unknown value

    // Read key flags
    uint32 NumKeys = mpInput->ReadLong();
    mpAnim->mNumKeys = NumKeys;
    mKeyFlags.resize(NumKeys);
    {
        CBitStreamInWrapper BitStream(mpInput);

        for (uint32 iBit = 0; iBit < NumKeys; iBit++)
            mKeyFlags[iBit] = BitStream.ReadBit();
    }
    mpInput->Seek(mGame == EGame::Prime ? 0x8 : 0x4, SEEK_CUR);

    // Read bone channel descriptors
    mCompressedChannels.resize(NumBoneChannels);
    mpAnim->mScaleChannels.resize(NumBoneChannels);
    mpAnim->mRotationChannels.resize(NumBoneChannels);
    mpAnim->mTranslationChannels.resize(NumBoneChannels);

    for (uint32 iChan = 0; iChan < NumBoneChannels; iChan++)
    {
        SCompressedChannel& rChan = mCompressedChannels[iChan];
        rChan.BoneID = (mGame == EGame::Prime ? mpInput->ReadLong() : mpInput->ReadByte());

        // Read rotation parameters
        rChan.NumRotationKeys = mpInput->ReadShort();

        if (rChan.NumRotationKeys > 0)
        {
            for (uint32 iComp = 0; iComp < 3; iComp++)
            {
                rChan.Rotation[iComp] = mpInput->ReadShort();
                rChan.RotationBits[iComp] = mpInput->ReadByte();
            }

            mpAnim->mBoneInfo[rChan.BoneID].RotationChannelIdx = (uint8) iChan;
        }
        else mpAnim->mBoneInfo[rChan.BoneID].RotationChannelIdx = 0xFF;

        // Read translation parameters
        rChan.NumTranslationKeys = mpInput->ReadShort();

        if (rChan.NumTranslationKeys > 0)
        {
            for (uint32 iComp = 0; iComp < 3; iComp++)
            {
                rChan.Translation[iComp] = mpInput->ReadShort();
                rChan.TranslationBits[iComp] = mpInput->ReadByte();
            }

            mpAnim->mBoneInfo[rChan.BoneID].TranslationChannelIdx = (uint8) iChan;
        }
        else mpAnim->mBoneInfo[rChan.BoneID].TranslationChannelIdx = 0xFF;

        // Read scale parameters
        uint8 ScaleIdx = 0xFF;

        if (mGame >= EGame::EchoesDemo)
        {
            rChan.NumScaleKeys = mpInput->ReadShort();

            if (rChan.NumScaleKeys > 0)
            {
                for (uint32 iComp = 0; iComp < 3; iComp++)
                {
                    rChan.Scale[iComp] = mpInput->ReadShort();
                    rChan.ScaleBits[iComp] = mpInput->ReadByte();
                }

                ScaleIdx = (uint8) iChan;
            }
        }
        mpAnim->mBoneInfo[rChan.BoneID].ScaleChannelIdx = ScaleIdx;
    }

    // Read animation data
    ReadCompressedAnimationData();
}

void CAnimationLoader::ReadCompressedAnimationData()
{
    CBitStreamInWrapper BitStream(mpInput);

    // Initialize
    for (uint32 iChan = 0; iChan < mCompressedChannels.size(); iChan++)
    {
        SCompressedChannel& rChan = mCompressedChannels[iChan];

        // Set initial rotation/translation/scale
        if (rChan.NumRotationKeys > 0)
        {
            mpAnim->mRotationChannels[iChan].reserve(rChan.NumRotationKeys + 1);
            CQuaternion Rotation = DequantizeRotation(false, rChan.Rotation[0], rChan.Rotation[1], rChan.Rotation[2]);
            mpAnim->mRotationChannels[iChan].push_back(Rotation);
        }

        if (rChan.NumTranslationKeys > 0)
        {
            mpAnim->mTranslationChannels[iChan].reserve(rChan.NumTranslationKeys + 1);
            CVector3f Translate = CVector3f(rChan.Translation[0], rChan.Translation[1], rChan.Translation[2]) * mTranslationMultiplier;
            mpAnim->mTranslationChannels[iChan].push_back(Translate);
        }

        if (rChan.NumScaleKeys > 0)
        {
            mpAnim->mScaleChannels[iChan].reserve(rChan.NumScaleKeys + 1);
            CVector3f Scale = CVector3f(rChan.Scale[0], rChan.Scale[1], rChan.Scale[2]) * mScaleMultiplier;
            mpAnim->mScaleChannels[iChan].push_back(Scale);
        }
    }

    // Read keys
    for (uint32 iKey = 0; iKey < mpAnim->mNumKeys - 1; iKey++)
    {
        bool KeyPresent = mKeyFlags[iKey+1];

        for (uint32 iChan = 0; iChan < mCompressedChannels.size(); iChan++)
        {
            SCompressedChannel& rChan = mCompressedChannels[iChan];

            // Read rotation
            if (rChan.NumRotationKeys > 0)
            {
                // Note if KeyPresent is false, this isn't the correct value of WSign.
                // However, we're going to recreate this key later via interpolation, so it doesn't matter what value we use here.
                bool WSign = (KeyPresent ? BitStream.ReadBit() : false);

                if (KeyPresent)
                {
                    rChan.Rotation[0] += (int16) BitStream.ReadBits(rChan.RotationBits[0]);
                    rChan.Rotation[1] += (int16) BitStream.ReadBits(rChan.RotationBits[1]);
                    rChan.Rotation[2] += (int16) BitStream.ReadBits(rChan.RotationBits[2]);
                }

                CQuaternion Rotation = DequantizeRotation(WSign, rChan.Rotation[0], rChan.Rotation[1], rChan.Rotation[2]);
                mpAnim->mRotationChannels[iChan].push_back(Rotation);
            }

            // Read translation
            if (rChan.NumTranslationKeys > 0)
            {
                if (KeyPresent)
                {
                    rChan.Translation[0] += (int16) BitStream.ReadBits(rChan.TranslationBits[0]);
                    rChan.Translation[1] += (int16) BitStream.ReadBits(rChan.TranslationBits[1]);
                    rChan.Translation[2] += (int16) BitStream.ReadBits(rChan.TranslationBits[2]);
                }

                CVector3f Translate = CVector3f(rChan.Translation[0], rChan.Translation[1], rChan.Translation[2]) * mTranslationMultiplier;
                mpAnim->mTranslationChannels[iChan].push_back(Translate);
            }

            // Read scale
            if (rChan.NumScaleKeys > 0)
            {
                if (KeyPresent)
                {
                    rChan.Scale[0] += (int16) BitStream.ReadBits(rChan.ScaleBits[0]);
                    rChan.Scale[1] += (int16) BitStream.ReadBits(rChan.ScaleBits[1]);
                    rChan.Scale[2] += (int16) BitStream.ReadBits(rChan.ScaleBits[2]);
                }

                CVector3f Scale = CVector3f(rChan.Scale[0], rChan.Scale[1], rChan.Scale[2]) * mScaleMultiplier;
                mpAnim->mScaleChannels[iChan].push_back(Scale);
            }
        }
    }

    // Fill in missing keys
    uint32 NumMissedKeys = 0;

    for (uint32 iKey = 0; iKey < mpAnim->mNumKeys; iKey++)
    {
        if (!mKeyFlags[iKey])
            NumMissedKeys++;

        else if (NumMissedKeys > 0)
        {
            uint32 FirstIndex = iKey - NumMissedKeys - 1;
            uint32 LastIndex = iKey;
            uint32 RelLastIndex = LastIndex - FirstIndex;

            for (uint32 iMissed = 0; iMissed < NumMissedKeys; iMissed++)
            {
                uint32 KeyIndex = FirstIndex + iMissed + 1;
                uint32 RelKeyIndex = (KeyIndex - FirstIndex);
                float Interp = (float) RelKeyIndex / (float) RelLastIndex;

                for (uint32 iChan = 0; iChan < mCompressedChannels.size(); iChan++)
                {
                    bool HasTranslationKeys = mCompressedChannels[iChan].NumTranslationKeys > 0;
                    bool HasRotationKeys = mCompressedChannels[iChan].NumRotationKeys > 0;
                    bool HasScaleKeys = mCompressedChannels[iChan].NumScaleKeys > 0;

                    if (HasRotationKeys)
                    {
                        CQuaternion Left = mpAnim->mRotationChannels[iChan][FirstIndex];
                        CQuaternion Right = mpAnim->mRotationChannels[iChan][LastIndex];
                        mpAnim->mRotationChannels[iChan][KeyIndex] = Left.Slerp(Right, Interp);
                    }

                    if (HasTranslationKeys)
                    {
                        CVector3f Left = mpAnim->mTranslationChannels[iChan][FirstIndex];
                        CVector3f Right = mpAnim->mTranslationChannels[iChan][LastIndex];
                        mpAnim->mTranslationChannels[iChan][KeyIndex] = Math::Lerp<CVector3f>(Left, Right, Interp);
                    }

                    if (HasScaleKeys)
                    {
                        CVector3f Left = mpAnim->mScaleChannels[iChan][FirstIndex];
                        CVector3f Right = mpAnim->mScaleChannels[iChan][LastIndex];
                        mpAnim->mScaleChannels[iChan][KeyIndex] = Math::Lerp<CVector3f>(Left, Right, Interp);
                    }
                }
            }

            NumMissedKeys = 0;
        }
    }
}

CQuaternion CAnimationLoader::DequantizeRotation(bool Sign, int16 X, int16 Y, int16 Z)
{
    CQuaternion Out;
    float Multiplier = Math::skHalfPi / (float) mRotationDivisor;
    Out.X = sinf(X * Multiplier);
    Out.Y = sinf(Y * Multiplier);
    Out.Z = sinf(Z * Multiplier);
    Out.W = Math::Sqrt( fmax(1.f - ((Out.X * Out.X) + (Out.Y * Out.Y) + (Out.Z * Out.Z)), 0.f) );
    if (Sign) Out.W = -Out.W;
    return Out;
}

// ************ STATIC ************
CAnimation* CAnimationLoader::LoadANIM(IInputStream& rANIM, CResourceEntry *pEntry)
{
    // MP3/DKCR unsupported
    if (pEntry->Game() > EGame::Echoes)
        return new CAnimation(pEntry);

    uint32 CompressionType = rANIM.ReadLong();

    if (CompressionType != 0 && CompressionType != 2)
    {
        errorf("%s: Unknown ANIM compression type: %d", *rANIM.GetSourceString(), CompressionType);
        return nullptr;
    }

    CAnimationLoader Loader;
    Loader.mpAnim = new CAnimation(pEntry);
    Loader.mGame = pEntry->Game();
    Loader.mpInput = &rANIM;

    if (CompressionType == 0)
        Loader.ReadUncompressedANIM();
    else
        Loader.ReadCompressedANIM();

    return Loader.mpAnim;
}
