#include "stdafx.h"
#include "asRoot.h"
#include "asLinearCS.h"

void EnableNanSignal(bool enabled)
{
    return stub<cdecl_t<void, bool>>(0x4C51F0, enabled);
}

asRoot::asRoot()
{
    Matrix.Identity();

    asLinearCS::CurrentMatrix = &Matrix;

    Reset();
}

void asRoot::Init(bool nanSignal)
{
    Reset();

    EnableNan = nanSignal;

    EnableNanSignal(nanSignal);
}

bool asRoot::IsPaused()
{
    return Paused;
}

void asRoot::SetPause(bool paused)
{
    Paused = paused;
}

void asRoot::TogglePause()
{
    Paused = !Paused;
}

void asRoot::Update(void)
{
    if (EnableNan)
    {
        EnableNanSignal(true);
    }

    asLinearCS::CurrentMatrix = &Matrix;

    asNode::Update();

    if (ShouldPause)
    {
        ShouldPause = false;
        Paused = true;
    }
}

void asRoot::Reset(void)
{
    Paused = false;
    ShouldPause = false;
}

char* asRoot::GetClassName(void)
{
    return "asRoot";
}
