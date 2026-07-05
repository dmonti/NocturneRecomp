// game_symbols mod - a "library mod": no UI, no code of its own to react to
// anything, just published guest addresses other mods can depend on instead
// of re-deriving (or copy-pasting) the same reverse-engineered constants.
//
// Registers every known address in OnCreateDialogs (dispatched before
// OnModuleLaunched, and before any consumer's own OnCreateDialogs runs a
// lazy lookup) via rex::system::ModRegistry, reached through
// ctx->runtime->mod_registry(). Consumers add `requires = "game_symbols"` to
// their own mod.toml (see mods_src/ui_color/mod.toml) so the SDK guarantees
// this mod is enabled and ordered first, instead of relying on convention.
//
// See docs/making-mods.md's "Library mods and the shared registry" section.

#include <rex/system/mod_plugin.h>

#include <rex/runtime.h>
#include <rex/system/mod_registry.h>

namespace {

// Guest addresses of the live Settings -> Accent Color struct (three
// consecutive big-endian uint32 fields, R/G/B, each 0-15)
constexpr uint32_t kAccentAddrVanilla = 0x83173CC8u;
constexpr uint32_t kAccentAddrTU = 0x83173A88u;

// Guest address of the live screen-stretch viewport rect: four consecutive
// big-endian uint32 fields (x offset, y offset, width, height). Found via
// the min/max boundary-matrix memory scan documented in the project's
// "live-memory-reverse-engineering-technique" note. Width ranges
// 1052-1280, height 667-720 (this game renders at 1280x720); x/y offset
// are derived by the game as (1280 - width) and (720 - height) -- the rect
// is anchored to the bottom-right corner, not centered. This setting is
// NOT save-file-persisted -- it resets to some default every launch.
constexpr uint32_t kScreenStretchRectAddrVanilla = 0x82882C68u;

// Guest address of the graphics-style *menu selection*, in the same
// Settings screen as the screen-stretch rect above: a single big-endian
// uint32, 0 = "Original", 1 = "Enhanced".
constexpr uint32_t kGraphicsStyleMenuAddrVanilla = 0x828B04A4u;

// Guest address of the *applied* graphics-style flag, 4040 bytes after the
// menu-selection address above: 0 = "Original", 2 = "Enhanced" (not a plain
// bool). Read-only from a mod's perspective -- writing it does not render
// correctly. See mods_src/graphics_settings for the read-only display.
constexpr uint32_t kGraphicsStyleAddrVanilla = 0x828B146Cu;

// Guest address near the graphics-style fields above; idle value 0xFFFFFFFF,
// pulses briefly to 8 when the Settings menu selection changes.
constexpr uint32_t kGraphicsStyleTriggerAddrVanilla = 0x82883038u;

class GameSymbolsMod : public rex::system::IModPlugin {
 public:
  explicit GameSymbolsMod(rex::Runtime* runtime) : runtime_(runtime) {}

  void OnCreateDialogs(rex::ui::ImGuiDrawer* /*drawer*/) override {
    if (runtime_ && runtime_->mod_registry()) {
      runtime_->mod_registry()->RegisterAddress("ui.accent_color", kAccentAddrVanilla,
                                                kAccentAddrTU);
      runtime_->mod_registry()->RegisterAddress("graphics.stretch_rect",
                                                kScreenStretchRectAddrVanilla);
      runtime_->mod_registry()->RegisterAddress("graphics.style",
                                                kGraphicsStyleAddrVanilla);
      runtime_->mod_registry()->RegisterAddress("graphics.style_menu",
                                                kGraphicsStyleMenuAddrVanilla);
      runtime_->mod_registry()->RegisterAddress("graphics.style_trigger",
                                                kGraphicsStyleTriggerAddrVanilla);
    }
  }

 private:
  rex::Runtime* runtime_ = nullptr;
};

}  // namespace

extern "C" REX_MOD_PLUGIN_EXPORT uint32_t rex_mod_abi_version(void) {
  return rex::system::kModPluginAbiVersion;
}

extern "C" REX_MOD_PLUGIN_EXPORT rex::system::IModPlugin* rex_mod_create(
    uint32_t abi_version, const rex::system::ModHostContext* ctx) {
  if (abi_version != rex::system::kModPluginAbiVersion || !ctx) {
    return nullptr;
  }
  return new GameSymbolsMod(ctx->runtime);
}
