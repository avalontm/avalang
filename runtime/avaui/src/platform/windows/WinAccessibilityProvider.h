#ifndef AVA_UI_PLATFORM_WINDOWS_WINACCESSIBILITYPROVIDER_H
#define AVA_UI_PLATFORM_WINDOWS_WINACCESSIBILITYPROVIDER_H

#include <windows.h>
#include <uiautomation.h>

#include "accessibility/AccessibilityNode.h"

namespace avalang {
namespace ui {
namespace platform {
namespace windows {

class WinUiaProvider final : public IRawElementProviderSimple,
                              public IRawElementProviderFragment,
                              public IRawElementProviderFragmentRoot,
                              public IInvokeProvider,
                              public IToggleProvider,
                              public ISelectionItemProvider,
                              public IScrollProvider {
public:
    WinUiaProvider(accessibility::AccessibilityNode* node, HWND hwnd);

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppInterface) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    HRESULT STDMETHODCALLTYPE get_ProviderOptions(ProviderOptions* pRetVal) override;
    HRESULT STDMETHODCALLTYPE GetPatternProvider(PATTERNID patternId, IUnknown** pRetVal) override;
    HRESULT STDMETHODCALLTYPE GetPropertyValue(PROPERTYID propertyId, VARIANT* pRetVal) override;
    HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(IRawElementProviderSimple** pRetVal) override;

    HRESULT STDMETHODCALLTYPE Navigate(NavigateDirection direction, IRawElementProviderFragment** pRetVal) override;
    HRESULT STDMETHODCALLTYPE GetRuntimeId(SAFEARRAY** pRetVal) override;
    HRESULT STDMETHODCALLTYPE get_BoundingRectangle(UiaRect* pRetVal) override;
    HRESULT STDMETHODCALLTYPE GetEmbeddedFragmentRoots(SAFEARRAY** pRetVal) override;
    HRESULT STDMETHODCALLTYPE SetFocus() override;
    HRESULT STDMETHODCALLTYPE get_FragmentRoot(IRawElementProviderFragmentRoot** pRetVal) override;

    HRESULT STDMETHODCALLTYPE ElementProviderFromPoint(double x, double y, IRawElementProviderFragment** pRetVal) override;
    HRESULT STDMETHODCALLTYPE GetFocus(IRawElementProviderFragment** pRetVal) override;

    HRESULT STDMETHODCALLTYPE Invoke() override;

    HRESULT STDMETHODCALLTYPE Toggle() override;
    HRESULT STDMETHODCALLTYPE get_ToggleState(ToggleState* pRetVal) override;

    HRESULT STDMETHODCALLTYPE Select() override;
    HRESULT STDMETHODCALLTYPE AddToSelection() override;
    HRESULT STDMETHODCALLTYPE RemoveFromSelection() override;
    HRESULT STDMETHODCALLTYPE get_IsSelected(BOOL* pRetVal) override;
    HRESULT STDMETHODCALLTYPE get_SelectionContainer(IRawElementProviderSimple** pRetVal) override;

    HRESULT STDMETHODCALLTYPE Scroll(ScrollAmount horizontalAmount, ScrollAmount verticalAmount) override;
    HRESULT STDMETHODCALLTYPE SetScrollPercent(double horizontalPercent, double verticalPercent) override;
    HRESULT STDMETHODCALLTYPE get_HorizontalScrollPercent(double* pRetVal) override;
    HRESULT STDMETHODCALLTYPE get_VerticalScrollPercent(double* pRetVal) override;
    HRESULT STDMETHODCALLTYPE get_HorizontalViewSize(double* pRetVal) override;
    HRESULT STDMETHODCALLTYPE get_VerticalViewSize(double* pRetVal) override;
    HRESULT STDMETHODCALLTYPE get_HorizontallyScrollable(BOOL* pRetVal) override;
    HRESULT STDMETHODCALLTYPE get_VerticallyScrollable(BOOL* pRetVal) override;

private:
    bool IsRoot() const;

    accessibility::AccessibilityNode* node_;
    HWND hwnd_;
    LONG refCount_ = 1;
};

}
}
}
}

#endif // AVA_UI_PLATFORM_WINDOWS_WINACCESSIBILITYPROVIDER_H
