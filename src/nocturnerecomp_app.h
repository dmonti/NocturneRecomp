// nocturnerecomp - ReXGlue Recompiled Project
//
// This file is yours to edit. 'rexglue migrate' will NOT overwrite it.
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <memory>

#include <rex/rex_app.h>

#ifdef _WIN32
#include <Windows.h>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")
#endif

#include "nocturnerecomp_fp_guard.h"

class NocturnerecompApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<NocturnerecompApp>(new NocturnerecompApp(ctx, "nocturnerecomp",
        PPCImageConfig));
  }

  void OnPreSetup(rex::RuntimeConfig& /*config*/) override {
#ifdef _WIN32
    timeBeginPeriod(1);
    veh_handle_ = InstallGuestFpExceptionHandlerWin();
#endif
  }

  void OnPostSetup() override {
#ifndef _WIN32
    // Install after SDK setup so we override any SDK-installed SIGFPE handler.
    veh_handle_ = InstallGuestFpExceptionHandlerPosix();
#endif
  }

  void OnShutdown() override {
    RemoveGuestFpExceptionHandler(veh_handle_);
    veh_handle_ = nullptr;
#ifdef _WIN32
    timeEndPeriod(1);
#endif
  }

 private:
  void* veh_handle_ = nullptr;
};
