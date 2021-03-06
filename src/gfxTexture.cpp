#include "stdafx.h"
#include "gfxTexture.h"

#include "gfxImage.h"
#include "gfxPipeline.h"
#include "d3dpipe.h"

static_var(0x6844C4, bool, g_Tex565);

DDPIXELFORMAT ddPixelFormat_8888 = { sizeof(DDPIXELFORMAT), DDPF_RGB | DDPF_ALPHAPIXELS, 0, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000 };
DDPIXELFORMAT ddPixelFormat_0888 = { sizeof(DDPIXELFORMAT), DDPF_RGB,                    0, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00000000 };

DDPIXELFORMAT ddPixelFormat_1555 = { sizeof(DDPIXELFORMAT), DDPF_RGB | DDPF_ALPHAPIXELS, 0, 16, 0x7C00, 0x03E0, 0x001F, 0x8000 };
DDPIXELFORMAT ddPixelFormat_4444 = { sizeof(DDPIXELFORMAT), DDPF_RGB | DDPF_ALPHAPIXELS, 0, 16, 0x0F00, 0x00F0, 0x000F, 0xF000 };
DDPIXELFORMAT ddPixelFormat_0565 = { sizeof(DDPIXELFORMAT), DDPF_RGB,                    0, 16, 0xF800, 0x07E0, 0x001F, 0x0000 };
DDPIXELFORMAT ddPixelFormat_0555 = { sizeof(DDPIXELFORMAT), DDPF_RGB,                    0, 16, 0x7C00, 0x03E0, 0x001F, 0x0000 };

DDPIXELFORMAT ddPixelFormat_Pallete8 = { sizeof(DDPIXELFORMAT), DDPF_RGB | DDPF_PALETTEINDEXED8, 0, 8, 0, 0, 0, 0 };
DDPIXELFORMAT ddPixelFormat_Pallete4 = { sizeof(DDPIXELFORMAT), DDPF_RGB | DDPF_PALETTEINDEXED4, 0, 4, 0, 0, 0, 0 };

PALETTEENTRY paletteEntry;

gfxTexture::gfxTexture()
{
    PrevLOD = sm_First;

    sm_First = this;
}

gfxTexture::~gfxTexture()
{
    if (CacheEntry)
    {
        CacheEntry->Evict();
    }

    delete Name;

    gfxTexture** ppFirst = &gfxTexture::sm_First;

    for (gfxTexture* i = gfxTexture::sm_First ; i; i = i->PrevLOD )
    {
        if ( i == this )
        {
            break;
        }

        ppFirst = &i->PrevLOD;
    }

    *ppFirst = PrevLOD;

    if ( m_Surface )
    {
        gfxDebugf(gfxDebug, "Releasing %s: %d", "m_Surface", m_Surface->Release());
        m_Surface = 0;
    }

    if ( m_Palette )
    {
        gfxDebugf(gfxDebug, "Releasing %s: %d", "m_Palette", m_Palette->Release());
        m_Palette = 0;
    }
}

void gfxTexture::Load(gfxImage * image)
{
    return stub<thiscall_t<void, gfxTexture, gfxImage*>>(0x4AD100, this, image);
}

void gfxTexture::SetTexEnv(int texEnv)
{
    TexEnv = texEnv;
}

IDirectDrawSurface7* gfxTexture::FindEntry()
{
    CachePool->FindEntry(this);

    CacheEntry->LastAccessTime = gfxTextureCacheEntry::sm_CurrentTime++;

    return CacheEntry->Surface;
}

IDirectDrawSurface7* gfxTexture::GetResidentSurface()
{
    if (CachePool)
    {
        if (CacheEntry)
        {
            CacheEntry->LastAccessTime = gfxTextureCacheEntry::sm_CurrentTime++;

            return CacheEntry->Surface;
        }

        return FindEntry();
    }

    return this->m_Surface;
}

void gfxTexture::MarkFirstUse()
{
    NextLOD = std::exchange(sm_FirstActive, this);

    MarkHigherUse();
}

