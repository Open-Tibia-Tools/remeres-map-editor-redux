//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include "ui/properties/srv_attribute_fields.h"

#include "app/managers/version_manager.h"
#include "game/item.h"

#include <limits>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>

namespace rme::srv {

	namespace {
		bool takesAttribute(const Item& item, ItemAttribute attribute) {
			switch (attribute) {
				case ItemAttribute::KeyNumber:
					return item.getDefinition().isKey();
				case ItemAttribute::KeyholeNumber:
				case ItemAttribute::DoorLevel:
				case ItemAttribute::DoorQuestNumber:
				case ItemAttribute::DoorQuestValue:
					return item.isDoor();
				case ItemAttribute::ChestQuestNumber:
					return item.getDefinition().isContainer();
				case ItemAttribute::RemainingExpireTime:
				case ItemAttribute::SavedExpireTime:
				case ItemAttribute::RemainingUses:
				case ItemAttribute::Responsible:
					return false; // Server state; shown only when the map has it
			}
			return false;
		}
	}

	bool isSrvClient() {
		return g_version.IsVersionLoaded() && g_version.GetCurrentVersion().getItemDefinitionMode() == ItemDefinitionMode::DatSrv;
	}

	bool showsAttribute(const Item& item, ItemAttribute attribute) {
		return item.getIntegerAttribute(key(attribute)) != nullptr || (isSrvClient() && takesAttribute(item, attribute));
	}

	int attributeValue(const Item& item, ItemAttribute attribute) {
		const int32_t* value = item.getIntegerAttribute(key(attribute));
		return value ? *value : 0;
	}

	void setAttributeValue(Item& item, ItemAttribute attribute, int value) {
		if (value == 0) {
			item.eraseAttribute(key(attribute));
		} else {
			item.setAttribute(key(attribute), static_cast<int32_t>(value));
		}
	}

} // namespace rme::srv

void SrvAttributeFields::create(wxFlexGridSizer* grid, wxWindow* parent, const Item& item) {
	for (size_t i = 0; i < spins_.size(); ++i) {
		const auto attribute = static_cast<rme::srv::ItemAttribute>(i);
		if (!rme::srv::showsAttribute(item, attribute)) {
			continue;
		}
		const int value = rme::srv::attributeValue(item, attribute);
		grid->Add(new wxStaticText(parent, wxID_ANY, wxstr(rme::srv::info(attribute).label)));
		spins_[i] = new wxSpinCtrl(parent, wxID_ANY, std::to_string(value), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, std::numeric_limits<int>::max(), value);
		grid->Add(spins_[i], wxSizerFlags(1).Expand());
	}
}

void SrvAttributeFields::apply(Item& item) const {
	for (size_t i = 0; i < spins_.size(); ++i) {
		if (spins_[i]) {
			rme::srv::setAttributeValue(item, static_cast<rme::srv::ItemAttribute>(i), spins_[i]->GetValue());
		}
	}
}
