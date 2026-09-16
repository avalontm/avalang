#pragma once

#include <string>

#include "Export.h"
#include "Fwd.h"
#include "components/PropertyValue.h"

namespace avalang {
namespace ui {

class AVA_UI_API IAvaView {
public:
    // Fix: antes `virtual ~IAvaView() = default;` estaba definido inline en
    // el header. Como IAvaView.h nunca lo incluye ningun .cpp de
    // avalang_ui (solo avahost/avanative, ver IAvaView.cpp nuevo), esa
    // definicion inline nunca se compilaba dentro del build de
    // avalang_ui.dll -- el `AVA_UI_API` de la clase pedia exportar el
    // ctor/dtor implicitos, pero nunca se generaban ahi, y avanative
    // (que si la usa via VmAvaView) fallaba en link con LNK2001 al
    // importarlos. Declararlos aca sin cuerpo y definirlos en
    // IAvaView.cpp (agregado a AVA_UI_SOURCES) fuerza a que avalang_ui
    // los compile y exporte de verdad.
    IAvaView();
    virtual ~IAvaView();

    virtual bool Load(const std::string& viewName, const std::string& codeBehind,
                       IComponent* root, std::string& outError) = 0;

    virtual bool OnLoad(std::string& outError) = 0;
    virtual bool OnUnload(std::string& outError) = 0;
    virtual bool InvokeHandler(const std::string& handlerName, std::string& outError) = 0;

    virtual void BindComponentRef(const std::string& id, const PropertyRecord& props) = 0;
    virtual PropertyRecord ExportComponentRef(const std::string& id) = 0;

    virtual PropertyValue GetAttr(const std::string& name) = 0;
    virtual void SetAttr(const std::string& name, PropertyValue value) = 0;

    virtual bool IsLoaded() const = 0;
    virtual const std::string& ViewName() const = 0;
};

}
}