void gfxTexture::MarkHigherUse()
{
    m_LOD = std::min<int8_t>(sm_LOD, m_MaxLOD);

    if (m_MaxLOD)
    {
        if (sm_EnableSetLOD)
        {
            if (m_Surface)
            {
                DX_ASSERT(m_Surface->SetLOD(m_LOD));
            }
        }
    }
}

gfxTexture * gfxTexture::Create(gfxImage * image, bool mipMap)
{
    if (image == nullptr)
    {
        return nullptr;
    }

    int mipMapCount = 0;

    if (mipMap)
    {
        for (gfxImage* i = image; i; i = i->Next)
        {
            ++mipMapCount;
        }
    }

    gfxTexture* texture = Create(
        image->Width,
        image->Height,
        (gfxImage::gfxImageFormat) image->Type,
        (gfxImage::gfxImageFormat) image->PaletteType,
        mipMapCount);

    if (texture)
    {
        texture->Load(image);

        texture->SetTexEnv(image->TexEnv | texture->TexEnv);
    }

    return texture;
}

uint32_t ToPow2(uint32_t size)
{
    if (size < 4)
    {
        return 4;
    }

    if (size < 8)
    {
        return 8;
    }

    if (size < 16)
    {
        return 16;
    }

    if (size < 32)
    {
        return 32;
    }

    if (size < 64)
    {
        return 64;
    }

    if (size < 128)
    {
        return 128;
    }

    if (size < 256)
    {
        return 256;
    }

    if (size < 512)
    {
        return 512;
    }

    if (size < 1024)
    {
        return 1024;
    }

    return 2048;
}

