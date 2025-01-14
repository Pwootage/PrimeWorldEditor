#ifndef CTEXTURE_H
#define CTEXTURE_H

#include "CResource.h"
#include "ETexelFormat.h"
#include <Common/BasicTypes.h>
#include <Common/FileIO.h>
#include <Common/Math/CVector2f.h>

#include <GL/glew.h>

class CTexture : public CResource
{
    DECLARE_RESOURCE_TYPE(Texture)
    friend class CTextureDecoder;
    friend class CTextureEncoder;

    ETexelFormat mTexelFormat;          // Format of decoded image data
    ETexelFormat mSourceTexelFormat;    // Format of input TXTR file
    uint16 mWidth, mHeight;             // Image dimensions
    uint32 mNumMipMaps;                 // The number of mipmaps this texture has
    uint32 mLinearSize;                 // The size of the top level mipmap, in bytes

    bool mEnableMultisampling;  // Whether multisample should be enabled (if this texture is a render target).
    bool mBufferExists;         // Indicates whether image data buffer has valid data
    uint8 *mpImgDataBuffer;     // Pointer to image data buffer
    uint32 mImgDataSize;        // Size of image data buffer

    bool mGLBufferExists; // Indicates whether GL buffer has valid data
    GLuint mTextureID;    // ID for texture GL buffer

public:
    CTexture(CResourceEntry *pEntry = 0);
    CTexture(uint32 Width, uint32 Height);
    ~CTexture();

    bool BufferGL();
    void Bind(uint32 GLTextureUnit);
    void Resize(uint32 Width, uint32 Height);
    float ReadTexelAlpha(const CVector2f& rkTexCoord);
    bool WriteDDS(IOutputStream& rOut);

    // Accessors
    ETexelFormat TexelFormat() const        { return mTexelFormat; }
    ETexelFormat SourceTexelFormat() const  { return mSourceTexelFormat; }
    uint32 Width() const                    { return (uint32) mWidth; }
    uint32 Height() const                   { return (uint32) mHeight; }
    uint32 NumMipMaps() const               { return mNumMipMaps; }
    GLuint TextureID() const                { return mTextureID; }

    inline void SetMultisamplingEnabled(bool Enable)
    {
        if (mEnableMultisampling != Enable)
            DeleteBuffers();

        mEnableMultisampling = Enable;
    }

    // Static
    static uint32 FormatBPP(ETexelFormat Format);

    // Private
private:
    void CalcLinearSize();
    uint32 CalcTotalSize();
    void CopyGLBuffer();
    void DeleteBuffers();
};

#endif // CTEXTURE_H
