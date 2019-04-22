#include "CTextureDecoder.h"
#include "Core/Resource/Texture/NTextureUtils.h"
#include <Common/Log.h>
#include <Common/CColor.h>
#include <Common/Math/MathUtil.h>

CTexture* CTextureDecoder::LoadTXTR(IInputStream& TXTR, CResourceEntry* pEntry)
{
    CTextureDecoder Decoder(pEntry);
    return Decoder.ReadTXTR(TXTR);
}

// ************ READ ************
CTexture* CTextureDecoder::ReadTXTR(IInputStream& TXTR)
{
    if (!TXTR.IsValid())
    {
        warnf("Texture stream is invalid");
        return nullptr;
    }

    mpTexture = new CTexture(mpEntry);

    // Read TXTR header
    mTexelFormat = EGXTexelFormat(TXTR.ReadLong());
    mSizeX = TXTR.ReadShort();
    mSizeY = TXTR.ReadShort();
    mNumMipMaps = TXTR.ReadLong();

    // For C4 and C8 images, read palette
    if ((mTexelFormat == EGXTexelFormat::C4) || (mTexelFormat == EGXTexelFormat::C8))
    {
        uint PaletteFormatID = TXTR.ReadLong();

        EGXTexelFormat PaletteFormat = (PaletteFormatID == 0 ? EGXTexelFormat::IA8 :
                                        PaletteFormatID == 1 ? EGXTexelFormat::RGB565 :
                                        PaletteFormatID == 2 ? EGXTexelFormat::RGB5A3 :
                                        EGXTexelFormat::Invalid);

        TXTR.Skip(4);

        // Parse in palette colors
        const STexelFormatInfo& EncodedFormatInfo = NTextureUtils::GetTexelFormatInfo(PaletteFormat);
        const STexelFormatInfo& DecodedFormatInfo = NTextureUtils::GetTexelFormatInfo(EncodedFormatInfo.EditorTexelFormat);
        uint32 PaletteEntryCount = (mTexelFormat == EGXTexelFormat::C4) ? 16 : 256;

        mPaletteTexelStride = DecodedFormatInfo.BitsPerPixel / 8;
        mPaletteData.resize(PaletteEntryCount * DecodedFormatInfo.BitsPerPixel / 8);
        CMemoryOutStream PaletteStream(mPaletteData.data(), mPaletteData.size(), EEndian::LittleEndian);

        for (uint EntryIdx = 0; EntryIdx < PaletteEntryCount; EntryIdx++)
        {
            ParseTexel(PaletteFormat, TXTR, PaletteStream);
        }
    }

    // Create mipmaps
    const STexelFormatInfo& FormatInfo   = NTextureUtils::GetTexelFormatInfo(mTexelFormat);
    const STexelFormatInfo& EdFormatInfo = NTextureUtils::GetTexelFormatInfo(FormatInfo.EditorTexelFormat);
    mpTexture->mMipData.resize(mNumMipMaps);
    uint SizeX = mSizeX, SizeY = mSizeY;

    for (uint MipIdx = 0; MipIdx < mNumMipMaps; MipIdx++)
    {
        SMipData& MipData = mpTexture->mMipData[MipIdx];
        MipData.SizeX = SizeX;
        MipData.SizeY = SizeY;

        uint MipDataSize = SizeX * SizeY * EdFormatInfo.BitsPerPixel / 8;
        MipData.DataBuffer.resize(MipDataSize);

        SizeX /= 2;
        SizeY /= 2;
    }

    // Read image data
    DecodeGXTexture(TXTR);
    return mpTexture;
}

