#pragma once

#include <memory>
#include <string>
#include <vector>

#include "panels/properties_panel.h"
#include "components/ComponentTree.h"
#include "components/IComponent.h"
#include "parser/AvauiParser.h"

namespace studio::design {

struct DesignDocument {
    std::unique_ptr<avalang::ui::ComponentTree> tree;

    std::string code_behind;
    std::vector<PropertyRow> initial_state;
    std::vector<std::string> imports;
    std::string extends;

    std::string selected_node_id;
    bool dirty = false;

    avalang::ui::IComponent* Root() const { return tree ? tree->Root() : nullptr; }
};

std::string GenerateNodeUid();

DesignDocument NewBlankAvauiDocument();






// sourcePath is optional: text coming straight from the editor may not
// be saved to disk yet, so there may be nothing to pass. When it IS
// known (LoadAvauiFile always has it), it's threaded into AvauiParser::
// Parse so a ParseError carries the right file (see Fase 2,
// PLAN_DIAGNOSTICOS_AVAUI.md).
//
// out_info (Fase 4) is filled in, when non-null, with the same
// message/line/column/source a ParseError carried -- structured, so a
// caller like editor_panel.cpp can hand it straight to HighlightError
// instead of only getting the flattened `out_error` string. Mirrors the
// outParseError out-param added to avahost's RenderAvauiDynamic* in
// Fase 3.
bool ParseAvauiText(const std::string& text, DesignDocument& out_doc, std::string& out_error,
                     const std::string& sourcePath = "",
                     avalang::ui::parser::ParseErrorInfo* out_info = nullptr);

bool LoadAvauiFile(const std::string& path, DesignDocument& out_doc, std::string& out_error,
                    avalang::ui::parser::ParseErrorInfo* out_info = nullptr);
bool SaveAvauiFile(const DesignDocument& doc, const std::string& path);

avalang::ui::IComponent* FindNodeById(avalang::ui::IComponent* root, const std::string& nodeId);
avalang::ui::IComponent* FindParentOf(avalang::ui::IComponent* root, avalang::ui::IComponent* target);
bool NodeContains(avalang::ui::IComponent* node, avalang::ui::IComponent* target);

enum class DropZone { kBefore, kInto, kAfter };

bool MoveNode(DesignDocument& doc, const std::string& movedNodeId, const std::string& targetNodeId,
              DropZone zone);

std::string EnsureClickHandler(DesignDocument& doc, const std::string& nodeId);

bool RemoveNode(DesignDocument& doc, const std::string& nodeId);

std::string AddComponentNode(DesignDocument& doc, const std::string& parentId, const std::string& type,
                              const std::string& id, const std::vector<PropertyRow>& properties);

bool EditComponentNode(DesignDocument& doc, const std::string& nodeId, const std::vector<PropertyRow>& properties,
                        const std::string* newId);

}