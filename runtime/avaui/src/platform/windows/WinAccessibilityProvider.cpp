#include "WinAccessibilityProvider.h"
#include "WinAccessibilityBridge.h"

#include <vector>

namespace avalang {
namespace ui {
namespace platform {
namespace windows {

namespace {

BSTR Utf8ToBstr(const std::string& text) {
    int wlen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (wlen <= 0) return SysAllocString(L"");

    std::vector<wchar_t> buffer(static_cast<size_t>(wlen));
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, buffer.data(), wlen);
    return SysAllocString(buffer.data());
}

long ControlTypeIdFromName(const std::string& name) {
    if (name == "UIA_ButtonControlTypeId") return UIA_ButtonControlTypeId;
    if (name == "UIA_TextControlTypeId") return UIA_TextControlTypeId;
    if (name == "UIA_ImageControlTypeId") return UIA_ImageControlTypeId;
    if (name == "UIA_GroupControlTypeId") return UIA_GroupControlTypeId;
    if (name == "UIA_EditControlTypeId") return UIA_EditControlTypeId;
    if (name == "UIA_CheckBoxControlTypeId") return UIA_CheckBoxControlTypeId;
    if (name == "UIA_RadioButtonControlTypeId") return UIA_RadioButtonControlTypeId;
    if (name == "UIA_ComboBoxControlTypeId") return UIA_ComboBoxControlTypeId;
    if (name == "UIA_WindowControlTypeId") return UIA_WindowControlTypeId;
    if (name == "UIA_HyperlinkControlTypeId") return UIA_HyperlinkControlTypeId;
    if (name == "UIA_ScrollBarControlTypeId") return UIA_ScrollBarControlTypeId;
    if (name == "UIA_DocumentControlTypeId") return UIA_DocumentControlTypeId;
    return UIA_CustomControlTypeId;
}

accessibility::AccessibilityNode* NthChild(accessibility::AccessibilityNode* node, size_t index) {
    if (!node) return nullptr;
    const auto& children = node->Children();
    if (index >= children.size()) return nullptr;
    return children[index];
}

bool ContainsPoint(const accessibility::AccessibilityRect& bounds, double x, double y) {
    if (bounds.width <= 0.0 && bounds.height <= 0.0) return false;
    return x >= bounds.x && x < bounds.x + bounds.width &&
           y >= bounds.y && y < bounds.y + bounds.height;
}

accessibility::AccessibilityNode* HitTest(accessibility::AccessibilityNode* node, double x, double y) {
    if (!node) return nullptr;
    for (accessibility::AccessibilityNode* child : node->Children()) {
        if (accessibility::AccessibilityNode* hit = HitTest(child, x, y)) return hit;
    }
    return ContainsPoint(node->Bounds(), x, y) ? node : nullptr;
}

int ScrollAmountToInt(ScrollAmount amount) {
    return static_cast<int>(amount) - static_cast<int>(ScrollAmount_NoAmount);
}

}

WinUiaProvider::WinUiaProvider(accessibility::AccessibilityNode* node, HWND hwnd)
    : node_(node), hwnd_(hwnd) {
}

bool WinUiaProvider::IsRoot() const {
    return node_ && node_->Parent() == nullptr;
}

HRESULT STDMETHODCALLTYPE WinUiaProvider::QueryInterface(REFIID riid, void** ppInterface) {
    if (!ppInterface) return E_INVALIDARG;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(IRawElementProviderSimple)) {
        *ppInterface = static_cast<IRawElementProviderSimple*>(this);
    } else if (riid == __uuidof(IRawElementProviderFragment)) {
        *ppInterface = static_cast<IRawElementProviderFragment*>(this);
    } else if (riid == __uuidof(IRawElementProviderFragmentRoot) && IsRoot()) {
        *ppInterface = static_cast<IRawElementProviderFragmentRoot*>(this);
    } else if (riid == __uuidof(IInvokeProvider)) {
        *ppInterface = static_cast<IInvokeProvider*>(this);
    } else if (riid == __uuidof(IToggleProvider)) {
        *ppInterface = static_cast<IToggleProvider*>(this);
    } else if (riid == __uuidof(ISelectionItemProvider)) {
        *ppInterface = static_cast<ISelectionItemProvider*>(this);
    } else if (riid == __uuidof(IScrollProvider)) {
        *ppInterface = static_cast<IScrollProvider*>(this);
    } else {
        *ppInterface = nullptr;
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE WinUiaProvider::AddRef() {
    return InterlockedIncrement(&refCount_);
}

ULONG STDMETHODCALLTYPE WinUiaProvider::Release() {
    ULONG result = InterlockedDecrement(&refCount_);
    if (result == 0) delete this;
    return result;
}

HRESULT STDMETHODCALLTYPE WinUiaProvider::get_ProviderOptions(ProviderOptions* pRetVal) {
    if (!pRetVal) return E_INVALIDARG;
    *pRetVal = ProviderOptions_ServerSideProvider;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WinUiaProvider::GetPatternProvider(PATTERNID patternId, IUnknown** pRetVal) {
    if (!pRetVal) return E_INVALIDARG;
    *pRetVal = nullptr;
    if (!node_) return S_OK;

    const accessibility::AccessibilityRole role = node_->Role();

    if (patternId == UIA_InvokePatternId &&
        (role == accessibility::AccessibilityRole::Button ||
         role == accessibility::AccessibilityRole::Link ||
         role == accessibility::AccessibilityRole::ComboBox)) {
        AddRef();
        *pRetVal = static_cast<IInvokeProvider*>(this);
    } else if (patternId == UIA_TogglePatternId && role == accessibility::AccessibilityRole::CheckBox) {
        AddRef();
        *pRetVal = static_cast<IToggleProvider*>(this);
    } else if (patternId == UIA_SelectionItemPatternId && role == accessibility::AccessibilityRole::RadioButton) {
        AddRef();
        *pRetVal = static_cast<ISelectionItemProvider*>(this);
    } else if (patternId == UIA_ScrollPatternId && role == accessibility::AccessibilityRole::ScrollView) {
        AddRef();
        *pRetVal = static_cast<IScrollProvider*>(this);
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WinUiaProvider::GetPropertyValue(PROPERTYID propertyId, VARIANT* pRetVal) {
    if (!pRetVal) return E_INVALIDARG;
    VariantInit(pRetVal);
    if (!node_) return S_OK;

    switch (propertyId) {
        case UIA_ControlTypePropertyId: {
            accessibility::WindowsMapping mapping = WinAccessibility_MappingFor(node_);
            pRetVal->vt = VT_I4;
            pRetVal->lVal = ControlTypeIdFromName(mapping.controlType);
            break;
        }
        case UIA_NamePropertyId:
            pRetVal->vt = VT_BSTR;
            pRetVal->bstrVal = Utf8ToBstr(node_->Label());
            break;
        case UIA_HelpTextPropertyId:
            pRetVal->vt = VT_BSTR;
            pRetVal->bstrVal = Utf8ToBstr(node_->Hint());
            break;
        case UIA_IsEnabledPropertyId:
            pRetVal->vt = VT_BOOL;
            pRetVal->boolVal = node_->HasState(accessibility::AccessibilityState::Disabled) ? VARIANT_FALSE : VARIANT_TRUE;
            break;
        case UIA_HasKeyboardFocusPropertyId:
            pRetVal->vt = VT_BOOL;
            pRetVal->boolVal = node_->HasState(accessibility::AccessibilityState::Focused) ? VARIANT_TRUE : VARIANT_FALSE;
            break;
        case UIA_IsKeyboardFocusablePropertyId:
            pRetVal->vt = VT_BOOL;
            pRetVal->boolVal = node_->HasState(accessibility::AccessibilityState::Focusable) ? VARIANT_TRUE : VARIANT_FALSE;
            break;
        case UIA_IsContentElementPropertyId:
        case UIA_IsControlElementPropertyId:
            pRetVal->vt = VT_BOOL;
            pRetVal->boolVal = VARIANT_TRUE;
            break;
        default:
            break;
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WinUiaProvider::get_HostRawElementProvider(IRawElementProviderSimple** pRetVal) {
    if (!pRetVal) return E_INVALIDARG;
    if (IsRoot()) return UiaHostProviderFromHwnd(hwnd_, pRetVal);
    *pRetVal = nullptr;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WinUiaProvider::Navigate(NavigateDirection direction, IRawElementProviderFragment** pRetVal) {
    if (!pRetVal) return E_INVALIDARG;
    *pRetVal = nullptr;
    if (!node_) return S_OK;

    accessibility::AccessibilityNode* target = nullptr;

    switch (direction) {
        case NavigateDirection_Parent:
            target = node_->Parent();
            break;
        case NavigateDirection_FirstChild:
            target = NthChild(node_, 0);
            break;
        case NavigateDirection_LastChild: {
            const auto& children = node_->Children();
            if (!children.empty()) target = children.back();
            break;
        }
        case NavigateDirection_NextSibling:
        case NavigateDirection_PreviousSibling: {
            accessibility::AccessibilityNode* parent = node_->Parent();
            if (parent) {
                const auto& siblings = parent->Children();
                for (size_t i = 0; i < siblings.size(); ++i) {
                    if (siblings[i] != node_) continue;
                    if (direction == NavigateDirection_NextSibling && i + 1 < siblings.size()) {
                        target = siblings[i + 1];
                    } else if (direction == NavigateDirection_PreviousSibling && i > 0) {
                        target = siblings[i - 1];
                    }
                    break;
                }
            }
            break;
        }
        default:
            break;
    }

    if (target) *pRetVal = new WinUiaProvider(target, hwnd_);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WinUiaProvider::GetRuntimeId(SAFEARRAY** pRetVal) {
    if (!pRetVal) return E_INVALIDARG;
    *pRetVal = nullptr;
    if (!node_ || IsRoot()) return S_OK;

    SAFEARRAY* runtimeId = SafeArrayCreateVector(VT_I4, 0, 2);
    if (!runtimeId) return E_OUTOFMEMORY;

    LONG index = 0;
    int part0 = UiaAppendRuntimeId;
    SafeArrayPutElement(runtimeId, &index, &part0);
    index = 1;
    int part1 = static_cast<int>(node_->Id());
    SafeArrayPutElement(runtimeId, &index, &part1);

    *pRetVal = runtimeId;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WinUiaProvider::get_BoundingRectangle(UiaRect* pRetVal) {
    if (!pRetVal) return E_INVALIDARG;
    *pRetVal = UiaRect{0.0, 0.0, 0.0, 0.0};
    if (!node_) return S_OK;

    const accessibility::AccessibilityRect& bounds = node_->Bounds();
    if (bounds.width <= 0.0 && bounds.height <= 0.0) return S_OK;

    POINT origin{static_cast<LONG>(bounds.x), static_cast<LONG>(bounds.y)};
    ClientToScreen(hwnd_, &origin);

    pRetVal->left = origin.x;
    pRetVal->top = origin.y;
    pRetVal->width = bounds.width;
    pRetVal->height = bounds.height;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WinUiaProvider::GetEmbeddedFragmentRoots(SAFEARRAY** pRetVal) {
    if (!pRetVal) return E_INVALIDARG;
    *pRetVal = nullptr;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WinUiaProvider::SetFocus() {
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WinUiaProvider::get_FragmentRoot(IRawElementProviderFragmentRoot** pRetVal) {
    if (!pRetVal) return E_INVALIDARG;

    accessibility::AccessibilityNode* root = node_;
    while (root && root->Parent()) root = root->Parent();

    if (!root) {
        *pRetVal = nullptr;
        return S_OK;
    }

    auto* provider = new WinUiaProvider(root, hwnd_);
    *pRetVal = provider;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WinUiaProvider::ElementProviderFromPoint(double x, double y, IRawElementProviderFragment** pRetVal) {
    if (!pRetVal) return E_INVALIDARG;
    *pRetVal = nullptr;
    if (!node_) return S_OK;

    POINT client{static_cast<LONG>(x), static_cast<LONG>(y)};
    ScreenToClient(hwnd_, &client);

    accessibility::AccessibilityNode* root = node_;
    while (root->Parent()) root = root->Parent();

    if (accessibility::AccessibilityNode* hit = HitTest(root, client.x, client.y)) {
        *pRetVal = new WinUiaProvider(hit, hwnd_);
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WinUiaProvider::GetFocus(IRawElementProviderFragment** pRetVal) {
    if (!pRetVal) return E_INVALIDARG;
    *pRetVal = nullptr;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WinUiaProvider::Invoke() {
    if (!node_) return UIA_E_ELEMENTNOTAVAILABLE;
    WinAccessibility_Invoke(node_->Id());
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WinUiaProvider::Toggle() {
    if (!node_) return UIA_E_ELEMENTNOTAVAILABLE;
    WinAccessibility_Invoke(node_->Id());
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WinUiaProvider::get_ToggleState(ToggleState* pRetVal) {
    if (!pRetVal) return E_INVALIDARG;
    *pRetVal = (node_ && node_->HasState(accessibility::AccessibilityState::Checked))
                   ? ToggleState_On
                   : ToggleState_Off;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WinUiaProvider::Select() {
    if (!node_) return UIA_E_ELEMENTNOTAVAILABLE;
    if (!node_->HasState(accessibility::AccessibilityState::Checked)) {
        WinAccessibility_Invoke(node_->Id());
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WinUiaProvider::AddToSelection() {
    return Select();
}

HRESULT STDMETHODCALLTYPE WinUiaProvider::RemoveFromSelection() {
    return UIA_E_INVALIDOPERATION;
}

HRESULT STDMETHODCALLTYPE WinUiaProvider::get_IsSelected(BOOL* pRetVal) {
    if (!pRetVal) return E_INVALIDARG;
    *pRetVal = (node_ && node_->HasState(accessibility::AccessibilityState::Checked))
                   ? VARIANT_TRUE
                   : VARIANT_FALSE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WinUiaProvider::get_SelectionContainer(IRawElementProviderSimple** pRetVal) {
    if (!pRetVal) return E_INVALIDARG;
    *pRetVal = nullptr;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WinUiaProvider::Scroll(ScrollAmount horizontalAmount, ScrollAmount verticalAmount) {
    if (!node_) return UIA_E_ELEMENTNOTAVAILABLE;
    WinAccessibility_ScrollBy(node_->Id(), ScrollAmountToInt(horizontalAmount), ScrollAmountToInt(verticalAmount));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WinUiaProvider::SetScrollPercent(double horizontalPercent, double verticalPercent) {
    if (!node_) return UIA_E_ELEMENTNOTAVAILABLE;
    WinAccessibility_SetScrollPercent(node_->Id(), horizontalPercent, verticalPercent);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WinUiaProvider::get_HorizontalScrollPercent(double* pRetVal) {
    if (!pRetVal) return E_INVALIDARG;
    *pRetVal = node_ ? WinAccessibility_GetScrollInfo(node_->Id()).horizontalScrollPercent : -1.0;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WinUiaProvider::get_VerticalScrollPercent(double* pRetVal) {
    if (!pRetVal) return E_INVALIDARG;
    *pRetVal = node_ ? WinAccessibility_GetScrollInfo(node_->Id()).verticalScrollPercent : -1.0;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WinUiaProvider::get_HorizontalViewSize(double* pRetVal) {
    if (!pRetVal) return E_INVALIDARG;
    *pRetVal = node_ ? WinAccessibility_GetScrollInfo(node_->Id()).horizontalViewSize : 100.0;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WinUiaProvider::get_VerticalViewSize(double* pRetVal) {
    if (!pRetVal) return E_INVALIDARG;
    *pRetVal = node_ ? WinAccessibility_GetScrollInfo(node_->Id()).verticalViewSize : 100.0;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WinUiaProvider::get_HorizontallyScrollable(BOOL* pRetVal) {
    if (!pRetVal) return E_INVALIDARG;
    *pRetVal = (node_ && WinAccessibility_GetScrollInfo(node_->Id()).horizontallyScrollable) ? VARIANT_TRUE : VARIANT_FALSE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WinUiaProvider::get_VerticallyScrollable(BOOL* pRetVal) {
    if (!pRetVal) return E_INVALIDARG;
    *pRetVal = (node_ && WinAccessibility_GetScrollInfo(node_->Id()).verticallyScrollable) ? VARIANT_TRUE : VARIANT_FALSE;
    return S_OK;
}

}
}
}
}
