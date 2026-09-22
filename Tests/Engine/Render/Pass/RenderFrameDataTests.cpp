#include "Engine/Render/Pipeline/Pass/RenderFrameData.h"
#include "Tests/TestFramework.h"

namespace
{
    struct TestPayload
    {
        explicit TestPayload(int value) : value(value) {}
        int value;
    };

    struct OtherPayload
    {
        explicit OtherPayload(int value) : value(value) {}
        int value;
    };
}

GILGAMESH_TEST("Engine.Render.RenderFrameData", "EmplacesAndRetrievesPayload")
{
    RenderFrameData frame;
    TestPayload& inserted = frame.Emplace<TestPayload>(42);

    GILGAMESH_CHECK_MESSAGE(inserted.value == 42,
        "Emplace must construct the payload from its arguments");

    const RenderFrameData& readOnlyFrame = frame;
    const TestPayload* stored = readOnlyFrame.TryGet<TestPayload>();
    GILGAMESH_CHECK_MESSAGE(stored == &inserted && stored->value == 42,
        "TryGet must return the inserted payload through the const view");
}

GILGAMESH_TEST("Engine.Render.RenderFrameData", "MissingTypeReturnsNull")
{
    RenderFrameData frame;
    frame.Emplace<TestPayload>(7);

    GILGAMESH_CHECK_MESSAGE(frame.TryGet<OtherPayload>() == nullptr,
        "looking up a different payload type must return null");
}

GILGAMESH_TEST("Engine.Render.RenderFrameData", "ReplacesPayloadOfSameType")
{
    RenderFrameData frame;
    frame.Emplace<TestPayload>(7);
    frame.Emplace<OtherPayload>(11);

    TestPayload& replacement = frame.Emplace<TestPayload>(29);
    const TestPayload* stored = frame.TryGet<TestPayload>();
    const OtherPayload* other = frame.TryGet<OtherPayload>();

    GILGAMESH_CHECK_MESSAGE(stored == &replacement && stored->value == 29,
        "Emplace must replace an existing payload of the same type");
    GILGAMESH_CHECK_MESSAGE(other != nullptr && other->value == 11,
        "replacing one type must preserve unrelated payloads");
}
