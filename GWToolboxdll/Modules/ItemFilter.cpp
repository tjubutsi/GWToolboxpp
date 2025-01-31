#include "stdafx.h"

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/ItemContext.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Packets/StoC.h>

#include <Utils/GuiUtils.h>

#include <Modules/ItemFilter.h>

#define LOAD_BOOL(var) var = ini->GetBoolValue(Name(), #var, var);
#define SAVE_BOOL(var) ini->SetBoolValue(Name(), #var, var);
#define MAP_ENTRY(var) \
    {                  \
        var, #var      \
    }

namespace {
    using namespace GW::Constants::ItemID;

    Rarity GetRarity(GW::Item const& item)
    {
        if (item.complete_name_enc == nullptr) return Rarity::Unknown;

        switch (item.complete_name_enc[0]) {
            case 2621: return Rarity::White;
            case 2623: return Rarity::Blue;
            case 2626: return Rarity::Purple;
            case 2624: return Rarity::Gold;
            case 2627: return Rarity::Green;
            default: return Rarity::Unknown;
        }
    }

    const GW::Item* GetItemFromPacket(const GW::Packet::StoC::AgentAdd& packet)
    {
        // filter non-item-agents
        if (packet.type != 4 || packet.unk3 != 0) return nullptr;

        const auto& items = GW::GameContext::instance()->items->item_array;
        const auto item_id = packet.agent_type;
        if (item_id >= items.size()) return nullptr;

        return items[item_id];
    }

    const std::map<ItemModelID, std::string> default_dont_hide_for_player = {
        // Weapons
        MAP_ENTRY(DSR), MAP_ENTRY(EternalBlade), MAP_ENTRY(VoltaicSpear), MAP_ENTRY(CrystallineSword), MAP_ENTRY(ObsidianEdge),

        // Rare and Valuable Items
        MAP_ENTRY(MiniDhuum), MAP_ENTRY(ArmbraceOfTruth), MAP_ENTRY(MargoniteGem), MAP_ENTRY(StygianGem), MAP_ENTRY(TitanGem), MAP_ENTRY(TormentGem),

        // Crafting Items
        MAP_ENTRY(Diamond), MAP_ENTRY(Ruby), MAP_ENTRY(Sapphire), MAP_ENTRY(GlobofEctoplasm), MAP_ENTRY(ObsidianShard),

        // Consumables
        MAP_ENTRY(Cupcakes), MAP_ENTRY(Apples), MAP_ENTRY(Corns), MAP_ENTRY(Pies), MAP_ENTRY(Eggs), MAP_ENTRY(Warsupplies), MAP_ENTRY(SkalefinSoup), MAP_ENTRY(PahnaiSalad), MAP_ENTRY(Kabobs), MAP_ENTRY(PumpkinCookie),

        // Rock Candy
        MAP_ENTRY(GRC), MAP_ENTRY(BRC), MAP_ENTRY(RRC),

        // Conset
        MAP_ENTRY(ConsEssence), MAP_ENTRY(ConsArmor), MAP_ENTRY(ConsGrail),

        // Lunars
        MAP_ENTRY(LunarDragon), MAP_ENTRY(LunarHorse), MAP_ENTRY(LunarMonkey), MAP_ENTRY(LunarOx), MAP_ENTRY(LunarRabbit), MAP_ENTRY(LunarRat), MAP_ENTRY(LunarRooster), MAP_ENTRY(LunarSheep), MAP_ENTRY(LunarSnake), MAP_ENTRY(LunarTiger),
        MAP_ENTRY(LunarDog), MAP_ENTRY(LunarPig), MAP_ENTRY(LunarMonkey),

        // Alcohol
        MAP_ENTRY(Absinthe), MAP_ENTRY(AgedDwarvenAle), MAP_ENTRY(AgedHuntersAle), MAP_ENTRY(BottleOfJuniberryGin), MAP_ENTRY(BottleOfVabbianWine), MAP_ENTRY(Cider), MAP_ENTRY(DwarvenAle), MAP_ENTRY(Eggnog), MAP_ENTRY(FlaskOfFirewater), MAP_ENTRY(Grog),
        MAP_ENTRY(HuntersAle), MAP_ENTRY(Keg), MAP_ENTRY(KrytanBrandy), MAP_ENTRY(Ricewine), MAP_ENTRY(ShamrockAle), MAP_ENTRY(SpikedEggnog), MAP_ENTRY(WitchsBrew),

        // Summons
        MAP_ENTRY(GhastlyStone), MAP_ENTRY(GakiSummon), MAP_ENTRY(TurtleSummon),

        // Summons x3
        MAP_ENTRY(TenguSummon), MAP_ENTRY(ImperialGuardSummon), MAP_ENTRY(WarhornSummon),

        // Other
        MAP_ENTRY(IdentificationKit), MAP_ENTRY(IdentificationKit_Superior), MAP_ENTRY(SalvageKit), MAP_ENTRY(SalvageKit_Expert), MAP_ENTRY(SalvageKit_Superior), MAP_ENTRY(Lockpick), MAP_ENTRY(ResScrolls), MAP_ENTRY(GoldCoin),

        // Quest Items
        MAP_ENTRY(UnholyText)};

