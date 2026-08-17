#pragma once
#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "state/IState.h"

namespace avahost {

class RuntimeHost;

class VmBackedState final : public avalang::ui::IState {
public:
    VmBackedState(RuntimeHost& host, std::string key, avalang::ui::PropertyValue initial);

    const avalang::ui::PropertyValue& Value() const override;
    void Set(avalang::ui::PropertyValue value) override;

    std::size_t Subscribe(ChangeHandler handler) override;
    void Unsubscribe(std::size_t subscriptionId) override;

    void RefreshFromVm();

    const std::string& Key() const { return key_; }

private:
    void NotifyIfChanged(const avalang::ui::PropertyValue& newValue);

    RuntimeHost& host_;
    std::string key_;
    avalang::ui::PropertyValue value_;
    std::vector<std::pair<std::size_t, ChangeHandler>> subscribers_;
    std::size_t nextSubscriptionId_ = 1;
};

class VmStateBridge {
public:
    explicit VmStateBridge(RuntimeHost& host);

    void Bind(const std::unordered_map<std::string, std::string>& stateSpec);

    void BindWithOverlay(const std::unordered_map<std::string, std::string>& stateSpec,
                          const std::string& cachedStateJson);

    void RefreshAll();

    avalang::ui::IState* Find(const std::string& key) const;

    std::string ExportJson() const;

    std::string EvalIdentifier(const std::string& raw) const;

private:
    RuntimeHost& host_;
    std::string templateStateJson_ = "{}";
    std::vector<std::pair<std::string, std::unique_ptr<VmBackedState>>> states_;
};

}  // namespace avahost
