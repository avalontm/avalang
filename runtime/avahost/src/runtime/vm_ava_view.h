#pragma once

#include <string>
#include <unordered_set>

#include "avalang.h"
#include "components/IComponent.h"
#include "components/PropertyValue.h"
#include "view/IAvaView.h"

namespace avahost {

class VmAvaView final : public avalang::ui::IAvaView {
public:
    explicit VmAvaView(AvaVM* vm);
    ~VmAvaView() override;

    VmAvaView(const VmAvaView&) = delete;
    VmAvaView& operator=(const VmAvaView&) = delete;

    bool Load(const std::string& viewName, const std::string& codeBehind,
              avalang::ui::IComponent* root, std::string& outError) override;

    bool OnLoad(std::string& outError) override;
    bool OnUnload(std::string& outError) override;
    bool InvokeHandler(const std::string& handlerName, std::string& outError) override;

    void BindComponentRef(const std::string& id, const avalang::ui::PropertyRecord& props) override;
    avalang::ui::PropertyRecord ExportComponentRef(const std::string& id) override;

    avalang::ui::PropertyValue GetAttr(const std::string& name) override;
    void SetAttr(const std::string& name, avalang::ui::PropertyValue value) override;

    bool IsLoaded() const override { return loaded_; }
    const std::string& ViewName() const override { return viewName_; }

private:
    void BindComponentRefsRecursive(avalang::ui::IComponent* node,
                                     std::unordered_set<std::string>& seenIds);

    AvaVM* vm_;
    ava_value_t instance_;
    std::string viewName_;
    bool loaded_ = false;
};

}
