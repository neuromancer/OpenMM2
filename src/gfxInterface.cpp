#include "stdafx.h"
#include "gfxInterface.h"

defnvar(0x683130, gfxInterfaces);
defnvar(0x6844C0, gfxInterfaceCount);
defnvar(0x6844C8, gfxInterfaceChoice);

gfxResData * gfxInterface::CurrentResolution()
{
    return &Resolutions[ResolutionChoice];
}

gfxInterface * gfxInterface::Current()
{
    return &gfxInterfaces[gfxInterfaceChoice];
}