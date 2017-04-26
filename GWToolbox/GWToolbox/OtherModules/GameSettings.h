#pragma once

#include <vector>

#include <GWCA\Utilities\MemoryPatcher.h>
#include <GWCA\Managers\ChatMgr.h>

#include "ToolboxModule.h"

class GameSettings : public ToolboxModule {
	GameSettings() {};
	~GameSettings() {};
public:
	static GameSettings& Instance() {
		static GameSettings instance;
		return instance;
	}

	const char* Name() const override { return "Game Settings"; }

	void Initialize() override;
	void Terminate() override;
	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;
	void DrawSettingInternal() override;

	void DrawBorderlessSetting();

	// some settings that are either referenced from multiple places
	// or have nowhere else to be
	bool borderless_window = false;
	bool open_template_links = false;
	bool auto_transform_url;
	bool tick_is_toggle = false;
	bool select_with_chat_doubleclick = false;
	bool flash_window_on_pm = false;
	bool flash_window_on_party_invite = false;
	bool flash_window_on_zoning = false;

	void ApplyBorderless(bool value);

private:
	std::vector<GW::MemoryPatcher*> patches;
	
};
