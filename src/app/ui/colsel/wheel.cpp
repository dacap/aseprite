// Aseprite
// Copyright (C) 2020-present  Igara Studio S.A.
// Copyright (C) 2001-2018  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "app/ui/colsel/wheel.h"

#include "app/color_spaces.h"
#include "app/color_utils.h"
#include "app/i18n/strings.h"
#include "app/pref/preferences.h"
#include "app/ui/skin/skin_theme.h"
#include "app/ui/status_bar.h"
#include "app/util/shader_helpers.h"
#include "base/pi.h"
#include "os/surface.h"
#include "ui/graphics.h"
#include "ui/menu.h"
#include "ui/message.h"
#include "ui/paint_event.h"
#include "ui/resize_event.h"
#include "ui/size_hint_event.h"
#include "ui/system.h"

namespace app::colsel {

using namespace app::skin;
using namespace gfx;
using namespace ui;

static struct {
  int n;
  int hues[4];
  int sats[4];
} harmonies[] = {
  { 1, { 0, 0, 0, 0 },       { 100, 0, 0, 0 }       }, // NONE
  { 2, { 0, 180, 0, 0 },     { 100, 100, 0, 0 }     }, // COMPLEMENTARY
  { 2, { 0, 0, 0, 0 },       { 100, 50, 0, 0 }      }, // MONOCHROMATIC
  { 3, { 0, 30, 330, 0 },    { 100, 100, 100, 0 }   }, // ANALOGOUS
  { 3, { 0, 150, 210, 0 },   { 100, 100, 100, 0 }   }, // SPLIT
  { 3, { 0, 120, 240, 0 },   { 100, 100, 100, 0 }   }, // TRIADIC
  { 4, { 0, 120, 180, 300 }, { 100, 100, 100, 100 } }, // TETRADIC
  { 4, { 0, 90, 180, 270 },  { 100, 100, 100, 100 } }, // SQUARE
};

Wheel::Wheel(const ColorSelector::Type type)
  : m_discrete(Preferences::instance().colorBar.discreteWheel())
  , m_colorModel((ColorModel)Preferences::instance().colorBar.wheelModel())
  , m_harmony((Harmony)Preferences::instance().colorBar.harmony())
  , m_harmonyPicked(false)
{
  switch (type) {
    case ColorSelector::Type::RGB_WHEEL: m_colorModel = colsel::Wheel::ColorModel::RGB; break;
    case ColorSelector::Type::RYB_WHEEL: m_colorModel = colsel::Wheel::ColorModel::RYB; break;
    case ColorSelector::Type::NORMAL_MAP_WHEEL:
      m_colorModel = colsel::Wheel::ColorModel::NORMAL_MAP;
      break;
  }
}

#if SK_ENABLE_SKSL

std::string Wheel::mainAreaShader() const
{
  std::string shader;
  // TODO create one shader for each wheel mode (RGB, RYB, normal)
  shader += "uniform half3 iRes;"
            "uniform half4 iHsv;"
            "uniform half4 iBack;"
            "uniform int iDiscrete;"
            "uniform int iMode;";
  shader += kHSV_to_RGB_sksl;
  shader += R"(
const half PI = 3.1415;

half rybhue_to_rgbhue(half h) {
 if (h >= 0 && h < 120) return h / 2;      // from red to yellow
 else if (h < 180) return (h-60.0);        // from yellow to green
 else if (h < 240) return 120 + 2*(h-180); // from green to blue
 else return h;                            // from blue to red (same hue)
}

half4 main(vec2 fragcoord) {
 vec2 res = vec2(min(iRes.x, iRes.y), min(iRes.x, iRes.y));
 vec2 d = (fragcoord.xy-iRes.xy/2) / res.xy;
 half r = length(d);

 if (r <= 0.5) {
  if (iMode == 2) { // Normal map mode
   float nd = r / 0.5;
   half4 rgba = half4(0, 0, 0, 1);
   float blueAngle;

   if (iDiscrete != 0) {
    float angle;
    if (nd < 1.0/6.0)
     angle = 0;
    else {
     angle = atan(-d.y, d.x);
     angle = floor(180.0 * angle / PI) + 360;
     angle = floor((angle+15) / 30) * 30;
     angle = PI * angle / 180.0;
    }
    nd = (floor(nd * 6.0 + 1.0) - 1) / 5.0;
    float blueAngleDegrees = floor(90.0 * (6.0 - floor(nd * 6.0)) / 5.0);
    blueAngle = PI * blueAngleDegrees / 180.0;

    rgba.r = 0.5 + 0.5 * nd * cos(angle);
    rgba.g = 0.5 + 0.5 * nd * sin(angle);
   }
   else {
    rgba.r = 0.5 + d.x;
    rgba.g = 0.5 - d.y;
    blueAngle = acos(nd);
   }
   rgba.b = 0.5 + 0.5 * sin(blueAngle);

   return clamp(rgba, 0.0, 1.0);
  }

  half a = atan(-d.y, d.x);
  half hue = (floor(180.0 * a / PI)
             + 180            // To avoid [-180,0) range
             + 180 + 30       // To locate green at 12 o'clock
             );

  hue = mod(hue, 360);   // To leave hue in [0,360) range
  if (iDiscrete != 0) {
   hue += 15.0;
   hue = floor(hue / 30.0);
   hue *= 30.0;
  }
  if (iMode == 1) { // RYB color wheel
   hue = rybhue_to_rgbhue(hue);
  }
  hue /= 360.0;

  half sat = r / 0.5;
  if (iDiscrete != 0) {
   sat *= 120.0;
   sat = floor(sat / 20.0);
   sat *= 20.0;
   sat /= 100.0;
   sat = clamp(sat, 0.0, 1.0);
  }
  return hsv_to_rgb(vec3(hue, sat, iHsv.w > 0 ? iHsv.z: 1.0)).rgb1;
 }
 else {
  if (iMode == 2) // Normal map mode
   return half4(0.5, 0.5, 1, 1);
  return iBack;
 }
}
)";
  return shader;
}

