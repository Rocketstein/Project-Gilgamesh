#include "Editor/EditorCamera/EditorCamera.h"
#include "Tests/TestFramework.h"

#include <numbers>

GILGAMESH_TEST("Editor.Camera", "AppliesPendingStateOnUpdate")
{
    EditorCamera camera;
    const Vector3 initialPosition = camera.GetPosition();
    const Vector3 newPosition{ 1.0f, 2.0f, 3.0f };

    camera.SetPosition(newPosition);
    GILGAMESH_CHECK_MESSAGE(camera.GetPosition() == initialPosition,
        "camera setters must not modify the current frame state immediately");

    camera.Update();
    GILGAMESH_CHECK_MESSAGE(camera.GetPosition() == newPosition,
        "Update must commit the pending camera state");
}

GILGAMESH_TEST("Editor.Camera", "BuildsLeftHandedViewMatrix")
{
    EditorCamera camera;
    const Vector3 position{ -3.0f, 0.0f, 0.0f };
    camera.SetPosition(position);
    camera.SetYaw(0.0f);
    camera.SetPitch(0.0f);
    camera.Update();

    const Matrix4 view = camera.GetViewMatrix();
    GILGAMESH_CHECK_MESSAGE(NearlyEquals(
        TransformPoint(position, view),
        Vector3{}),
        "the camera position must transform to the view-space origin");
    GILGAMESH_CHECK_MESSAGE(NearlyEquals(
        TransformDirection({ 1.0f, 0.0f, 0.0f }, view),
        Vector3{ 0.0f, 0.0f, 1.0f }),
        "world forward must map to positive view-space Z");
    GILGAMESH_CHECK_MESSAGE(NearlyEquals(
        TransformDirection({ 0.0f, 0.0f, 1.0f }, view),
        Vector3{ 0.0f, 1.0f, 0.0f }),
        "world up must map to positive view-space Y");
}

GILGAMESH_TEST("Editor.Camera", "BuildsProjectionMatrices")
{
    constexpr float nearPlane = 0.1f;
    constexpr float farPlane = 1000.0f;
    constexpr float orthographicHeight = 10.0f;
    constexpr float fieldOfView = std::numbers::pi_v<float> / 3.0f;
    constexpr float aspectRatio = 2.0f;

    EditorCamera camera;
    camera.SetFOV(fieldOfView);
    camera.Update();

    GILGAMESH_CHECK_MESSAGE(NearlyEquals(
        camera.GetProjectionMatrix(aspectRatio),
        PerspectiveFovLH(fieldOfView, aspectRatio, nearPlane, farPlane)),
        "perspective projection must use the current FOV and aspect ratio");
    GILGAMESH_CHECK_MESSAGE(NearlyEquals(
        camera.GetProjectionMatrix(0.0f),
        PerspectiveFovLH(fieldOfView, 1.0f, nearPlane, farPlane)),
        "non-positive aspect ratios must fall back to one");

    camera.SetOrthographic(true);
    camera.Update();
    GILGAMESH_CHECK_MESSAGE(NearlyEquals(
        camera.GetProjectionMatrix(aspectRatio),
        OrthographicLH(
            orthographicHeight * aspectRatio,
            orthographicHeight,
            nearPlane,
            farPlane)),
        "orthographic projection must preserve its configured view height");
}
