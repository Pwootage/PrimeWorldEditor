#include "CTexture.h"
#include "NTextureUtils.h"
#include <Common/Math/MathUtil.h>

CTexture::CTexture(CResourceEntry *pEntry /*= 0*/)
    : CResource(pEntry)
    , mEditorFormat(ETexelFormat::RGBA8)
    , mGameFormat(EGXTexelFormat::RGBA8)
    , mEnableMultisampling(false)
    , mTextureResource(0)
{
}

CTexture::CTexture(uint32 SizeX, uint32 SizeY)
    : mEditorFormat(ETexelFormat::RGBA8)
    , mGameFormat(EGXTexelFormat::RGBA8)
    , mEnableMultisampling(false)
    , mTextureResource(0)
{
    mMipData.emplace_back();
    SMipData& Mip = mMipData.back();
    Mip.SizeX = SizeX;
    Mip.SizeY = SizeY;
    Mip.DataBuffer.resize(SizeX * SizeY * 4);
}

CTexture::~CTexture()
{
    ReleaseRenderResources();
}

void CTexture::CreateRenderResources()
{
    if (mTextureResource != 0)
    {
        ReleaseRenderResources();
    }

    GLenum BindTarget = (mEnableMultisampling ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D);
    glGenTextures(1, &mTextureResource);
    glBindTexture(BindTarget, mTextureResource);

    GLenum GLFormat, GLType;
    bool bCompressed = false;

    switch (mEditorFormat)
    {
        case ETexelFormat::Luminance:
            GLFormat = GL_LUMINANCE;
            GLType = GL_UNSIGNED_BYTE;
            break;
        case ETexelFormat::LuminanceAlpha:
            GLFormat = GL_LUMINANCE_ALPHA;
            GLType = GL_UNSIGNED_BYTE;
            break;
        case ETexelFormat::RGB565:
            GLFormat = GL_RGB;
            GLType = GL_UNSIGNED_SHORT_5_6_5;
            break;
        case ETexelFormat::RGBA8:
            GLFormat = GL_RGBA;
            GLType = GL_UNSIGNED_BYTE;
            break;
        case ETexelFormat::BC1:
            GLFormat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
            bCompressed = true;
            break;
    }

    // The smallest mipmaps are probably not being loaded correctly, because mipmaps in GX textures have a minimum size depending on the format, and these don't.
    // Not sure specifically what accomodations should be made to fix that though so whatever.
    for (uint MipIdx = 0; MipIdx < mMipData.size(); MipIdx++)
    {
        const SMipData& MipData = mMipData[MipIdx];
        uint SizeX = MipData.SizeX;
        uint SizeY = MipData.SizeY;
        uint DataSize = MipData.DataBuffer.size();
        const void* pkData = MipData.DataBuffer.data();

        if (bCompressed)
        {
            glCompressedTexImage2D(BindTarget, MipIdx, GLFormat, SizeX, SizeY, 0, DataSize, pkData);
        }
        else
        {
            if (mEnableMultisampling)
            {
                glTexImage2DMultisample(BindTarget, 4, GLFormat, SizeX, SizeY, true);
            }
            else
            {
                glTexImage2D(BindTarget, MipIdx, GLFormat, SizeX, SizeY, 0, GLFormat, GLType, pkData);
            }
        }
    }

    glTexParameteri(BindTarget, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(BindTarget, GL_TEXTURE_MAX_LEVEL, mMipData.size() - 1);

    // Linear filtering on mipmaps:
    glTexParameteri(BindTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(BindTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    // Anisotropic filtering:
    float MaxAnisotropy;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &MaxAnisotropy);
    glTexParameterf(BindTarget, GL_TEXTURE_MAX_ANISOTROPY_EXT, MaxAnisotropy);

    // Swizzle for LuminanceAlpha
    if (mEditorFormat == ETexelFormat::LuminanceAlpha)
    {
        glTexParameterf(BindTarget, GL_TEXTURE_SWIZZLE_A, GL_GREEN);
    }
}

void CTexture::ReleaseRenderResources()
{
    if (mTextureResource != 0)
    {
        glDeleteTextures(1, &mTextureResource);
        mTextureResource = 0;
    }
}

void CTexture::BindToSampler(uint SamplerIndex) const
{
    // CreateGraphicsResources() must have been called before calling this
    // @todo this should not be the responsibility of CTexture
    ASSERT( mTextureResource != 0 );
    GLenum BindTarget = (mEnableMultisampling ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D);

    glActiveTexture(GL_TEXTURE0 + SamplerIndex);
    glBindTexture(BindTarget, mTextureResource);
}

/** Generate mipmap chain based on the contents of the first mip */
void CTexture::GenerateMipTail(uint NumMips /*= 0*/)
{
    //@todo
}

/** Allocate mipmap data, but does not fill any data. Returns the new mipmap count. */
uint CTexture::AllocateMipTail(uint DesiredMipCount /*= 0*/)
{
    // We must have at least one mipmap to start with.
    if (mMipData.empty())
    {
        warnf("Failed to allocate mip tail; texture is empty, did not initialize correctly");
        return 0;
    }

    // Try to allocate the requested number of mipmaps, but don't allocate any below 1x1.
    // Also, we always need at least one mipmap.
    uint BaseSizeX = mMipData[0].SizeX;
    uint BaseSizeY = mMipData[0].SizeY;
    uint MaxMips = Math::Min( Math::FloorLog2(BaseSizeX), Math::FloorLog2(BaseSizeY) ) + 1;
    uint NewMipCount = Math::Min(MaxMips, DesiredMipCount);

    if (mMipData.size() != NewMipCount)
    {
        uint OldMipCount = mMipData.size();
        mMipData.resize(NewMipCount);

        // Allocate internal data for any new mips.
        if (NewMipCount > OldMipCount)
        {
            uint LastMipIdx = OldMipCount - 1;
            uint SizeX = mMipData[LastMipIdx].SizeX;
            uint SizeY = mMipData[LastMipIdx].SizeY;
            uint BPP = NTextureUtils::GetTexelFormatInfo( mEditorFormat ).BitsPerPixel;

            for (uint MipIdx = OldMipCount; MipIdx < NewMipCount; MipIdx++)
            {
                SizeX /= 2;
                SizeY /= 2;
                uint Size = (SizeX * SizeY * BPP) / 8;
                mMipData[MipIdx].SizeX = SizeX;
                mMipData[MipIdx].SizeY = SizeY;
                mMipData[MipIdx].DataBuffer.resize(Size);
            }
        }
    }

    return mMipData.size();
}

/**
 * Update the internal resolution of the texture; used for dynamically-scaling textures
 */
void CTexture::Resize(uint32 SizeX, uint32 SizeY)
{
    if (mMipData.size() > 0)
    {
        if (mMipData[0].SizeX == SizeX &&
            mMipData[0].SizeY == SizeY)
        {
            return;
        }
    }

    if (mMipData.size() == 0)
    {
        mMipData.emplace_back();
    }

    const STexelFormatInfo& kFormatInfo = NTextureUtils::GetTexelFormatInfo(mEditorFormat);
    mMipData.back().SizeX = SizeX;
    mMipData.back().SizeY = SizeY;
    mMipData.back().DataBuffer.resize( SizeX * SizeY * kFormatInfo.BitsPerPixel / 8 );

    if (mTextureResource != 0)
    {
        ReleaseRenderResources();
        CreateRenderResources();
    }
}

float CTexture::ReadTexelAlpha(const CVector2f& kTexCoord)
{
    // todo: support texel formats other than DXT1
    // also: this is an inaccurate implementation because it
    // doesn't take into account mipmaps or texture filtering
    const SMipData& kMipData = mMipData[0];
    uint32 TexelX = (uint32) ((kMipData.SizeX - 1) * kTexCoord.X);
    uint32 TexelY = (uint32) ((kMipData.SizeY - 1) * (1.f - fmodf(kTexCoord.Y, 1.f)));

    if (mEditorFormat == ETexelFormat::Luminance || mEditorFormat == ETexelFormat::RGB565)
    {
        // No alpha in these formats
        return 1.f;
    }
    else if (mEditorFormat == ETexelFormat::BC1)
    {
        // 8 bytes per 4x4 16-pixel block, left-to-right top-to-bottom
        uint32 BlockIdxX = TexelX / 4;
        uint32 BlockIdxY = TexelY / 4;
        uint32 BlocksPerRow = kMipData.SizeX / 4;
        uint32 BufferPos = (8 * BlockIdxX) + (8 * BlockIdxY * BlocksPerRow);

        uint16 PaletteA, PaletteB;
        uint32 PaletteIndices;
        memcpy(&PaletteA, &kMipData.DataBuffer[BufferPos+0], 2);
        memcpy(&PaletteB, &kMipData.DataBuffer[BufferPos+2], 2);
        memcpy(&PaletteIndices, &kMipData.DataBuffer[BufferPos+4], 4);

        if (PaletteA > PaletteB)
        {
            // No palette colors have alpha
            return 1.f;
        }

        // BC1 is a 1-bit alpha format; texels either have alpha, or they don't
        // Alpha is only present on palette index 3
        // We don't need to calculate/decode the actual palette colors.
        uint32 BlockCol = (TexelX & 0xF) / 4;
        uint32 BlockRow = (TexelY & 0xF) / 4;
        uint32 Shift = (BlockRow << 3) | (BlockCol << 1);
        uint32 PaletteIndex = (PaletteIndices >> Shift) & 0x3;
        return (PaletteIndex == 3 ? 0.f : 1.f);
    }
    else
    {
        // Other formats unsupported
        return 1.f;
    }
}
