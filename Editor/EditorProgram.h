#pragma once
#include "Engine/Runtime/Program.h"

class EditorProgram : public IProgram
{
public:
    bool Initialize(EngineServices&) override;
    void Update(const FrameContext&) override;
    void Render(RenderContext&) override;
    void Shutdown() override;

private:

};