gfxTexture * gfxTexture::Create(int width, int height, gfxImage::gfxImageFormat type, gfxImage::gfxImageFormat paletteType, int mipMapCount)
{
    (void)paletteType;

    if (!lpDD)
    {
        Warningf("PIPE not started, cannot create textures.");

        return 0;
    }

    gfxTexture::ShutdownCache();

    IDirectDrawSurface7 *ddrawSurface = nullptr;
    IDirectDrawPalette *ddrawPalette = nullptr;
    DDPIXELFORMAT *pixelFormat = nullptr;

    DDSURFACEDESC2 ddSurfaceDesc;
    memset(&ddSurfaceDesc, 0, sizeof(ddSurfaceDesc));
    ddSurfaceDesc.dwSize = sizeof(DDSURFACEDESC2);
    ddSurfaceDesc.dwFlags = 0x1007;
    ddSurfaceDesc.dwWidth = ToPow2(width);
    ddSurfaceDesc.dwHeight = ToPow2(height);
    ddSurfaceDesc.ddsCaps.dwCaps = 0x1000;

    if (useSoftware || gfxTexture::sm_UseInternalCache)
    {
        ddSurfaceDesc.ddsCaps.dwCaps = 0x1800;
    }
    else
    {
        ddSurfaceDesc.ddsCaps.dwCaps2 = 0x20000;
    }

    if (mipMapCount)
    {
        ddSurfaceDesc.ddsCaps.dwCaps |= 0x400008;
        ddSurfaceDesc.dwFlags |= 0x20000u;
        ddSurfaceDesc.dwMipMapCount = mipMapCount;
    }

    bool hasAlpha = 0;

    switch (type)
    {
    case gfxImage::rif8888:
        memcpy(&ddSurfaceDesc.ddpfPixelFormat, &ddPixelFormat_8888, sizeof(ddSurfaceDesc.ddpfPixelFormat));

        if (!gfxTexture::sm_Allow32 || lpDD->CreateSurface(&ddSurfaceDesc, &ddrawSurface, 0) != DD_OK)
        {
            memcpy(&ddSurfaceDesc.ddpfPixelFormat, &ddPixelFormat_4444, sizeof(ddSurfaceDesc.ddpfPixelFormat));

            DX_ASSERT(lpDD->CreateSurface(&ddSurfaceDesc, &ddrawSurface, 0));
        }
        hasAlpha = 1;
        break;
    case gfxImage::rif888:
        memcpy(&ddSurfaceDesc.ddpfPixelFormat, &ddPixelFormat_0888, sizeof(ddSurfaceDesc.ddpfPixelFormat));
        if (!gfxTexture::sm_Allow32 || lpDD->CreateSurface(&ddSurfaceDesc, &ddrawSurface, 0) != DD_OK)
        {
            pixelFormat = &ddPixelFormat_0565;

            if (!g_Tex565)
            {
                pixelFormat = &ddPixelFormat_0555;
            }

            memcpy(&ddSurfaceDesc.ddpfPixelFormat, pixelFormat, sizeof(ddSurfaceDesc.ddpfPixelFormat));

            DX_ASSERT(lpDD->CreateSurface(&ddSurfaceDesc, &ddrawSurface, 0));
        }
        break;
    case gfxImage::rif555:
        pixelFormat = &ddPixelFormat_0565;

        if (!g_Tex565)
        {
            pixelFormat = &ddPixelFormat_0555;
        }

        memcpy(&ddSurfaceDesc.ddpfPixelFormat, pixelFormat, sizeof(ddSurfaceDesc.ddpfPixelFormat));

        DX_ASSERT(lpDD->CreateSurface(&ddSurfaceDesc, &ddrawSurface, 0));
        break;
    case gfxImage::rif5551:
        memcpy(&ddSurfaceDesc.ddpfPixelFormat, &ddPixelFormat_1555, sizeof(ddSurfaceDesc.ddpfPixelFormat));
        lpDD->CreateSurface(&ddSurfaceDesc, &ddrawSurface, 0);
        hasAlpha = 1;
        break;
    case gfxImage::rif8:
        memcpy(&ddSurfaceDesc.ddpfPixelFormat, &ddPixelFormat_Pallete8, sizeof(ddSurfaceDesc.ddpfPixelFormat));
        if (lpDD->CreateSurface(&ddSurfaceDesc, &ddrawSurface, 0) != DD_OK)
        {
            memcpy(&ddSurfaceDesc.ddpfPixelFormat, &ddPixelFormat_0555, sizeof(ddSurfaceDesc.ddpfPixelFormat));

            lpDD->CreateSurface(&ddSurfaceDesc, &ddrawSurface, 0);
        }
        else
        {
            DX_ASSERT(lpDD->CreatePalette(68, &paletteEntry, &ddrawPalette, 0));
            DX_ASSERT(ddrawSurface->SetPalette(ddrawPalette));
        }
        break;
    case gfxImage::rif4:
        memcpy(&ddSurfaceDesc.ddpfPixelFormat, &ddPixelFormat_Pallete4, sizeof(ddSurfaceDesc.ddpfPixelFormat));
        if (lpDD->CreateSurface(&ddSurfaceDesc, &ddrawSurface, 0) != DD_OK)
        {
            memcpy(&ddSurfaceDesc.ddpfPixelFormat, &ddPixelFormat_Pallete8, sizeof(ddSurfaceDesc.ddpfPixelFormat));
            if (lpDD->CreateSurface(&ddSurfaceDesc, &ddrawSurface, 0) != DD_OK)
            {
                memcpy(&ddSurfaceDesc.ddpfPixelFormat, &ddPixelFormat_0555, sizeof(ddSurfaceDesc.ddpfPixelFormat));

                lpDD->CreateSurface(&ddSurfaceDesc, &ddrawSurface, 0);
            }
            else
            {
                DX_ASSERT(lpDD->CreatePalette(68, &paletteEntry, &ddrawPalette, 0));
                DX_ASSERT(ddrawSurface->SetPalette(ddrawPalette));
            }
        }
        else
        {
            DX_ASSERT(lpDD->CreatePalette(1, &paletteEntry, &ddrawPalette, 0));
            DX_ASSERT(ddrawSurface->SetPalette(ddrawPalette));
        }
        break;
    default:
        Abortf("Unsupported texture format!");
        break;
    }

    if (!ddrawSurface)
    {
        return 0;
    }

    gfxTexture* texture = new gfxTexture();
    texture->Width = (uint16_t) ddSurfaceDesc.dwWidth;
    texture->Height = (uint16_t) ddSurfaceDesc.dwHeight;
    texture->m_Surface = ddrawSurface;
    texture->m_Palette = ddrawPalette;

    uint32_t texEnv = useSoftware != 0 ? 0 : 0x40000;

    if (hasAlpha)
    {
        texEnv |= 0x20000u;
    }

    texture->TexEnv = texEnv;

    if (mipMapCount)
    {
        if (!useSoftware)
        {
            texture->TexEnv = texEnv | 0x80000;
        }
    }

    texture->m_MaxLOD = (int8_t) mipMapCount;

    return texture;
}

