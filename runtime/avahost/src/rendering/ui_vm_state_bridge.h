#pragma once
// AvaHost.Rendering.UiVmStateBridge -- Fase 20.0 (puente VM <-> avaui,
// runtime/avaui/docs/AVAUI_FASE20_INTEGRATION.md "Que falta para
// 20.2+" item 1). Prerequisite the freeze plan's Phase 1 ("Complete
// Internal Implementations") calls for before AvaHost's real request
// path (`avahost run`/`watch`) can move off RuntimeHost+HtmlRenderer
// onto avalang.ui.
//
// avalang.ui's state::IState (runtime/avaui/src/state/IState.h) is, by
// design (Fase 4), a C++ value cell with zero knowledge of AvaLang or
// any VM -- and it stays that way here. Nothing in runtime/avaui/ is
// touched by this file: avaui/ must never depend on avahost (AvaHost
// renders AvaUI, not the other way around -- see
// AVAUI_ARCHITECTURE_FREEZE_PLAN.md "Do NOT couple AvaUI with
// AvaHost"). Instead this file lives entirely on the AvaHost side of
// that boundary and provides a NEW *implementation* of the existing,
// frozen IState interface (Fase 13 froze the interface's signature,
// not the set of classes allowed to implement it) that happens to read
// and write a RuntimeHost's VM globals instead of an isolated
// PropertyValue. Same boundary rule runtime_host.h's own header
// comment states for RuntimeHost itself: this file reaches the VM
// exclusively through RuntimeHost's public API, never through
// avalang.h directly.
#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "state/IState.h"

namespace avahost {

class RuntimeHost;

// A state::IState backed by one VM global on a RuntimeHost, in place
// of StateImpl's (runtime/avaui/src/state/StateImpl.cpp) isolated
// PropertyValue + subscriber list.
//
// Value() reads a locally cached copy, refreshed by RefreshFromVm() --
// not a live VM round-trip on every call. That mirrors the same
// "per-request snapshot" model RuntimeHost::BindState/ExportStateJson
// already use rather than introducing a new one. Set() writes straight
// through to the VM global (via RuntimeHost::SetStateGlobal, added
// alongside this file) AND updates the local cache, so code that reads
// Value() again immediately -- or a handler InvokeHandler runs right
// after -- sees the same value this call just wrote.
//
// Subscribe/Unsubscribe behave exactly like StateImpl's: a plain
// in-process list of ChangeHandlers, fired whenever the cached value
// actually changes (PropertyValueEquals-style comparison, same rule
// IState::Set's doc comment specifies). This is what lets
// avalang::ui::state::StateBinding (Fase 4) and
// avalang::ui::animation::WireAnimations (Fase 19) keep working
// unmodified against a VmBackedState exactly as they would against a
// StateImpl -- they only ever see the IState interface.
class VmBackedState final : public avalang::ui::IState {
public:
    VmBackedState(RuntimeHost& host, std::string key, avalang::ui::PropertyValue initial);

    const avalang::ui::PropertyValue& Value() const override;
    void Set(avalang::ui::PropertyValue value) override;

    std::size_t Subscribe(ChangeHandler handler) override;
    void Unsubscribe(std::size_t subscriptionId) override;

    // Not part of IState (frozen, Fase 13) -- bridge-only, called by
    // VmStateBridge::RefreshAll. Re-reads this key's *current* VM
    // global into the local cache -- needed because a handler mutates
    // globals directly (`counter = counter + 1`), never through this
    // class's Set() -- firing subscribers exactly like Set() would if
    // the value actually changed.
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

// Owns one VmBackedState per key in a .avaui `state` block, all backed
// by the same RuntimeHost/VM. Mirrors, on the avaui/-facing side, what
// RuntimeHost::BindState/ExportStateJson already do on the VM-facing
// side: Bind() pushes the parsed `state` block onto the VM once and
// creates the IState cells; RefreshAll() re-syncs every cell from the
// VM's current globals and must be called after anything that mutates
// VM globals outside this bridge (InvokeHandler, InvokeHandlerIfDefined).
class VmStateBridge {
public:
    explicit VmStateBridge(RuntimeHost& host);

    // `stateSpec` is a parsed .avaui `state` block
    // (parser::ParsedAvaui::state -- key -> raw source text, e.g.
    // {"counter": "0"}, same shape avalang::ui::parser::AvauiParser
    // (Fase 14) already produces). Binds it onto the VM
    // (RuntimeHost::BindState) and creates one VmBackedState per key,
    // initialized from that same raw text -- not read back from the
    // VM, since BindState just wrote exactly this, so re-parsing here
    // (same true/false/numeric/string inference RuntimeHost::BindState
    // uses) avoids a redundant per-key VM round-trip.
    void Bind(const std::unordered_map<std::string, std::string>& stateSpec);

    // Fase 20.2.4: like Bind, but overlays `cachedStateJson` (the same
    // JSON shape RuntimeHost::ExportStateJson produces, i.e.
    // {"key":"raw text value", ...}) on top of `stateSpec`. Cached
    // values win for keys present in both, so a handler-mutated
    // counter accumulates across requests instead of every render
    // restarting at the page's `state` block default. Keys only in
    // stateSpec keep the file default; keys only in cachedStateJson
    // are still bound (the cached entry may reference a key the page
    // has since removed -- a future cleanup task can prune those).
    void BindWithOverlay(const std::unordered_map<std::string, std::string>& stateSpec,
                          const std::string& cachedStateJson);

    // Re-syncs every bound key from the VM's current globals, firing
    // each VmBackedState's subscribers for keys whose value actually
    // changed. Call after any RuntimeHost::InvokeHandler/
    // InvokeHandlerIfDefined that ran against this same VM.
    void RefreshAll();

    // nullptr if `key` wasn't part of the last Bind() call.
    avalang::ui::IState* Find(const std::string& key) const;

    // Current values of every bound key, in RuntimeHost::BindState's
    // JSON shape ({"key":"raw text value", ...}) -- for persisting
    // state across requests the same way AvaHostApp's stateCache_
    // already does on the old pipeline.
    std::string ExportJson() const;

    // Fase C: property-evaluator. Delegates to
    // RuntimeHost::EvalPropertyExpr, which compiles and runs `raw` as a
    // full expression against the VM's current globals (not just an
    // exact bound-state-key lookup), so expressions like
    // `"Count: " + counter` evaluate correctly instead of falling back
    // to the raw source text. Falls back to `raw` unchanged when it
    // doesn't compile/run or evaluates to Nil (e.g. a bare string
    // literal). Used by RenderTree to resolve `value = counter` style
    // properties without re-opening the frozen IComponent interface to
    // add PropertyNames().
    std::string EvalIdentifier(const std::string& raw) const;

private:
    RuntimeHost& host_;
    std::string templateStateJson_ = "{}";
    std::vector<std::pair<std::string, std::unique_ptr<VmBackedState>>> states_;
};

}  // namespace avahost
