// Aseprite
// Copyright (C) 2020-present  Igara Studio S.A.
// Copyright (C) 2016-2018  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "app/ui/colsel/tint_shade_tone.h"

#include "app/color_utils.h"
#include "app/pref/preferences.h"
#include "app/ui/skin/skin_theme.h"
#include "app/util/shader_helpers.h"
#include "ui/graphics.h"

#include <algorithm>

namespace app::colsel {

using namespace app::skin;
using namespace gfx;
using namespace ui;

TintShadeTone::TintShadeTone()
{
}

#if SK_ENABLE_SKSL

std::string TintShadeTone::mainAreaShader() const
{
  std::string shader;
  shader += "uniform half3 iRes;"
            "uniform half4 iHsv;";
  shader += kHSV_to_RGB_sksl;
  shader += R"(
half4 main(vec2 fragcoord) {
 vec2 d = fragcoord.xy / iRes.xy;
 half hue = iHsv.x;
 half sat = d.x;
 half val = 1.0 - d.y;
 return hsv_to_rgb(vec3(hue, sat, val)).rgb1;
}
)";
  return shader;
}

std::string TintShadeTone::bottomBarShader(const ColorSelector* colSel) const
{
  std::string shader;
  shader += "uniform half3 iRes;"
            "uniform half4 iHsv;";
  shader += kHSV_to_RGB_sksl;

  if (colSel->hueWithSatValue()) {
    shader += R"(
half4 main(vec2 fragcoord) {
 half h = (fragcoord.x / iRes.x);
 return hsv_to_rgb(half3(h, iHsv.y, iHsv.z)).rgb1;
}
)";
  }
  else {
    shader += R"(
half4 main(vec2 fragcoord) {
 half h = (fragcoord.x / iRes.x);
 return hsv_to_rgb(half3(h, 1.0, 1.0)).rgb1;
}
)";
  }
  return shader;
}

void TintShadeTone::setShaderParams(SkRuntimeShaderBuilder& builder,
                                    const ColorSelector* colSel,
                                    const app::Color& color,
                                    const bool main)
{
  builder.uniform("iHsv") = appColorHsv_to_SkV4(color);
}

#endif // SK_ENABLE_SKSL

app::Color TintShadeTone::getMainAreaColor(const ColorSelector* colSel,
                                           const int u,
                                           const int umax,
                                           const int v,
                                           const int vmax)
{
  const app::Color color = colSel->color();
  const double sat = (1.0 * u / umax);
  const double val = (1.0 - double(v) / double(vmax));
  return app::Color::fromHsv(color.getHsvHue(),
                             std::clamp(sat, 0.0, 1.0),
                             std::clamp(val, 0.0, 1.0),
                             colSel->currentAlphaForNewColor());
}

app::Color TintShadeTone::getBottomBarColor(const ColorSelector* colSel,
                                            const int u,
                                            const int umax)
{
  const app::Color color = colSel->color();
  const double hue = (360.0 * u / umax);
  return app::Color::fromHsv(std::clamp(hue, 0.0, 360.0),
                             color.getHsvSaturation(),
                             color.getHsvValue(),
                             colSel->currentAlphaForNewColor());
}

void TintShadeTone::onPaintMainArea(ColorSelector* colSel, ui::Graphics* g, const gfx::Rect& rc)
{
  const app::Color color = colSel->color();

  if (color.getType() != app::Color::MaskType) {
    const double sat = color.getHsvSaturation();
    const double val = color.getHsvValue();
    const gfx::Point pos(rc.x + int(sat * rc.w), rc.y + int((1.0 - val) * rc.h));

    colSel->paintColorIndicator(g, pos, val < 0.5);
  }
}

void TintShadeTone::onPaintBottomBar(ColorSelector* colSel, ui::Graphics* g, const gfx::Rect& rc)
{
  const app::Color color = colSel->color();

  if (color.getType() != app::Color::MaskType) {
    const double hue = color.getHsvHue();
    double val;
    if (colSel->hueWithSatValue())
      val = color.getHsvValue();
    else
      val = 1.0;

    const gfx::Point pos(rc.x + int(rc.w * hue / 360.0), rc.y + rc.h / 2);
    colSel->paintColorIndicator(g, pos, val < 0.5);
  }
}

void TintShadeTone::onPaintSurfaceInBgThread(os::Surface* s,
                                             const ColorSelector* colSel,
                                             const gfx::Rect& main,
                                             const gfx::Rect& bottom,
                                             const gfx::Rect& alpha,
                                             const PaintFlags paintFlags,
                                             bool& stop)
{
  const app::Color color = colSel->color();
  const double hue = color.getHsvHue();
  const int umax = std::max(1, main.w - 1);
  const int vmax = std::max(1, main.h - 1);

  if ((paintFlags & PaintFlags::MainArea) == PaintFlags::MainArea) {
    for (int y = 0; y < main.h && !stop; ++y) {
      for (int x = 0; x < main.w && !stop; ++x) {
        const double sat = double(x) / double(umax);
        const double val = 1.0 - double(y) / double(vmax);

        const gfx::Color color = color_utils::color_for_ui(
          app::Color::fromHsv(hue, std::clamp(sat, 0.0, 1.0), std::clamp(val, 0.0, 1.0)));

        s->putPixel(color, main.x + x, main.y + y);
      }
    }
    if (stop)
      return;
  }

  if ((paintFlags & PaintFlags::BottomBar) == PaintFlags::BottomBar) {
    os::Paint paint;
    double sat, val;

    if (colSel->hueWithSatValue()) {
      sat = color.getHsvSaturation();
      val = color.getHsvValue();
    }
    else {
      sat = 1.0;
      val = 1.0;
    }

    for (int x = 0; x < bottom.w && !stop; ++x) {
      paint.color(color_utils::color_for_ui(app::Color::fromHsv((360.0 * x / bottom.w), sat, val)));

      s->drawRect(gfx::Rect(bottom.x + x, bottom.y, 1, bottom.h), paint);
    }
    if (stop)
      return;
  }
}

PaintFlags TintShadeTone::onNeedsSurfaceRepaint(const ColorSelector* colSel,
                                                const app::Color& newColor)
{
  const app::Color color = colSel->color();
  PaintFlags flags =
    // Only if the hue changes we have to redraw the main surface.
    (cs_double_diff(color.getHsvHue(), newColor.getHsvHue()) ? PaintFlags::MainArea :
                                                               PaintFlags::None);

  if (colSel->hueWithSatValue()) {
    flags |= (cs_double_diff(color.getHsvSaturation(), newColor.getHsvSaturation()) ||
                  cs_double_diff(color.getHsvValue(), newColor.getHsvValue()) ?
                PaintFlags::BottomBar :
                PaintFlags::None);
  }
  return flags;
}

} // namespace app::colsel