std::string Wheel::bottomBarShader(const ColorSelector*) const
{
  std::string shader;
  shader += "uniform half3 iRes;"
            "uniform half4 iHsv;";
  shader += kHSV_to_RGB_sksl;
  // TODO should we display the hue bar with the current sat/value?
  shader += R"(
half4 main(vec2 fragcoord) {
 half v = (fragcoord.x / iRes.x);
 return hsv_to_rgb(half3(iHsv.x, iHsv.y, v)).rgb1;
}
)";
  return shader;
}

void Wheel::setShaderParams(SkRuntimeShaderBuilder& builder,
                            const ColorSelector* colSel,
                            const app::Color& color,
                            const bool main)
{
  builder.uniform("iHsv") = appColorHsv_to_SkV4(color);
  if (main) {
    builder.uniform("iBack") = gfxColor_to_SkV4(colSel->bgColor());
    builder.uniform("iDiscrete") = (m_discrete ? 1 : 0);
    builder.uniform("iMode") = int(m_colorModel);
  }
}

#endif // SK_ENABLE_SKSL

app::Color Wheel::getMainAreaColor(const ColorSelector* colSel,
                                   const int _u,
                                   const int umax,
                                   const int _v,
                                   const int vmax)
{
  m_harmonyPicked = false;

  const app::Color color = colSel->color();
  const int u = _u - umax / 2;
  const int v = _v - vmax / 2;

  // Pick harmonies
  if (color.getAlpha() > 0) {
    const gfx::Point pos(_u, _v);
    const int n = getHarmonies();
    const int boxsize = std::min(umax / 10, vmax / 10);

    for (int i = 0; i < n; ++i) {
      const app::Color harmonyColor = getColorInHarmony(color, i);

      if (gfx::Rect(umax - (n - i) * boxsize, vmax - boxsize, boxsize, boxsize).contains(pos)) {
        m_harmonyPicked = true;

        return app::Color::fromHsv(convertHueAngle(harmonyColor.getHsvHue(), 1),
                                   harmonyColor.getHsvSaturation(),
                                   harmonyColor.getHsvValue(),
                                   color.getAlpha());
      }
    }
  }

  double d = std::sqrt(u * u + v * v);

  // When we click the main area we can limit the distance to the
  // wheel radius to pick colors even outside the wheel radius.
  if (colSel->hasCaptureInMainArea() && d > m_wheelRadius) {
    d = m_wheelRadius;
  }

  if (m_colorModel == ColorModel::NORMAL_MAP) {
    if (d <= m_wheelRadius) {
      double normalizedDistance = d / m_wheelRadius;
      double normalizedU = u / m_wheelRadius;
      double normalizedV = v / m_wheelRadius;
      double blueAngle;
      int r, g, b;

      if (m_discrete) {
        double angle = std::atan2(-v, u);

        int intAngle = (int(180.0 * angle / PI) + 360);
        intAngle += 15;
        intAngle /= 30;
        intAngle *= 30;
        angle = PI * intAngle / 180.0;

        if (normalizedDistance < 1.0 / 6.0)
          angle = 0;
        normalizedDistance = (std::floor((normalizedDistance) * 6.0 + 1.0) - 1) / 5.0;
        int blueAngleDegrees = 90.0 * (6.0 - std::floor(normalizedDistance * 6.0)) / 5.0;
        blueAngle = PI * blueAngleDegrees / 180.0;

        r = 128 + int(128.0 * normalizedDistance * std::cos(angle));
        g = 128 + int(128.0 * normalizedDistance * std::sin(angle));
      }
      else {
        r = 128 + int(128.0 * normalizedU);
        g = 128 - int(128.0 * normalizedV);

        blueAngle = std::acos(normalizedDistance);
      }

      b = 128 + int(128.0 * std::sin(blueAngle));

      return app::Color::fromRgb(std::clamp(r, 0, 255),
                                 std::clamp(g, 0, 255),
                                 std::clamp(b, 0, 255));
    }
    else {
      return app::Color::fromRgb(128, 128, 255);
    }
  }

  // Pick from the wheel
  if (d <= m_wheelRadius) {
    double a = std::atan2(-v, u);

    int hue = (int(180.0 * a / PI) + 180 // To avoid [-180,0) range
               + 180 + 30                // To locate green at 12 o'clock
    );
    if (m_discrete) {
      hue += 15;
      hue /= 30;
      hue *= 30;
    }
    hue %= 360; // To leave hue in [0,360) range
    hue = convertHueAngle(hue, 1);

    int sat;
    if (m_discrete) {
      sat = int(120.0 * d / m_wheelRadius);
      sat /= 20;
      sat *= 20;
    }
    else {
      sat = int(100.0 * d / m_wheelRadius);
    }

    return app::Color::fromHsv(std::clamp(hue, 0, 360),
                               std::clamp(sat / 100.0, 0.0, 1.0),
                               (color.getType() != Color::MaskType ? color.getHsvValue() : 1.0),
                               colSel->currentAlphaForNewColor());
  }

  return app::Color::fromMask();
}