// ************ DECODE ************
void CTextureDecoder::DecodeGXTexture(IInputStream& TXTR)
{
    const STexelFormatInfo& InFormatInfo  = NTextureUtils::GetTexelFormatInfo(mTexelFormat);
    const STexelFormatInfo& OutFormatInfo = NTextureUtils::GetTexelFormatInfo(InFormatInfo.EditorTexelFormat);

    uint MipSizeX = mSizeX;
    uint MipSizeY = mSizeY;
    uint BlockSizeX = InFormatInfo.BlockSizeX;
    uint BlockSizeY = InFormatInfo.BlockSizeY;
    uint InBPP  = InFormatInfo .BitsPerPixel;
    uint OutBPP = OutFormatInfo.BitsPerPixel;

    // For CMPR, we parse per 4x4 block instead of per texel
    if (mTexelFormat == EGXTexelFormat::CMPR)
    {
        MipSizeX /= 4;
        MipSizeY /= 4;
        BlockSizeX /= 4;
        BlockSizeY /= 4;
        InBPP  *= 16;
        OutBPP *= 16;
    }

    // This value set to true if we hit the end of the file earlier than expected.
    // This is necessary due to a mistake Retro made in their cooker for I8 textures where very small mipmaps are cut off early, resulting in an out-of-bounds memory access.
    // This affects one texture that I know of - Echoes 3BB2C034
    bool bBreakEarly = false;

    for (uint MipIdx = 0; MipIdx < mNumMipMaps && !bBreakEarly; MipIdx++)
    {
        SMipData& MipData = mpTexture->mMipData[MipIdx];
        CMemoryOutStream MipStream( MipData.DataBuffer.data(), MipData.DataBuffer.size(), EEndian::LittleEndian );

        uint SizeX = Math::Max(MipSizeX, BlockSizeX);
        uint SizeY = Math::Max(MipSizeY, BlockSizeY);

        for (uint Y = 0; Y < SizeY && !bBreakEarly; Y += BlockSizeY)
        {
            for (uint X = 0; X < SizeX && !bBreakEarly; X += BlockSizeX)
            {
                for (uint BY = 0; BY < BlockSizeY && !bBreakEarly; BY++)
                {
                    for (uint BX = 0; BX < BlockSizeX && !bBreakEarly; BX++)
                    {
                        // For small mipmaps below the format's block size, skip texels outside the mipmap.
                        // The input data has texels for these dummy texels, but our output data doesn't.
                        if (BX >= BlockSizeX || BY >= BlockSizeY)
                        {
                            uint SkipAmount = Math::Max(InBPP / 8, 1u);
                            TXTR.Skip(SkipAmount);
                        }
                        else
                        {
                            uint Col = X + BX;
                            uint Row = Y + BY;
                            uint DstPixel = (Row * SizeX) + Col;
                            uint DstOffset = (DstPixel * OutBPP) / 8;
                            MipStream.GoTo(DstOffset);
                            ParseTexel(mTexelFormat, TXTR, MipStream);
                        }

                        // ParseTexel parses two texels at a time for 4 bpp formats
                        if (InFormatInfo.BitsPerPixel == 4 && mTexelFormat != EGXTexelFormat::CMPR)
                        {
                            BX++;
                        }

                        // Check if we reached the end of the file early
                        if (TXTR.EoF())
                        {
                            bBreakEarly = true;
                        }
                    }
                }
            }
        }

        MipSizeX /= 2;
        MipSizeY /= 2;
    }

    // Finalize texture
    mpTexture->mGameFormat = mTexelFormat;
    mpTexture->mEditorFormat = InFormatInfo.EditorTexelFormat;
    mpTexture->CreateRenderResources();
}

// ************ READ PIXELS (PARTIAL DECODE) ************
void CTextureDecoder::ParseTexel(EGXTexelFormat Format, IInputStream& Src, IOutputStream& Dst)
{
    switch (Format)
    {
    case EGXTexelFormat::I4:        ParseI4(Src, Dst);      break;
    case EGXTexelFormat::I8:        ParseI8(Src, Dst);      break;
    case EGXTexelFormat::IA4:       ParseIA4(Src, Dst);     break;
    case EGXTexelFormat::IA8:       ParseIA8(Src, Dst);     break;
    case EGXTexelFormat::C4:        ParseC4(Src, Dst);      break;
    case EGXTexelFormat::C8:        ParseC8(Src, Dst);      break;
    case EGXTexelFormat::C14x2:     /* Unsupported */       break;
    case EGXTexelFormat::RGB565:    ParseRGB565(Src, Dst);  break;
    case EGXTexelFormat::RGB5A3:    ParseRGB5A3(Src, Dst);  break;
    case EGXTexelFormat::RGBA8:     ParseRGBA8(Src, Dst);   break;
    case EGXTexelFormat::CMPR:      ParseCMPR(Src, Dst);    break;
    }
}

void CTextureDecoder::ParseI4(IInputStream& Src, IOutputStream& Dst)
{
    uint8 Pixels = Src.ReadByte();
    Dst.WriteByte(Extend4to8(Pixels >> 4));
    Dst.WriteByte(Extend4to8(Pixels >> 4));
    Dst.WriteByte(Extend4to8(Pixels));
    Dst.WriteByte(Extend4to8(Pixels));
}

void CTextureDecoder::ParseI8(IInputStream& Src, IOutputStream& Dst)
{
    uint8 Pixel = Src.ReadByte();
    Dst.WriteByte(Pixel);
    Dst.WriteByte(Pixel);
}

void CTextureDecoder::ParseIA4(IInputStream& Src, IOutputStream& Dst)
{
    // this can be left as-is for DDS conversion, but opengl doesn't support two components in one byte...
    uint8 Byte = Src.ReadByte();
    uint8 Alpha = Extend4to8(Byte >> 4);
    uint8 Lum = Extend4to8(Byte);
    Dst.WriteShort((Lum << 8) | Alpha);
}

void CTextureDecoder::ParseIA8(IInputStream& Src, IOutputStream& Dst)
{
    Dst.WriteShort(Src.ReadShort());
}

