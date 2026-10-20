#include "LibraryAssets.h"
#include <juce_cryptography/juce_cryptography.h>
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shobjidl.h>
#include <propkey.h>
#include <propvarutil.h>
#include <wincodec.h>
#include <wrl/client.h>
#endif

namespace mixworks
{
namespace
{
constexpr int thumbnailSize = 64;
#if JUCE_WINDOWS
using Microsoft::WRL::ComPtr;
struct ComApartment
{
    HRESULT status = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ~ComApartment() { if (SUCCEEDED(status)) CoUninitialize(); }
    bool ready() const { return SUCCEEDED(status) || status == RPC_E_CHANGED_MODE; }
};
juce::String property(IPropertyStore& store, REFPROPERTYKEY key)
{
    PROPVARIANT value; PropVariantInit(&value); juce::String text;
    if (SUCCEEDED(store.GetValue(key, &value)))
    {
        PWSTR result = nullptr;
        if (SUCCEEDED(PropVariantToStringAlloc(value, &result))) { text = juce::String(result).trim().substring(0, 2048); CoTaskMemFree(result); }
    }
    PropVariantClear(&value); return text;
}
juce::Image audioCover(IShellItemImageFactory& factory)
{
    HBITMAP bitmap = nullptr;
    const auto status = factory.GetImage({thumbnailSize, thumbnailSize}, SIIGBF_THUMBNAILONLY, &bitmap);
    struct Release { HBITMAP handle; ~Release() { if (handle) DeleteObject(handle); } } release {bitmap};
    if (FAILED(status)) return {};
    BITMAP dimensions {};
    if (!bitmap || !GetObject(bitmap, sizeof(dimensions), &dimensions) || dimensions.bmWidth < 1 || dimensions.bmWidth > thumbnailSize || dimensions.bmHeight < 1 || dimensions.bmHeight > thumbnailSize) return {};
    BITMAPINFO info {}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth = dimensions.bmWidth; info.bmiHeader.biHeight = -dimensions.bmHeight;
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
    std::vector<juce::uint32> pixels((size_t) dimensions.bmWidth * dimensions.bmHeight);
    const auto dc = GetDC(nullptr); const int rows = GetDIBits(dc, bitmap, 0, (UINT) dimensions.bmHeight, pixels.data(), &info, DIB_RGB_COLORS); ReleaseDC(nullptr, dc);
    if (rows != dimensions.bmHeight) return {};
    juce::Image image(juce::Image::RGB, dimensions.bmWidth, dimensions.bmHeight, false, juce::SoftwareImageType());
    juce::Image::BitmapData data(image, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < dimensions.bmHeight; ++y) for (int x = 0; x < dimensions.bmWidth; ++x)
    { const auto pixel = pixels[(size_t) y * dimensions.bmWidth + x]; data.setPixelColour(x, y, juce::Colour::fromRGB((juce::uint8) (pixel >> 16), (juce::uint8) (pixel >> 8), (juce::uint8) pixel)); }
    return image;
}
juce::Image folderCover(const juce::File& file)
{
    // Bound compressed input and decoded source dimensions before constructing a scaler.
    if (!file.existsAsFile() || file.isSymbolicLink() || file.getSize() < 1 || file.getSize() > 16 * 1024 * 1024) return {};
    ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))) return {};
    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory->CreateDecoderFromFilename(file.getFullPathName().toWideCharPointer(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder))) return {};
    ComPtr<IWICBitmapFrameDecode> frame; UINT width = 0, height = 0;
    if (FAILED(decoder->GetFrame(0, &frame)) || FAILED(frame->GetSize(&width, &height)) || width < 1 || height < 1 || (uint64_t) width * height > 16 * 1024 * 1024) return {};
    const double scale = std::min(1.0, thumbnailSize / (double) std::max(width, height));
    const int w = std::max(1, (int) std::round(width * scale)), h = std::max(1, (int) std::round(height * scale));
    ComPtr<IWICBitmapScaler> scaler; ComPtr<IWICFormatConverter> converter;
    if (FAILED(factory->CreateBitmapScaler(&scaler)) || FAILED(scaler->Initialize(frame.Get(), (UINT) w, (UINT) h, WICBitmapInterpolationModeFant))
        || FAILED(factory->CreateFormatConverter(&converter)) || FAILED(converter->Initialize(scaler.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom))) return {};
    juce::Image image(juce::Image::ARGB, w, h, true, juce::SoftwareImageType()); juce::Image::BitmapData data(image, juce::Image::BitmapData::writeOnly);
    if (FAILED(converter->CopyPixels(nullptr, (UINT) data.lineStride, (UINT) (data.lineStride * h), data.data))) return {};
    return image;
}
#endif
}
void LibraryAssets::prepareArtworkGrouping()
{
    artworkGroup.clear(); if (!cover.isValid()) return;
    // Use the same software pixel format before scaling imported WIC and reloaded PNG covers.
    const auto image = juce::SoftwareImageType().convert(cover).convertedToFormat(juce::Image::ARGB).rescaled(32, 32, juce::Graphics::highResamplingQuality);
    juce::MemoryOutputStream pixels;
    for (int y = 0; y < 32; ++y) for (int x = 0; x < 32; ++x) pixels.writeIntBigEndian((int) image.getPixelAt(x, y).getARGB());
    artworkGroup = juce::SHA256(pixels.getData(), pixels.getDataSize()).toHexString();
}
LibraryAssets readLibraryAssets(const juce::File& audio)
{
    LibraryAssets result;
#if JUCE_WINDOWS
    ComApartment apartment; if (!apartment.ready()) return result;
    ComPtr<IShellItem2> item;
    if (SUCCEEDED(SHCreateItemFromParsingName(audio.getFullPathName().toWideCharPointer(), nullptr, IID_PPV_ARGS(&item))))
    {
        ComPtr<IPropertyStore> properties;
        if (SUCCEEDED(item->GetPropertyStore(GPS_DEFAULT, IID_PPV_ARGS(&properties))))
        { result.title = property(*properties.Get(), PKEY_Title); result.artist = property(*properties.Get(), PKEY_Music_Artist); result.album = property(*properties.Get(), PKEY_Music_AlbumTitle); }
        ComPtr<IShellItemImageFactory> factory;
        if (SUCCEEDED(item.As(&factory))) result.cover = audioCover(*factory.Get());
        if (result.cover.isValid()) { result.coverSource = audio; result.audioThumbnail = true; result.prepareArtworkGrouping(); return result; }
    }
    juce::StringArray stems {audio.getFileNameWithoutExtension(), "cover", "folder", "front", "album"};
    for (const auto& stem : stems) for (const auto& extension : {"jpg", "jpeg", "png"})
    {
        const auto candidate = audio.getSiblingFile(stem + "." + extension); result.cover = folderCover(candidate);
        if (result.cover.isValid()) { result.coverSource = candidate; result.prepareArtworkGrouping(); return result; }
    }
#else
    juce::ignoreUnused(audio);
#endif
    return result;
}
}
