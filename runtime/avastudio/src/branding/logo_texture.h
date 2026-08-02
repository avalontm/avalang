#pragma once

namespace studio::branding {

// Decodes the embedded Ava Studio logo (baked into the exe from
// avastudio.png, see src/images/avastudio_logo_png.h) and uploads it as
// an OpenGL texture the first time it's called; every call after that
// just returns the cached texture id. Must be called after
// ImGui_ImplOpenGL3_Init() (it needs a live GL context) -- in practice,
// from inside the render loop, e.g. titlebar_panel.cpp.
//
// Returns 0 if decoding or upload ever failed, so callers can fall back
// to a plain drawn icon instead of showing a blank/garbage image.
unsigned int GetLogoTextureId();

// Pixel dimensions of the decoded logo (0 until GetLogoTextureId() has
// been called at least once). Useful for preserving aspect ratio if the
// logo is ever drawn at a size other than square.
int LogoWidth();
int LogoHeight();

} // namespace studio::branding