app::Color Wheel::getBottomBarColor(const ColorSelector* colSel, const int u, const int umax)
{
  const app::Color color = colSel->color();
  const double val = double(u) / double(umax);
  return app::Color::fromHsv(color.getHsvHue(),
                             color.getHsvSaturation(),
                             std::clamp(val, 0.0, 1.0),
                             colSel->currentAlphaForNewColor());
}

void Wheel::onPaintMainArea(ColorSelector* colSel, ui::Graphics* g, const gfx::Rect& rc)
{
  const app::Color color = colSel->color();
  const bool oldHarmonyPicked = m_harmonyPicked;

  const double r = std::max(1.0, std::min(rc.w, rc.h) / 2.0);
  m_wheelRadius = r - 0.1;
  m_wheelBounds = gfx::Rect(rc.x + rc.w / 2 - r, rc.y + rc.h / 2 - r, r * 2, r * 2);

  if (color.getAlpha() > 0) {
    if (m_colorModel == ColorModel::NORMAL_MAP) {
      double normalizedRed = (double(color.getRed()) / 255.0) * 2.0 - 1.0;
      double normalizedGreen = (double(color.getGreen()) / 255.0) * 2.0 - 1.0;
      double normalizedBlue = (double(color.getBlue()) / 255.0) * 2.0 - 1.0;
      normalizedBlue = std::clamp(normalizedBlue, 0.0, 1.0);

      double x, y;

      double approximationThreshold = (246.0 / 255.0) * 2.0 - 1.0;
      if (normalizedBlue > approximationThreshold) {
        // If blue is too high, we use red and green only as approximation
        double angle = std::atan2(normalizedGreen, normalizedRed);
        double dist = std::sqrt(normalizedRed * normalizedRed + normalizedGreen * normalizedGreen);
        dist = std::clamp(dist, 0.0, 1.0);

        x = std::cos(angle) * m_wheelRadius * dist;
        y = -std::sin(angle) * m_wheelRadius * dist;
      }
      else {
        double normalizedDistance = std::cos(std::asin(normalizedBlue));
        double angle = std::atan2(normalizedGreen, normalizedRed);

        x = std::cos(angle) * m_wheelRadius * normalizedDistance;
        y = -std::sin(angle) * m_wheelRadius * normalizedDistance;
      }

      gfx::Point pos = m_wheelBounds.center() + gfx::Point(int(x), int(y));
      colSel->paintColorIndicator(g, pos, true);
    }
    else {
      int n = getHarmonies();
      int boxsize = std::min(rc.w / 10, rc.h / 10);

      ui::Paint paint;
      auto cs = get_current_color_space(g->display());

      for (int i = 0; i < n; ++i) {
        const app::Color harmonyColor = getColorInHarmony(color, i);
        const double angle = harmonyColor.getHsvHue() - 30.0;
        const double dist = harmonyColor.getHsvSaturation();
        const app::Color iColor = app::Color::fromHsv(convertHueAngle(harmonyColor.getHsvHue(), 1),
                                                      harmonyColor.getHsvSaturation(),
                                                      harmonyColor.getHsvValue());

        gfx::Point pos = m_wheelBounds.center() +
                         gfx::Point(
                           int(+std::cos(PI * angle / 180.0) * double(m_wheelRadius) * dist),
                           int(-std::sin(PI * angle / 180.0) * double(m_wheelRadius) * dist));

        colSel->paintColorIndicator(g, pos, iColor.getHsvValue() < 0.5);

        paint.color(gfx::rgba(iColor.getRed(), iColor.getGreen(), iColor.getBlue(), 255), cs.get());
        g->drawRect(
          gfx::Rect(rc.x + rc.w - (n - i) * boxsize, rc.y + rc.h - boxsize, boxsize, boxsize),
          paint);
      }
    }
  }

  m_harmonyPicked = oldHarmonyPicked;
}

