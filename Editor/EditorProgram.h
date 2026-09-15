#pragma once
#include <memory>

#include "Engine/Runtime/Program.h"

class EditorProgram : public IProgram
{
public:
    EditorProgram();
    ~EditorProgram() override;

    bool Initialize(EngineServices&) override;
    void Update(const FrameContext&) override;
    void Render(RenderContext&) override;
    void Shutdown() override;

private:
    struct Impl;
	std::unique_ptr<Impl> impl_;
};
