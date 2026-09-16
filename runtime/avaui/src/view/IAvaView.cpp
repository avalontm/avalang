#include "view/IAvaView.h"

// Fix (LNK2001 en avanative: __imp_??0IAvaView.../__imp_??1IAvaView...):
// ver el comentario en IAvaView.h. IAvaView era una interfaz 100%
// header-only que ningun .cpp de avalang_ui incluia -- el compilador
// nunca la procesaba al construir avalang_ui.dll, asi que su ctor/dtor
// implicitos (pedidos por AVA_UI_API en la clase) nunca se generaban ni
// exportaban ahi. Este .cpp (agregado a AVA_UI_SOURCES en
// runtime/avaui/CMakeLists.txt) es lo minimo necesario para que
// avalang_ui.dll realmente los compile y exporte, de forma que
// consumidores en otra DLL (avahost/avanative, via VmAvaView) puedan
// importarlos en vez de fallar en link.

namespace avalang {
namespace ui {

IAvaView::IAvaView() = default;
IAvaView::~IAvaView() = default;

}  // namespace ui
}  // namespace avalang