void CTextureDecoder::ParseC4(IInputStream& Src, IOutputStream& Dst)
{
    // This isn't how C4 works, but due to the way Retro packed font textures (which use C4)
    // this is the only way to get them to decode correctly for now.
    // Commented-out code is proper C4 decoding. Dedicated font texture-decoding function
    // is probably going to be necessary in the future.
    uint8 Byte = Src.ReadByte();
    uint8 Indices[2];
    Indices[0] = (Byte >> 4) & 0xF;
    Indices[1] = Byte & 0xF;

    for (uint32 iIdx = 0; iIdx < 2; iIdx++)
    {
        uint8 R, G, B, A;
        ((Indices[iIdx] >> 3) & 0x1) ? R = 0xFF : R = 0x0;
        ((Indices[iIdx] >> 2) & 0x1) ? G = 0xFF : G = 0x0;
        ((Indices[iIdx] >> 1) & 0x1) ? B = 0xFF : B = 0x0;
        ((Indices[iIdx] >> 0) & 0x1) ? A = 0xFF : A = 0x0;
        uint32 RGBA = (R << 24) | (G << 16) | (B << 8) | (A);
        Dst.WriteLong(RGBA);

      /*void* pPaletteTexel = mPaletteData.data() + (Indices[iIdx] * mPaletteTexelStride);
        Dst.WriteBytes(pPaletteTexel, mPaletteTexelStride);*/
    }
}

void CTextureDecoder::ParseC8(IInputStream& Src, IOutputStream& Dst)
{
    // DKCR fonts use C8 :|
    uint8 Index = Src.ReadByte();

    /*u8 R, G, B, A;
    ((Index >> 3) & 0x1) ? R = 0xFF : R = 0x0;
    ((Index >> 2) & 0x1) ? G = 0xFF : G = 0x0;
    ((Index >> 1) & 0x1) ? B = 0xFF : B = 0x0;
    ((Index >> 0) & 0x1) ? A = 0xFF : A = 0x0;
    uint32 RGBA = (R << 24) | (G << 16) | (B << 8) | (A);
    dst.WriteLong(RGBA);*/

    void* pPaletteTexel = mPaletteData.data() + (Index * mPaletteTexelStride);
    Dst.WriteBytes(pPaletteTexel, mPaletteTexelStride);
}

void CTextureDecoder::ParseRGB565(IInputStream& Src, IOutputStream& Dst)
{
    // RGB565 can be used as-is.
    Dst.WriteShort(Src.ReadShort());
}

void CTextureDecoder::ParseRGB5A3(IInputStream& Src, IOutputStream& Dst)
{
    uint16 Pixel = Src.ReadShort();
    uint8 R, G, B, A;

    if (Pixel & 0x8000) // RGB5
    {
        B = Extend5to8(Pixel >> 10);
        G = Extend5to8(Pixel >>  5);
        R = Extend5to8(Pixel >>  0);
        A = 255;
    }

    else // RGB4A3
    {
        A = Extend3to8(Pixel >> 12);
        B = Extend4to8(Pixel >>  8);
        G = Extend4to8(Pixel >>  4);
        R = Extend4to8(Pixel >>  0);
    }

    uint32 Color = (A << 24) | (R << 16) | (G << 8) | B;
    Dst.WriteLong(Color);
}

void CTextureDecoder::ParseRGBA8(IInputStream& Src, IOutputStream& Dst)
{
    uint16 AR = Src.ReadShort();
    Src.Seek(0x1E, SEEK_CUR);
    uint16 GB = Src.ReadShort();
    Src.Seek(-0x20, SEEK_CUR);
    uint32 Pixel = (AR << 16) | GB;
    Dst.WriteLong(Pixel);
}

void CTextureDecoder::ParseCMPR(IInputStream& Src, IOutputStream& Dst)
{
    Dst.WriteShort(Src.ReadShort());
    Dst.WriteShort(Src.ReadShort());

    for (uint32 iByte = 0; iByte < 4; iByte++)
    {
        uint8 Byte = Src.ReadByte();
        Byte = ((Byte & 0x3) << 6) | ((Byte & 0xC) << 2) | ((Byte & 0x30) >> 2) | ((Byte & 0xC0) >> 6);
        Dst.WriteByte(Byte);
    }
}

CTextureDecoder::CTextureDecoder(CResourceEntry* pEntry)
    : mpEntry(pEntry)
{
}

CTextureDecoder::~CTextureDecoder()
{
}

// ************ UTILITY ************
uint8 CTextureDecoder::Extend3to8(uint8 In)
{
    In &= 0x7;
    return (In << 5) | (In << 2) | (In >> 1);
}

uint8 CTextureDecoder::Extend4to8(uint8 In)
{
    In &= 0xF;
    return (In << 4) | In;
}

uint8 CTextureDecoder::Extend5to8(uint8 In)
{
    In &= 0x1F;
    return (In << 3) | (In >> 2);
}

uint8 CTextureDecoder::Extend6to8(uint8 In)
{
    In &= 0x3F;
    return (In << 2) | (In >> 4);
}
