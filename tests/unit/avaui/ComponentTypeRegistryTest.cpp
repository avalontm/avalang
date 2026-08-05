// Phase 6 (avastudio/avaui unification plan, Part B) -- minimal unit
// test for ComponentTypeRegistry: registers fake types and checks
// lookup/listing/idempotency. Does not touch any real control.

#include "registry/ComponentTypeRegistry.h"

#include <cassert>
#include <cstdio>

using avalang::ui::PropertyValue;
using avalang::ui::registry::ComponentTypeDescriptor;
using avalang::ui::registry::FindComponentType;
using avalang::ui::registry::GetComponentTypeRegistry;
using avalang::ui::registry::PropertyDefault;
using avalang::ui::registry::RegisterComponentType;

int main() {
    // Register a fake type and check it shows up both in the listing
    // and in lookup by name.
    RegisterComponentType(ComponentTypeDescriptor{
        "FakeWidget", "Fake Widget", /*is_container=*/false,
        {PropertyDefault{"text", PropertyValue(std::string("hello"))}},
    });

    const ComponentTypeDescriptor* found = FindComponentType("FakeWidget");
    assert(found != nullptr);
    assert(found->display_name == "Fake Widget");
    assert(found->is_container == false);
    assert(found->default_properties.size() == 1);
    assert(found->default_properties[0].name == "text");
    assert(found->default_properties[0].value.AsString() == "hello");

    bool listed = false;
    for (const ComponentTypeDescriptor& desc : GetComponentTypeRegistry()) {
        if (desc.type == "FakeWidget") {
            listed = true;
            break;
        }
    }
    assert(listed);

    // Lookup of a type that doesn't exist -> nullptr.
    assert(FindComponentType("DoesNotExist") == nullptr);

    // Registering again with the same `type` replaces instead of
    // duplicating (idempotent by type).
    RegisterComponentType(ComponentTypeDescriptor{
        "FakeWidget", "Fake Widget v2", /*is_container=*/true, {},
    });
    const ComponentTypeDescriptor* replaced = FindComponentType("FakeWidget");
    assert(replaced != nullptr);
    assert(replaced->display_name == "Fake Widget v2");
    assert(replaced->is_container == true);

    int count = 0;
    for (const ComponentTypeDescriptor& desc : GetComponentTypeRegistry()) {
        if (desc.type == "FakeWidget") count++;
    }
    assert(count == 1);

    std::printf("ComponentTypeRegistryTest: OK\n");
    return 0;
}
