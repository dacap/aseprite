// Aseprite
// Copyright (C) 2021-present  Igara Studio S.A.
// Copyright (C) 2001-2018  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_UI_COLSEL_WHEEL_H_INCLUDED
#define APP_UI_COLSEL_WHEEL_H_INCLUDED
#pragma once

#include "app/ui/colsel/color_selector.h"

namespace app::colsel {

class Wheel : public ColorSelectorImpl {
public:
  enum class ColorModel {
    RGB,
    RYB,
    NORMAL_MAP,
  };

  enum class Harmony {
    NONE,
    COMPLEMENTARY,
    MONOCHROMATIC,
    ANALOGOUS,
    SPLIT,
    TRIADIC,
    TETRADIC,
    SQUARE,
    LAST = SQUARE
  };

  Wheel(ColorSelector::Type type);

  bool isDiscrete() const { return m_discrete; }
  void setDiscrete(ColorSelector* colSel, bool state);
  void setColorModel(ColorSelector* colSel, ColorModel colorModel);
  void setHarmony(ColorSelector* colSel, Harmony harmony);

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
  PaintFlags onNeedsSurfaceRepaint(const ColorSelector* colSel,
                                   const app::Color& newColor) override;
  bool subColorPicked() override { return m_harmonyPicked; }
  void onOptions(ColorSelector* colSel) override;

private:
  int getHarmonies() const;
  app::Color getColorInHarmony(const app::Color& color, int i) const;

  // Converts an hue angle from HSV <-> current color model hue.
  // With dir == +1, the angle is from the color model and it's converted to HSV hue.
  // With dir == -1, the angle came from HSV and is converted to the current color model.
  float convertHueAngle(float angle, int dir) const;

  gfx::Rect m_wheelBounds;
  double m_wheelRadius;
  bool m_discrete;
  ColorModel m_colorModel;
  Harmony m_harmony;

  // Internal flag used to know if after pickColor() we selected an
  // harmony.
  mutable bool m_harmonyPicked;
};

} // namespace app::colsel

#endif
