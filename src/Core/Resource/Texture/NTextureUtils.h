#ifndef NTEXTUREUTILS_H
#define NTEXTUREUTILS_H

#include "ETexelFormat.h"
#include <Common/BasicTypes.h>
#include <Common/TString.h>
#include <vector>

/**
 * Various utility functions for working with textures and images.
 * For import functions, the following formats are supported:
 * JPG, PNG, TGA, BMP, PSD, GIF, HDR, PIC
 **/
namespace NTextureUtils
{

/**
 * Retrieve the format info for a given texel format.
 * Note: EditorTexelFormat field is invalid for palette formats (C4, C8, C14x2).
 * For these, fetch the format info of the underlying texel format.
 */
const STexelFormatInfo& GetTexelFormatInfo(EGXTexelFormat Format);
const STexelFormatInfo& GetTexelFormatInfo(ETexelFormat Format);

/** Import the image file at the given path. Returns true if succeeded. */
bool LoadImageFromFile(const TString& kPath,
                       std::vector<uint8>& OutBuffer,
                       int& OutSizeX,
                       int& OutSizeY,
                       int& OutNumChannels);

/** Import an image file from a buffer. Returns true if succeeded. */
bool LoadImageFromMemory(void* pData,
                         uint DataSize,
                         std::vector<uint8>& OutBuffer,
                         int& OutSizeX,
                         int& OutSizeY,
                         int& OutNumChannels);

}

#endif // NTEXTUREUTILS_H
