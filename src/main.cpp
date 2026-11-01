// main.cpp – THERMATRIX entry point
// Windows 11 x64 one-click launch (WIN32 subsystem, no console window)

#include "app/App.h"
#include <cstdlib>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
#else
int main() {
#endif
    thermatrix::App app;
    if (!app.init()) {
        return EXIT_FAILURE;
    }
    app.run();
    return EXIT_SUCCESS;
}