    const std::map<ItemModelID, std::string> default_dont_hide_for_party = {
        MAP_ENTRY(EternalBlade),
        MAP_ENTRY(VoltaicSpear),
        MAP_ENTRY(MiniDhuum),
        MAP_ENTRY(CrystallineSword),
        MAP_ENTRY(DSR),
        MAP_ENTRY(ObsidianEdge),
    };

} // namespace

void ItemFilter::Initialize()
{
    ToolboxModule::Initialize();

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentAdd>(&OnAgentAdd_Entry, OnAgentAdd);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentRemove>(&OnAgentRemove_Entry, OnAgentRemove);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::MapLoaded>(&OnMapLoad_Entry, OnMapLoad);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ItemGeneral_ReuseID>(&OnItemReuseId_Entry, OnItemReuseId);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ItemUpdateOwner>(&OnItemUpdateOwner_Entry, OnItemUpdateOwner);
}

void ItemFilter::SignalTerminate()
{
    ToolboxModule::SignalTerminate();

    SpawnSuppressedItems();
    GW::StoC::RemoveCallback<GW::Packet::StoC::AgentAdd>(&OnAgentAdd_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::AgentRemove>(&OnAgentRemove_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::MapLoaded>(&OnMapLoad_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::ItemGeneral_ReuseID>(&OnItemReuseId_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::ItemUpdateOwner>(&OnItemUpdateOwner_Entry);
}

void ItemFilter::LoadSettings(CSimpleIniA* ini)
{
    ToolboxModule::LoadSettings(ini);
    LOAD_BOOL(hide_player_white);
    LOAD_BOOL(hide_player_blue);
    LOAD_BOOL(hide_player_purple);
    LOAD_BOOL(hide_player_gold);
    LOAD_BOOL(hide_player_green);
    LOAD_BOOL(hide_party_white);
    LOAD_BOOL(hide_party_blue);
    LOAD_BOOL(hide_party_purple);
    LOAD_BOOL(hide_party_gold);
    LOAD_BOOL(hide_party_green);

    dont_hide_for_player = GuiUtils::IniToMap<decltype(dont_hide_for_player)>(ini, Name(), "dont_hide_for_player", default_dont_hide_for_player);
    dont_hide_for_party = GuiUtils::IniToMap<decltype(dont_hide_for_party)>(ini, Name(), "dont_hide_for_party", default_dont_hide_for_party);
}

void ItemFilter::SaveSettings(CSimpleIniA* ini)
{
    ToolboxModule::SaveSettings(ini);
    SAVE_BOOL(hide_player_white);
    SAVE_BOOL(hide_player_blue);
    SAVE_BOOL(hide_player_purple);
    SAVE_BOOL(hide_player_gold);
    SAVE_BOOL(hide_player_green);
    SAVE_BOOL(hide_party_white);
    SAVE_BOOL(hide_party_blue);
    SAVE_BOOL(hide_party_purple);
    SAVE_BOOL(hide_party_gold);
    SAVE_BOOL(hide_party_green);

    GuiUtils::MapToIni(ini, Name(), "dont_hide_for_player", dont_hide_for_player);
    GuiUtils::MapToIni(ini, Name(), "dont_hide_for_party", dont_hide_for_party);
}

void ItemFilter::SpawnSuppressedItems()
{
    for (const auto& packet : suppressed_packets) {
        GW::GameThread::Enqueue([cpy = packet]() mutable { GW::StoC::EmulatePacket(reinterpret_cast<GW::Packet::StoC::PacketBase*>(&cpy)); });
    }

    suppressed_packets.clear();
}
void ItemFilter::OnAgentAdd(GW::HookStatus* status, GW::Packet::StoC::AgentAdd* packet)
{
    const auto* item = GetItemFromPacket(*packet);
    if (!item) return;

    const auto* player = GW::Agents::GetCharacter();
    if (player == nullptr) return;

    if (player->max_energy == 0 || player->login_number == 0) {
        // we're spectating, not sure what our own player is
        if (Instance().WantToHide(*item, false) && Instance().WantToHide(*item, true)) {
            // only block items that we want to block for player and party
            status->blocked = true;
            Instance().suppressed_packets.push_back(*packet);
        }
        return;
    }

    const auto owner_id = Instance().GetItemOwner(item->item_id);
    const auto can_pick_up = owner_id == 0                    // not reserved
                             || owner_id == player->agent_id; // reserved for user

    if (Instance().WantToHide(*item, can_pick_up)) {
        status->blocked = true;
        Instance().suppressed_packets.push_back(*packet);
    }
}

void ItemFilter::OnAgentRemove(GW::HookStatus* status, GW::Packet::StoC::AgentRemove* packet)
{
    // Block despawning the agent if the client never spawned it.
    const auto it = std::ranges::find_if(Instance().suppressed_packets, [&packet](const auto& suppressed_packet) { return suppressed_packet.agent_id == packet->agent_id; });

    if (it == Instance().suppressed_packets.end()) return;

    Instance().suppressed_packets.erase(it);
    status->blocked = true;
}

void ItemFilter::OnMapLoad(GW::HookStatus*, GW::Packet::StoC::MapLoaded*)
{
    Instance().suppressed_packets.clear();
    Instance().item_owners.clear();
}

void ItemFilter::OnItemReuseId(GW::HookStatus*, GW::Packet::StoC::ItemGeneral_ReuseID* packet)
{
    const auto it = std::ranges::find_if(Instance().item_owners, [packet](auto owner) { return owner.item == packet->item_id; });

    if (it != Instance().item_owners.end()) Instance().item_owners.erase(it);
}

void ItemFilter::OnItemUpdateOwner(GW::HookStatus*, GW::Packet::StoC::ItemUpdateOwner* packet)
{
    const auto it = std::ranges::find_if(Instance().item_owners, [packet](auto owner) { return owner.item == packet->item_id; });

    if (it == Instance().item_owners.end()) {
        Instance().item_owners.push_back({packet->item_id, packet->owner_agent_id});
    }
    else {
        it->owner = packet->owner_agent_id;
    }
}

GW::AgentID ItemFilter::GetItemOwner(const GW::ItemID item_id) const
{
    const auto it = std::ranges::find_if(item_owners, [item_id](auto owner) { return owner.item == item_id; });
    if (it == item_owners.end()) return 0;

    return it->owner;
}

bool ItemFilter::WantToHide(const GW::Item& item, const bool can_pick_up) const
{
    const auto rarity = GetRarity(item);
    if (can_pick_up) {
        if (dont_hide_for_player.contains(item.model_id)) return false;

        switch (rarity) {
            case Rarity::White: return hide_player_white;
            case Rarity::Blue: return hide_player_blue;
            case Rarity::Purple: return hide_player_purple;
            case Rarity::Gold: return hide_player_gold;
            case Rarity::Green: return hide_player_green;
            case Rarity::Unknown: return false;
        }
    }

    if (dont_hide_for_party.contains(item.model_id)) return false;

    switch (rarity) {
        case Rarity::White: return hide_party_white;
        case Rarity::Blue: return hide_party_blue;
        case Rarity::Purple: return hide_party_purple;
        case Rarity::Gold: return hide_party_gold;
        case Rarity::Green: return hide_party_green;
        case Rarity::Unknown: return false;
    }

    return false;
}

void ItemFilter::DrawSettingInternal()
{
    ImGui::Text("Block the following item drops:");
    ImGui::Separator();
    ImGui::TextDisabled("First column is for items you can pick up, second for items reserved for a party member");
    ImGui::TextDisabled("Below, you can define items that should never be blocked for you or party members.");
    ImGui::Columns(2, "player_or_ally");

    ImGui::Checkbox("White##player", &hide_player_white);
    ImGui::Checkbox("Blue##player", &hide_player_blue);
    ImGui::Checkbox("Purple##player", &hide_player_purple);
    ImGui::Checkbox("Gold##player", &hide_player_gold);
    ImGui::Checkbox("Green##player", &hide_player_green);

    ImGui::NextColumn();

    ImGui::Checkbox("White##party", &hide_party_white);
    ImGui::Checkbox("Blue##party", &hide_party_blue);
    ImGui::Checkbox("Purple##party", &hide_party_purple);
    ImGui::Checkbox("Gold##party", &hide_party_gold);
    ImGui::Checkbox("Green##party", &hide_party_green);

    ImGui::EndColumns();

    const auto itembtn = std::format("Spawn all blocked items ({})", suppressed_packets.size());

    if (ImGui::Button(itembtn.c_str())) {
        SpawnSuppressedItems();
    }

    ImGui::Separator();
    auto& style = ImGui::GetStyle();
    const auto old_color = style.Colors[ImGuiCol_Header];
    style.Colors[ImGuiCol_Header] = ImColor{};
    if (ImGui::CollapsingHeader("Don't hide items for you with model ids")) {
        ImGui::PushID("BlockPlayerItems");

        if (ImGui::Button("Restore defaults##player")) {
            dont_hide_for_player = default_dont_hide_for_player;
        }
        ImGui::BeginChild("dont_block_for_player", ImVec2(0.0f, dont_hide_for_player.size() * 26.f));
        for (const auto& [item_id, item_name] : dont_hide_for_player) {
            ImGui::PushID(static_cast<int>(item_id));
            ImGui::Text("%s (%d)", item_name.c_str(), item_id);
            ImGui::SameLine();
            const bool clicked = ImGui::Button(" X ");
            ImGui::PopID();
            if (clicked) {
                dont_hide_for_player.erase(item_id);
                break;
            }
        }
        ImGui::EndChild();
        ImGui::Separator();
        bool submitted = false;
        ImGui::Text("Add new item:");
        static int new_item_id;
        static char buf[50];
        ImGui::InputText("Item Name##player", buf, 50);
        ImGui::InputInt("Item Model ID##player", &new_item_id);
        submitted |= ImGui::Button("Add");
        if (submitted && new_item_id > 0) {
            const auto new_id = static_cast<uint32_t>(new_item_id);
            if (!dont_hide_for_player.contains(new_id)) {
                dont_hide_for_player[new_id] = std::string(buf);
                Log::Info("Added Item %s with ID (%d)", buf, new_id);
                std::ranges::fill(buf, '\0');
                new_item_id = 0;
            }
        }

        ImGui::PopID();
    }

    ImGui::Separator();
    if (ImGui::CollapsingHeader("Don't hide items for party members with model ids")) {
        ImGui::PushID("BlockPartyItems");
        if (ImGui::Button("Restore defaults##party")) {
            dont_hide_for_party = default_dont_hide_for_party;
        }
        ImGui::BeginChild("dont_block_for_party", ImVec2(0.0f, dont_hide_for_party.size() * 26.f));
        for (const auto& [item_id, item_name] : dont_hide_for_party) {
            ImGui::PushID(static_cast<int>(item_id));
            ImGui::Text("%s (%d)", item_name.c_str(), item_id);
            ImGui::SameLine();
            const bool clicked = ImGui::Button(" X ");
            ImGui::PopID();
            if (clicked) {
                dont_hide_for_party.erase(item_id);
                break;
            }
        }
        ImGui::EndChild();
        ImGui::Separator();
        bool submitted = false;
        ImGui::Text("Add new item:");
        static int new_item_id_party;
        static char buf[50];
        ImGui::InputText("Item Name##party", buf, 50);
        ImGui::InputInt("Item Model ID##party", &new_item_id_party);
        submitted |= ImGui::Button("Add");
        if (submitted && new_item_id_party > 0) {
            const auto new_id = static_cast<uint32_t>(new_item_id_party);
            if (!dont_hide_for_party.contains(new_id)) {
                dont_hide_for_party[new_id] = std::string(buf);
                Log::Info("Added Item %s with ID (%d)", buf, new_id);
                std::ranges::fill(buf, '\0');
                new_item_id_party = 0;
            }
        }

        ImGui::PopID();
    }
    style.Colors[ImGuiCol_Header] = old_color;
}
