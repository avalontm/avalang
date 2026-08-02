#include "branding/logo_texture.h"

// Pulls in the GL function/constant declarations (glGenTextures,
// glTexImage2D, GLuint, GL_TEXTURE_2D, ...) the same way main.cpp does:
// GLFW's own header brings in the platform's OpenGL header for us.
// imgui_impl_opengl3.h alone is NOT enough for this -- it only declares
// the ImGui_ImplOpenGL3_* backend functions, not raw GL symbols, which
// is why building this file with just that include failed with
// "GLuint: identificador no declarado" / "glGenTextures: no se
// encontró el identificador" under MSVC.
#include "GLFW/glfw3.h"

// Fase 10 (image widget preview, designer_canvas.cpp): STBI_ONLY_PNG and
// STBI_NO_STDIO used to be defined here since this file only ever
// decoded the embedded PNG logo from memory. Both are gone now that
// this is the one TU providing the actual stb_image implementation for
// the whole binary -- designer_canvas.cpp needs the file-path loader
// (stbi_load(path, ...), which STBI_NO_STDIO strips out) and arbitrary
// image formats (which STBI_ONLY_PNG would strip out) to preview
// whatever `src` a user points an Image node at, not just PNG.
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "images/avastudio_logo_png.h"

namespace studio::branding {

namespace {

unsigned int g_texture_id = 0;
int g_width = 0;
int g_height = 0;
bool g_load_attempted = false;

} // namespace

unsigned int GetLogoTextureId() {
    if (g_load_attempted) {
        return g_texture_id;
    }
    g_load_attempted = true;

    int channels = 0;
    unsigned char* pixels = stbi_load_from_memory(
        images::kAvaStudioLogoPNG, static_cast<int>(images::kAvaStudioLogoPNGLen), &g_width, &g_height, &channels,
        4 /* force RGBA */);
    if (!pixels) {
        return 0;
    }

    GLuint texture_id = 0;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_width, g_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    stbi_image_free(pixels);

    g_texture_id = texture_id;
    return g_texture_id;
}

int LogoWidth() { return g_width; }
int LogoHeight() { return g_height; }

} // namespace studio::branding