void Wheel::onPaintBottomBar(ColorSelector* colSel, ui::Graphics* g, const gfx::Rect& rc)
{
  const app::Color color = colSel->color();
  if (color.getType() != app::Color::MaskType) {
    const double val = color.getHsvValue();
    const gfx::Point pos(rc.x + int(double(rc.w) * val), rc.y + rc.h / 2);
    colSel->paintColorIndicator(g, pos, val < 0.5);
  }
}

PaintFlags Wheel::onNeedsSurfaceRepaint(const ColorSelector* colSel, const app::Color& newColor)
{
  const app::Color color = colSel->color();
  return
    // Only if the saturation changes we have to redraw the main surface.
    (m_colorModel != ColorModel::NORMAL_MAP &&
         cs_double_diff(color.getHsvValue(), newColor.getHsvValue()) ?
       PaintFlags::MainArea :
       PaintFlags::None) |
    (cs_double_diff(color.getHsvHue(), newColor.getHsvHue()) ||
         cs_double_diff(color.getHsvSaturation(), newColor.getHsvSaturation()) ?
       PaintFlags::BottomBar :
       PaintFlags::None);
}

void Wheel::setDiscrete(ColorSelector* colSel, bool state)
{
  m_discrete = state;
  Preferences::instance().colorBar.discreteWheel(m_discrete);

  colSel->repaintAllAreas();
}

void Wheel::setColorModel(ColorSelector* colSel, ColorModel colorModel)
{
  m_colorModel = colorModel;
  Preferences::instance().colorBar.wheelModel((int)m_colorModel);

  colSel->invalidate();
}

void Wheel::setHarmony(ColorSelector* colSel, Harmony harmony)
{
  m_harmony = harmony;
  Preferences::instance().colorBar.harmony((int)m_harmony);

  colSel->invalidate();
}

int Wheel::getHarmonies() const
{
  int i = std::clamp((int)m_harmony, 0, (int)Harmony::LAST);
  return harmonies[i].n;
}

app::Color Wheel::getColorInHarmony(const app::Color& color, int j) const
{
  int i = std::clamp((int)m_harmony, 0, (int)Harmony::LAST);
  j = std::clamp(j, 0, harmonies[i].n - 1);
  double hue = convertHueAngle(color.getHsvHue(), -1) + harmonies[i].hues[j];
  double sat = color.getHsvSaturation() * harmonies[i].sats[j] / 100.0;
  return app::Color::fromHsv(std::fmod(hue, 360), std::clamp(sat, 0.0, 1.0), color.getHsvValue());
}

