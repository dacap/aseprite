// Aseprite
// Copyright (C) 2018-present  Igara Studio S.A.
// Copyright (C) 2016-2018  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_UI_COLSEL_COLOR_SELECTOR_H_INCLUDED
#define APP_UI_COLSEL_COLOR_SELECTOR_H_INCLUDED
#pragma once

#include "app/color.h"
#include "app/ui/color_source.h"
#include "base/enum_flags.h"
#include "obs/connection.h"
#include "obs/signal.h"
#include "os/surface.h"
#include "ui/button.h"
#include "ui/mouse_button.h"
#include "ui/widget.h"

#include <cmath>

// TODO We should wrap the SkRuntimeEffect in laf-os, SkRuntimeEffect
//      and SkRuntimeShaderBuilder might change in future Skia
//      versions.
#if SK_ENABLE_SKSL
  #include "include/effects/SkRuntimeEffect.h"
#endif

// TODO move this to laf::base
inline bool cs_double_diff(double a, double b)
{
  return std::fabs((a) - (b)) > 0.001;
}

namespace app::colsel {

class ColorSelector;

// Flags for to know what part to paint.
enum class PaintFlags {
  None = 0,
  MainArea = 1,
  BottomBar = 2,
  AlphaBar = 4,
  AllAreas = MainArea | BottomBar | AlphaBar,
};

LAF_ENUM_FLAGS(PaintFlags);

class ColorSelectorImpl {
public:
  virtual ~ColorSelectorImpl() {}

#if SK_ENABLE_SKSL
  virtual std::string mainAreaShader() const = 0;
  virtual std::string bottomBarShader(const ColorSelector* colSel) const = 0;
  virtual void setShaderParams(SkRuntimeShaderBuilder& builder,
                               const ColorSelector* colSel,
                               const app::Color& color,
                               const bool main) = 0;
#endif

  virtual app::Color getMainAreaColor(const ColorSelector* colSel,
                                      int u,
                                      int umax,
                                      int v,
                                      int vmax) = 0;
  virtual app::Color getBottomBarColor(const ColorSelector* colSel, int u, int umax) = 0;

  virtual void onPaintMainArea(ColorSelector* colSel, ui::Graphics* g, const gfx::Rect& rc) = 0;
  virtual void onPaintBottomBar(ColorSelector* colSel, ui::Graphics* g, const gfx::Rect& rc) = 0;
  virtual PaintFlags onNeedsSurfaceRepaint(const ColorSelector* colSel,
                                           const app::Color& newColor) = 0;
  virtual void onOptions(ColorSelector*) {}
  virtual bool subColorPicked() { return false; }
};

class ColorSelector : public ui::Widget,
                      public IColorSource {
public:
  class Painter;

  enum Type {
    NONE,
    SPECTRUM,
    RGB_WHEEL,
    RYB_WHEEL,
    TINT_SHADE_TONE,
    NORMAL_MAP_WHEEL,
    NTYPES,
  };

  ColorSelector();
  ~ColorSelector();

  Type type() const { return m_type; }
  void setType(const Type type);

  void selectColor(const app::Color& color);
  app::Color color() const { return m_color; }

  // Returns the 255 if m_color is the mask color, or the
  // m_color.getAlpha() if it's really a color.
  int currentAlphaForNewColor() const;

  // IColorSource impl
  app::Color getColorByPosition(const gfx::Point& pos) override;

  // To be called by ColorSelectorImpl functions
  void paintColorIndicator(ui::Graphics* g, const gfx::Point& pos, const bool white);
  bool hueWithSatValue() const { return m_hueWithSatValue; }
  bool hasCaptureInMainArea() const { return m_capturedInMain; }
  ui::Button& optionsButton() { return m_options; }
  void repaintAllAreas()
  {
    m_paintFlags = PaintFlags::AllAreas;
    invalidate();
  }

  // Signals
  obs::signal<void(const app::Color&, ui::MouseButton)> ColorChange;

protected:
  void onSizeHint(ui::SizeHintEvent& ev) override;
  bool onProcessMessage(ui::Message* msg) override;
  void onInitTheme(ui::InitThemeEvent& ev) override;
  void onResize(ui::ResizeEvent& ev) override;
  void onPaint(ui::PaintEvent& ev) override;

  // Functions redirected to ColorSelectorImpl
#if SK_ENABLE_SKSL
  const char* mainAreaShader() const;
  const char* bottomBarShader() const;
  void setShaderParams(SkRuntimeShaderBuilder& builder, const app::Color& color, const bool main)
  {
    if (m_impl)
      m_impl->setShaderParams(builder, this, color, main);
  }
#endif
  app::Color getMainAreaColor(const int u, const int umax, const int v, const int vmax)
  {
    return m_impl ? m_impl->getMainAreaColor(this, u, umax, v, vmax) : app::Color();
  }
  app::Color getBottomBarColor(const int u, const int umax)
  {
    return m_impl ? m_impl->getBottomBarColor(this, u, umax) : app::Color();
  }
  PaintFlags onNeedsSurfaceRepaint(const app::Color& newColor);
  bool subColorPicked() { return m_impl ? m_impl->subColorPicked() : false; }

  app::Color m_color;

  // These flags indicate which areas must be repainted in onPaint().
  PaintFlags m_paintFlags;

private:
  app::Color getAlphaBarColor(const int u, const int umax);
  void onPaintAlphaBar(ui::Graphics* g, const gfx::Rect& rc);

  gfx::Rect bottomBarBounds() const;
  gfx::Rect alphaBarBounds() const;

  void updateColorSpace();

#if SK_ENABLE_SKSL
  static const char* alphaBarShader();
  bool buildEffects();
#endif

  Type m_type = Type::NONE;
  std::unique_ptr<ColorSelectorImpl> m_impl;
  bool m_hueWithSatValue = false;

  // Internal flag used to lock the modification of m_color.
  // E.g. When the user picks a color harmony, we don't want to
  // change the main color.
  bool m_lockColor;

  // True when the user pressed the mouse button in the bottom
  // slider. It's used to avoid swapping in both areas (main color
  // area vs bottom slider) when we drag the mouse above this
  // widget.
  bool m_capturedInBottom = false;
  bool m_capturedInAlpha = false;
  bool m_capturedInMain = false;

  ui::Button m_options;

  obs::scoped_connection m_appConn;
  obs::scoped_connection m_hueConn;

#if SK_ENABLE_SKSL
  mutable std::string m_mainShader;
  mutable std::string m_bottomShader;
  sk_sp<SkRuntimeEffect> m_mainEffect;
  sk_sp<SkRuntimeEffect> m_bottomEffect;
  static sk_sp<SkRuntimeEffect> m_alphaEffect;
#endif

  static os::Surface* getTempCanvas(ui::Display* display, int w, int h, gfx::Color bgColor);
  static os::SurfaceRef m_tempCanvas;
};

} // namespace app::colsel

#endif
