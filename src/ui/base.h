// Aseprite UI Library
// Copyright (C) 2018-present  Igara Studio S.A.
// Copyright (C) 2001-2016  David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifndef UI_BASE_H_INCLUDED
#define UI_BASE_H_INCLUDED
#pragma once

#include "base/enum_flags.h"
#include "base/ints.h"

namespace ui {

// clang-format off

enum WidgetFlags : uint32_t {
  NOFLAGS          = 0x00000000,
  HIDDEN           = 0x00000001, // Is hidden (not visible, not clickeable).
  SELECTED         = 0x00000002, // Is selected.
  DISABLED         = 0x00000004, // Is disabled (not usable).
  HAS_FOCUS        = 0x00000008, // Has the input focus.
  HAS_MOUSE        = 0x00000010, // Has the mouse.
  HAS_CAPTURE      = 0x00000020, // Captured the mouse .
  FOCUS_STOP       = 0x00000040, // The widget support the focus on it.
  FOCUS_MAGNET     = 0x00000080, // The widget wants the focus by default (e.g. when the dialog is shown
                                 // by first time).
  EXPANSIVE        = 0x00000100, // Is expansive (want more space).
  DECORATIVE       = 0x00000200, // To decorate windows.
  INITIALIZED      = 0x00000400, // The widget was already initialized by a theme.
  DIRTY            = 0x00000800, // The widget (or one child) is dirty (update_region != empty).
  HAS_TEXT         = 0x00001000, // The widget has text (at least setText() was called one time).
  DOUBLE_BUFFERED  = 0x00002000, // The widget is painted in a back-buffer and then flipped to the
                                 // main display
  TRANSPARENT      = 0x00004000, // The widget has transparent parts that needs the background painted
                                 // before
  CTRL_RIGHT_CLICK = 0x00008000, // The widget should transform Ctrl+click to right-click on OS X.
  ALLOW_DROP       = 0x00010000, // The widget can participate as a drop target in a drag & drop
                                 // operation.
  IGNORE_MOUSE     = 0x00020000, // Don't process mouse messages for this widget (useful for labels,
                                 // boxes, grids, etc.)
  FLAGS_MASK       = 0x000fffff,
};

enum WidgetAlign : uint16_t {
  NOALIGN     = 0x0000,
  HORIZONTAL  = 0x0001,
  VERTICAL    = 0x0002,
  LEFT        = 0x0004,
  CENTER      = 0x0008,
  RIGHT       = 0x0010,
  TOP         = 0x0020,
  MIDDLE      = 0x0040,
  BOTTOM      = 0x0080,
  HOMOGENEOUS = 0x0100,
  WORDWRAP    = 0x0200,
  CHARWRAP    = 0x0400,
  ALIGN_MASK  = 0x0fff,
};

// clang-format on

LAF_ENUM_FLAGS(WidgetFlags)
LAF_ENUM_FLAGS(WidgetAlign)

using fa_t = uint32_t; // fa = flags + alignment

constexpr fa_t make_fa(const WidgetFlags flags, const WidgetAlign align)
{
  return (flags & FLAGS_MASK) | ((align & ALIGN_MASK) << 20);
}

constexpr WidgetFlags get_flags_from_fa(const fa_t fa)
{
  return (WidgetFlags)(fa & FLAGS_MASK);
}

constexpr WidgetAlign get_align_from_fa(const fa_t fa)
{
  return (WidgetAlign)((fa >> 20) & ALIGN_MASK);
}

} // namespace ui

#endif // UI_BASE_H_INCLUDED
