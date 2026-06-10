// Aseprite
// Copyright (C) 2022-present  Igara Studio S.A.
// Copyright (C) 2001-2018  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_UI_COLSEL_SPECTRUM_H_INCLUDED
#define APP_UI_COLSEL_SPECTRUM_H_INCLUDED
#pragma once

#include "app/ui/colsel/color_selector.h"

namespace app::colsel {

class Spectrum : public ColorSelectorImpl {
public:
  Spectrum();

#if SK_ENABLE_SKSL
  std::string mainAreaShader() const override;
  std::string bottomBarShader(const ColorSelector* colSel) const override;
  void setShaderParams(SkRuntimeShaderBuilder& builder,
                       const ColorSelector* colSel,
                       const app::Color& color,
                       const bool main) override;
#endif

  app::Color getMainAreaColor(const ColorSelector* colSel,
                              int u,
                              int umax,
                              int v,
                              int vmax) override;
  app::Color getBottomBarColor(const ColorSelector* colSel, int u, int umax) override;

  void onPaintMainArea(ColorSelector* colSel, ui::Graphics* g, const gfx::Rect& rc) override;
  void onPaintBottomBar(ColorSelector* colSel, ui::Graphics* g, const gfx::Rect& rc) override;
  void onPaintSurfaceInBgThread(os::Surface* s,
                                const ColorSelector* colSel,
                                const gfx::Rect& main,
                                const gfx::Rect& bottom,
                                const gfx::Rect& alpha,
                                PaintFlags paintFlags,
                                bool& stop) override;
  PaintFlags onNeedsSurfaceRepaint(const ColorSelector* colSel,
                                   const app::Color& newColor) override;
};

} // namespace app::colsel

#endif
