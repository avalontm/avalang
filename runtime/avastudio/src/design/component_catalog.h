#pragma once

#include <string>
#include <vector>

#include "panels/properties_panel.h" // reuse PropertyRow -- same {key, value} shape
                                      // Properties already displays, no need for a
                                      // parallel struct.

namespace studio::design {

// Static description of one component type ("button", "stack", ...):
// what the Toolbox lists, what a freshly-dropped instance starts with,
// and whether it's allowed to be a drop target for other components.
//
// This is the piece flagged as missing in
// docs/architecture/08_DESIGNER_VIEW_PLAN.md section 2.4 -- today
// EngineBridge::BuildDemoComponentTree() hardcodes one fixed tree by
// hand; nothing enumerates "what component types exist and what are
// their default props" generically. Toolbox (drag source) and "new
// node from a drop" (Designer canvas) both need that list, so it lives
// here instead of being duplicated in both panels.
struct ComponentTypeInfo {
    std::string type;         // matches AvaComponent's type string, e.g. "button"
    std::string display_name; // Toolbox label, e.g. "Button"
    std::vector<PropertyRow> default_properties; // seeded onto a new instance
    bool is_container = false; // true = valid drop target for other components
    int order = 0;             // Toolbox row order, from data/component_catalog.csv
    std::string category;      // Toolbox group header, from data/component_catalog.csv
    std::string icon;          // reserved for a future real icon set
};

// Fase 9 (plan unificado avastudio/avaui, Parte B): construido a partir
// de `avalang::ui::registry::GetComponentTypeRegistry()` -- avaui es la
// unica fuente de verdad para tipos y defaults; ver B.4 en
// PLAN_UNIFICADO_AVAUI.md. Devuelve una referencia a un static
// local-a-la-funcion, construido una sola vez (el registro de avaui no
// cambia en caliente), seguro de llamar cada frame desde el panel del
// Toolbox.
const std::vector<ComponentTypeInfo>& GetComponentCatalog();

// Looks up one entry by ComponentTypeInfo::type. Returns nullptr if
// `type` isn't in the catalog (e.g. a .avaui file referencing a
// component type from a future/unknown version of the catalog -- the
// caller decides whether that's a load error or just "show it with no
// known default properties").
const ComponentTypeInfo* FindComponentType(const std::string& type);

} // namespace studio::design
