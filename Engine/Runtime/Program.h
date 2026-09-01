#pragma once

struct EngineServices;
struct FrameContext;
struct RenderContext;

class IProgram
{
public:
	virtual ~IProgram() = default;

    virtual bool Initialize(EngineServices&) = 0;
    virtual void Update(const FrameContext&) = 0;
    virtual void Render(RenderContext&) = 0;
    virtual void Shutdown() = 0;

    bool isProgramAlive = false;
};