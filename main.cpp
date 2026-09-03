#include "Application/Application.h"

int WINAPI WinMain(
    HINSTANCE,
    HINSTANCE,
    LPSTR,
    int)
{
    Application application;
    return application.Run();
}