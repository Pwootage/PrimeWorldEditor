#include "NTextureUtils.h"
#include <Common/Common.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ASSERT ASSERT
#include <stb_image.h>
#include <stb_image_resize.h>
#include <stb_image_write.h>

namespace NTextureUtils
{

/** Table of image format info; indexed by EGXTexelFormat. */
STexelFormatInfo kTexelFormatInfo[] =
{
    // GameTexelFormat          EditorTexelFormat               BlockSizeX  BlockSizeY  BitsPerPixel
    {  EGXTexelFormat::I4,      ETexelFormat::Luminance,        8,          8,          4   },
    {  EGXTexelFormat::I8,      ETexelFormat::Luminance,        8,          4,          8   },
    {  EGXTexelFormat::IA4,     ETexelFormat::LuminanceAlpha,   8,          4,          8   },
    {  EGXTexelFormat::IA8,     ETexelFormat::LuminanceAlpha,   4,          4,          16  },
    {  EGXTexelFormat::C4,      ETexelFormat::Invalid,          8,          8,          4   },
    {  EGXTexelFormat::C8,      ETexelFormat::Invalid,          8,          4,          8   },
    {  EGXTexelFormat::C14x2,   ETexelFormat::Invalid,          4,          4,          16  },
    {  EGXTexelFormat::RGB565,  ETexelFormat::RGB565,           4,          4,          16  },
    {  EGXTexelFormat::RGB5A3,  ETexelFormat::RGBA8,            4,          4,          16  },
    {  EGXTexelFormat::RGBA8,   ETexelFormat::RGBA8,            4,          4,          32  },
    {  EGXTexelFormat::CMPR,    ETexelFormat::BC1,              8,          8,          4   },
};

/** Remap an ETexelFormat to the closest EGXTexelFormat */
const EGXTexelFormat kEditorFormatToGameFormat[] =
{
    // Luminance
    EGXTexelFormat::I8,
    // LuminanceAlpha
    EGXTexelFormat::IA8,
    // RGB565
    EGXTexelFormat::RGB565,
    // RGBA8
    EGXTexelFormat::RGBA8,
    // BC1
    EGXTexelFormat::CMPR
};

/** Retrieve the format info for a given texel format */
const STexelFormatInfo& GetTexelFormatInfo(EGXTexelFormat Format)
{
    uint FormatIdx = (uint) Format;
    ASSERT( FormatIdx >= 0 && FormatIdx < ARRAY_SIZE(kTexelFormatInfo) );

    return kTexelFormatInfo[(uint) Format];
}

const STexelFormatInfo& GetTexelFormatInfo(ETexelFormat Format)
{
    return GetTexelFormatInfo( kEditorFormatToGameFormat[(uint) Format] );
}

/** Import the image file at the given path. Returns true if succeeded. */
bool LoadImageFromFile(const TString& kPath,
                       std::vector<uint8>& OutBuffer,
                       int& OutSizeX,
                       int& OutSizeY,
                       int& OutNumChannels)
{
    std::vector<uint8> DataBuffer;
    FileUtil::LoadFileToBuffer(kPath, DataBuffer);
    return LoadImageFromMemory(DataBuffer.data(), DataBuffer.size(), OutBuffer, OutSizeX, OutSizeY, OutNumChannels);
}

/** Import an image file from a buffer. Returns true if succeeded. */
bool LoadImageFromMemory(void* pData,
                         uint DataSize,
                         std::vector<uint8>& OutBuffer,
                         int& OutSizeX,
                         int& OutSizeY,
                         int& OutNumChannels)
{
    stbi_uc* pOutData = stbi_load_from_memory( (const stbi_uc*) pData, DataSize, &OutSizeX, &OutSizeY, &OutNumChannels, STBI_default );

    if (pOutData)
    {
        uint32 ImageSize = (OutSizeX * OutSizeY * OutNumChannels);
        OutBuffer.resize(ImageSize);
        memcpy(OutBuffer.data(), pOutData, ImageSize);

        STBI_FREE(pOutData);
        return true;
    }
    else
    {
        return false;
    }
}

} // end namespace NImageUtils