void Wheel::onOptions(ColorSelector* colSel)
{
  Menu menu;
  MenuItem discrete(Strings::color_wheel_discrete());
  MenuItem none(Strings::color_wheel_no_harmonies());
  MenuItem complementary(Strings::color_wheel_complementary());
  MenuItem monochromatic(Strings::color_wheel_monochromatic());
  MenuItem analogous(Strings::color_wheel_analogous());
  MenuItem split(Strings::color_wheel_split_complementary());
  MenuItem triadic(Strings::color_wheel_triadic());
  MenuItem tetradic(Strings::color_wheel_tetradic());
  MenuItem square(Strings::color_wheel_square());
  menu.addChild(&discrete);
  if (m_colorModel != ColorModel::NORMAL_MAP) {
    menu.addChild(new MenuSeparator);
    menu.addChild(&none);
    menu.addChild(&complementary);
    menu.addChild(&monochromatic);
    menu.addChild(&analogous);
    menu.addChild(&split);
    menu.addChild(&triadic);
    menu.addChild(&tetradic);
    menu.addChild(&square);
  }

  if (isDiscrete())
    discrete.setSelected(true);
  discrete.Click.connect([this, colSel] { setDiscrete(colSel, !isDiscrete()); });

  if (m_colorModel != ColorModel::NORMAL_MAP) {
    switch (m_harmony) {
      case Harmony::NONE:          none.setSelected(true); break;
      case Harmony::COMPLEMENTARY: complementary.setSelected(true); break;
      case Harmony::MONOCHROMATIC: monochromatic.setSelected(true); break;
      case Harmony::ANALOGOUS:     analogous.setSelected(true); break;
      case Harmony::SPLIT:         split.setSelected(true); break;
      case Harmony::TRIADIC:       triadic.setSelected(true); break;
      case Harmony::TETRADIC:      tetradic.setSelected(true); break;
      case Harmony::SQUARE:        square.setSelected(true); break;
    }
    none.Click.connect([this, colSel] { setHarmony(colSel, Harmony::NONE); });
    complementary.Click.connect([this, colSel] { setHarmony(colSel, Harmony::COMPLEMENTARY); });
    monochromatic.Click.connect([this, colSel] { setHarmony(colSel, Harmony::MONOCHROMATIC); });
    analogous.Click.connect([this, colSel] { setHarmony(colSel, Harmony::ANALOGOUS); });
    split.Click.connect([this, colSel] { setHarmony(colSel, Harmony::SPLIT); });
    triadic.Click.connect([this, colSel] { setHarmony(colSel, Harmony::TRIADIC); });
    tetradic.Click.connect([this, colSel] { setHarmony(colSel, Harmony::TETRADIC); });
    square.Click.connect([this, colSel] { setHarmony(colSel, Harmony::SQUARE); });
  }

  gfx::Rect rc = colSel->optionsButton().bounds();
  menu.showPopup(gfx::Point(rc.x2(), rc.y), colSel->display());
}

float Wheel::convertHueAngle(float h, int dir) const
{
  if (m_colorModel == ColorModel::RYB) {
    if (dir == 1) {
      // rybhue_to_rgbhue() maps:
      //   [0,120) -> [0,60)
      //   [120,180) -> [60,120)
      //   [180,240) -> [120,240)
      //   [240,360] -> [240,360]
      if (h >= 0 && h < 120)
        return h / 2; // from red to yellow
      else if (h < 180)
        return (h - 60); // from yellow to green
      else if (h < 240)
        return 120 + 2 * (h - 180); // from green to blue
      else
        return h; // from blue to red (same hue)
    }
    else {
      // rgbhue_to_rybhue()
      //   [0,60) -> [0,120)
      //   [60,120) -> [120,180)
      //   [120,240) -> [180,240)
      //   [240,360] -> [240,360]
      if (h >= 0 && h < 60)
        return 2 * h; // from red to yellow
      else if (h < 120)
        return 60 + h; // from yellow to green
      else if (h < 240)
        return 180 + (h - 120) / 2; // from green to blue
      else
        return h; // from blue to red (same hue)
    }
  }
  return h;
}

} // namespace app::colsel
