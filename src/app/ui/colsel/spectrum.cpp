// Aseprite
// Copyright (C) 2020-present  Igara Studio S.A.
// Copyright (C) 2001-2018  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "app/ui/colsel/spectrum.h"

#include "app/color_utils.h"
#include "app/ui/skin/skin_theme.h"
#include "app/ui/status_bar.h"
#include "app/util/shader_helpers.h"
#include "os/surface.h"
#include "ui/graphics.h"
#include "ui/message.h"
#include "ui/paint_event.h"
#include "ui/resize_event.h"
#include "ui/size_hint_event.h"
#include "ui/system.h"

#include <algorithm>

namespace app::colsel {

using namespace app::skin;
using namespace gfx;
using namespace ui;

Spectrum::Spectrum()
{
}

#if SK_ENABLE_SKSL

std::string Spectrum::mainAreaShader() const
{
  return R"(
uniform half3 iRes;
uniform half4 iHsl;

half4 main(vec2 fragcoord) {
 vec2 d = fragcoord.xy / iRes.xy;
 half hue = d.x;
 half sat = iHsl.y;
 half lit = 1.0 - d.y;
 return $hsl_to_rgb(half3(hue, sat, lit)).rgb1;
}
)";
}

std::string Spectrum::bottomBarShader(const ColorSelector*) const
{
  return R"(
uniform half3 iRes;
uniform half4 iHsl;

half4 main(vec2 fragcoord) {
 half s = (fragcoord.x / iRes.x);
 return $hsl_to_rgb(half3(iHsl.x, s, iHsl.z)).rgb1;
}
)";
}

void Spectrum::setShaderParams(SkRuntimeShaderBuilder& builder,
                               const ColorSelector* colSel,
                               const app::Color& color,
                               const bool main)
{
  builder.uniform("iHsl") = appColorHsl_to_SkV4(color);
}

#endif // SK_ENABLE_SKSL

app::Color Spectrum::getMainAreaColor(const ColorSelector* colSel,
                                      const int u,
                                      const int umax,
                                      const int v,
                                      const int vmax)
{
  const app::Color color = colSel->color();
  const double hue = 360.0 * u / umax;
  const double lit = 1.0 - (double(v) / double(vmax));
  return app::Color::fromHsl(std::clamp(hue, 0.0, 360.0),
                             color.getHslSaturation(),
                             std::clamp(lit, 0.0, 1.0),
                             colSel->currentAlphaForNewColor());
}

app::Color Spectrum::getBottomBarColor(const ColorSelector* colSel, const int u, const int umax)
{
  const app::Color color = colSel->color();
  const double sat = double(u) / double(umax);
  return app::Color::fromHsl(color.getHslHue(),
                             std::clamp(sat, 0.0, 1.0),
                             color.getHslLightness(),
                             colSel->currentAlphaForNewColor());
}

void Spectrum::onPaintMainArea(ColorSelector* colSel, ui::Graphics* g, const gfx::Rect& rc)
{
  const app::Color color = colSel->color();

  if (color.getType() != app::Color::MaskType) {
    const double hue = color.getHslHue();
    const double lit = color.getHslLightness();
    const gfx::Point pos(rc.x + int(hue * rc.w / 360.0), rc.y + rc.h - int(lit * rc.h));

    colSel->paintColorIndicator(g, pos, lit < 0.5);
  }
}

void Spectrum::onPaintBottomBar(ColorSelector* colSel, ui::Graphics* g, const gfx::Rect& rc)
{
  const app::Color color = colSel->color();
  const double lit = color.getHslLightness();

  if (color.getType() != app::Color::MaskType) {
    const double sat = color.getHslSaturation();
    const gfx::Point pos(rc.x + int(double(rc.w) * sat), rc.y + rc.h / 2);
    colSel->paintColorIndicator(g, pos, lit < 0.5);
  }
}

void Spectrum::onPaintSurfaceInBgThread(os::Surface* s,
                                        const ColorSelector* colSel,
                                        const gfx::Rect& main,
                                        const gfx::Rect& bottom,
                                        const gfx::Rect& alpha,
                                        PaintFlags paintFlags,
                                        bool& stop)
{
  const app::Color color = colSel->color();

  if ((paintFlags & PaintFlags::MainArea) == PaintFlags::MainArea) {
    const double sat = color.getHslSaturation();
    const int umax = std::max(1, main.w - 1);
    const int vmax = std::max(1, main.h - 1);

    for (int y = 0; y < main.h && !stop; ++y) {
      for (int x = 0; x < main.w && !stop; ++x) {
        const double hue = 360.0 * double(x) / double(umax);
        const double lit = 1.0 - double(y) / double(vmax);

        const gfx::Color c = color_utils::color_for_ui(
          app::Color::fromHsl(std::clamp(hue, 0.0, 360.0), sat, std::clamp(lit, 0.0, 1.0)));

        s->putPixel(c, main.x + x, main.y + y);
      }
    }
    if (stop)
      return;
  }

  if ((paintFlags & PaintFlags::BottomBar) == PaintFlags::BottomBar) {
    double lit = color.getHslLightness();
    double hue = color.getHslHue();
    os::Paint paint;
    for (int x = 0; x < bottom.w && !stop; ++x) {
      paint.color(
        color_utils::color_for_ui(app::Color::fromHsl(hue, double(x) / double(bottom.w), lit)));
      s->drawRect(gfx::Rect(bottom.x + x, bottom.y, 1, bottom.h), paint);
    }
  }
}

PaintFlags Spectrum::onNeedsSurfaceRepaint(const ColorSelector* colSel, const app::Color& newColor)
{
  const app::Color color = colSel->color();
  return
    // Only if the saturation changes we have to redraw the main surface.
    (cs_double_diff(color.getHslSaturation(), newColor.getHslSaturation()) ? PaintFlags::MainArea :
                                                                             PaintFlags::None) |
    (cs_double_diff(color.getHslHue(), newColor.getHslHue()) ||
         cs_double_diff(color.getHslLightness(), newColor.getHslLightness()) ?
       PaintFlags::BottomBar :
       PaintFlags::None);
}

} // namespace app::colsel
