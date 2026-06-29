#include "audio_player.h"

#include <string>

#include <imgui.h>

#include <rex/kernel/xam/apps/xmp_app.h>
#include <rex/string/utf8.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xam/app_manager.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/imgui_drawer.h>
#include <rex/ui/keybinds.h>

namespace nocturne {
namespace {

using XmpApp = rex::kernel::xam::apps::XmpApp;

XmpApp* GetXmpApp() {
  auto* ks = rex::system::kernel_state();
  if (!ks || !ks->app_manager()) {
    return nullptr;
  }
  return static_cast<XmpApp*>(ks->app_manager()->FindApp(0xFA));
}

std::string FilenameFromPath(const std::u16string& path) {
  std::string utf8 = rex::string::to_utf8(path);
  auto slash = utf8.find_last_of("/\\");
  if (slash != std::string::npos) {
    utf8 = utf8.substr(slash + 1);
  }
  auto dot = utf8.find_last_of('.');
  if (dot != std::string::npos) {
    utf8 = utf8.substr(0, dot);
  }
  return utf8;
}

}  // namespace

class AudioPlayerDialog : public rex::ui::ImGuiDialog {
 public:
  explicit AudioPlayerDialog(rex::ui::ImGuiDrawer* drawer) : ImGuiDialog(drawer) {
    rex::ui::RegisterBind("bind_audio_player", "F8", "Toggle audio player",
                          [this] { visible_ = !visible_; });
  }

  ~AudioPlayerDialog() override { rex::ui::UnregisterBind("bind_audio_player"); }

 protected:
  void OnDraw(ImGuiIO& io) override {
    if (!visible_) {
      return;
    }

    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Audio Player", &visible_)) {
      ImGui::End();
      return;
    }

    XmpApp* xmp = GetXmpApp();
    if (!xmp) {
      ImGui::TextUnformatted("Audio system not available.");
      ImGui::End();
      return;
    }

    const auto& songs = xmp->known_songs();
    if (songs.empty()) {
      ImGui::TextUnformatted("No tracks discovered yet.");
      ImGui::End();
      return;
    }

    bool is_playing = xmp->state() == XmpApp::State::kPlaying;
    bool is_paused = xmp->state() == XmpApp::State::kPaused;
    int current_idx = xmp->playing_known_index();
    int num_songs = static_cast<int>(songs.size());

    if (current_idx >= 0 && current_idx < num_songs) {
      std::string now = FilenameFromPath(songs[current_idx].file_path);
      ImGui::Text("%s: %s", is_playing ? "Playing" : "Paused", now.c_str());
    } else {
      ImGui::TextUnformatted("Stopped");
    }

    if (ImGui::Button("|<")) {
      int prev = current_idx > 0 ? current_idx - 1 : num_songs - 1;
      xmp->PlayKnownSong(prev);
    }
    ImGui::SameLine();
    if (is_playing) {
      if (ImGui::Button("||")) {
        xmp->XMPPause();
      }
    } else {
      if (ImGui::Button(">")) {
        if (is_paused) {
          xmp->XMPContinue();
        } else if (num_songs > 0) {
          xmp->PlayKnownSong(current_idx >= 0 ? current_idx : 0);
        }
      }
    }
    ImGui::SameLine();
    if (ImGui::Button(">|")) {
      int next = current_idx >= 0 ? (current_idx + 1) % num_songs : 0;
      xmp->PlayKnownSong(next);
    }
    ImGui::SameLine();
    if (ImGui::Button("[]")) {
      xmp->XMPStop(0);
    }

    ImGui::SameLine();
    ImGui::TextUnformatted("Vol.");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    float vol = xmp->volume();
    if (ImGui::SliderFloat("##vol", &vol, 0.0f, 1.0f, "%.2f",
                           ImGuiSliderFlags_AlwaysClamp)) {
      xmp->SetVolume(vol);
    }

    ImGui::Separator();

    ImGui::BeginChild("##tracklist", ImVec2(0, 0), true);
    for (int i = 0; i < num_songs; ++i) {
      const auto& song = songs[i];

      std::string label;
      if (!song.name.empty()) {
        label = rex::string::to_utf8(song.name);
      } else {
        label = FilenameFromPath(song.file_path);
      }

      bool is_current = (i == current_idx) && (is_playing || is_paused);
      if (is_current) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.4f, 1.0f));
      }

      char buf[512];
      snprintf(buf, sizeof(buf), "%d. %s", i + 1, label.c_str());
      if (ImGui::Selectable(buf, is_current)) {
        xmp->PlayKnownSong(i);
      }

      if (is_current) {
        ImGui::PopStyleColor();
      }
    }
    ImGui::EndChild();

    ImGui::End();
  }

 private:
  bool visible_ = false;
};

AudioPlayer::AudioPlayer() = default;
AudioPlayer::~AudioPlayer() = default;

AudioPlayer& GetAudioPlayer() {
  static AudioPlayer instance;
  return instance;
}

void AudioPlayer::Bind(rex::ui::Window* window, rex::ui::WindowedAppContext* context) {
  window_ = window;
  context_ = context;
}

void AudioPlayer::AttachDialog(rex::ui::ImGuiDrawer* drawer) {
  if (drawer && !dialog_) {
    dialog_ = std::make_unique<AudioPlayerDialog>(drawer);
  }
}

}  // namespace nocturne
