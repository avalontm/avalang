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