void gfxTexture::EnableCache(bool enabled)
{
    if (gfxTexture::sm_First)
    {
        Quitf("Can't call EnableCache if you already have textures defined.");
    }

    gfxTexture::sm_UseInternalCache = enabled;
}

void gfxTexture::ShutdownCache(void)
{

    if (gfxTexture::sm_FirstPool)
    {
        Warningf("gfxTexture::ShutdownCache: Shutting down texture cache.");

        lpD3DDev->SetTexture(0, 0);
        lpD3DDev->SetTexture(1, 0);

        for (gfxTexture* i = gfxTexture::sm_First; i; i = i->PrevLOD)
        {
            gfxTextureCacheEntry* cacheEntry = i->CacheEntry;

            if (cacheEntry)
            {
                cacheEntry->Evict();
            }

            i->CachePool = nullptr;
        }

        gfxTextureCachePool *currentPool = gfxTexture::sm_FirstPool;

        while (currentPool)
        {
            currentPool = currentPool->PrevPool;
            gfxTexture::sm_FirstPool = currentPool;

            if (currentPool)
            {
                delete currentPool;

                currentPool = gfxTexture::sm_FirstPool;
            }
        }

        gfxTextureCacheEntry::sm_CurrentTime = 0;
    }
}

gfxTextureCachePool::gfxTextureCachePool(gfxTextureCachePool* prevPool)
{
    memset(this, 0, sizeof(gfxTextureCachePool));

    PrevPool = prevPool;
}

gfxTextureCachePool::~gfxTextureCachePool()
{
    for (gfxTextureCacheEntry* i = FirstEntry; i; i = FirstEntry)
    {
        FirstEntry = i->PrevEntry;

        delete i;
    }
}

void gfxTextureCachePool::FindEntry(gfxTexture* texture)
{
    gfxTextureCacheEntry *entry = FirstEntry;

    uint32_t earliest = entry->LastAccessTime;

    for (entry = entry->PrevEntry; entry; entry = entry->PrevEntry)
    {
        if (!earliest)
        {
            break;
        }
        if (entry->LastAccessTime < earliest)
        {
            earliest = entry->LastAccessTime;
        }
    }

    entry->Evict();
    entry->Lease(texture);
}

gfxTextureCacheEntry::gfxTextureCacheEntry(IDirectDrawSurface7 * surface, gfxTextureCacheEntry * prevEntry)
    : Surface(surface)
    , PrevEntry(prevEntry)
{ }

gfxTextureCacheEntry::~gfxTextureCacheEntry()
{
    Evict();

    Surface->Release();
    Surface = nullptr;
}

void gfxTextureCacheEntry::Lease(gfxTexture* texture)
{
    Texture = texture;
    texture->CacheEntry = this;
    lpD3DDev->Load(Surface, nullptr, texture->m_Surface, nullptr, 0);
    texture->m_Surface->AddRef();
}

void gfxTextureCacheEntry::Evict()
{
    if (Texture)
    {
        Texture->m_Surface->Release();
        Texture->CacheEntry = nullptr;
        Texture = nullptr;
    }
}
