// Aseprite
// Copyright (C) 2019  Igara Studio S.A.
// Copyright (C) 2001-2018  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_PREF_OPTION_H_INCLUDED
#define APP_PREF_OPTION_H_INCLUDED
#pragma once

// Uncomment to trace preference changes
// #define TRACE_PREF_CHANGES 1

#include "obs/signal.h"

#ifdef ENABLE_SCRIPTING
  #include "app/script/values.h"
#endif

#if TRACE_PREF_CHANGES
  #include "app/color.h"
  #include "base/convert_to.h"
  #include "gfx/point_io.h"
  #include "gfx/rect_io.h"
  #include "gfx/size_io.h"
#endif

#include <string>
#include <type_traits>

namespace app {

class OptionBase;

class Section {
public:
  explicit Section(const std::string& name) : m_name(name) {}
  virtual ~Section() = default;
  const char* name() const { return m_name.c_str(); }

  virtual Section* section(const char* id) = 0;
  virtual OptionBase* option(const char* id) = 0;
  virtual std::vector<OptionBase*> optionList() const { return {}; }
  virtual std::vector<Section*> sectionList() const { return {}; }
  virtual void save() = 0;

  obs::signal<void()> BeforeChange;
  obs::signal<void()> AfterChange;

private:
  std::string m_name;
};

class OptionBase {
public:
  OptionBase(Section* section, const char* id) : m_section(section), m_id(id) {}
  virtual ~OptionBase() = default;
  const char* section() const { return m_section->name(); }
  const char* id() const { return m_id; }
  virtual void resetToDefault() = 0;

#ifdef ENABLE_SCRIPTING
  virtual void pushLua(lua_State* L) = 0;
  virtual void fromLua(lua_State* L, int index) = 0;
#endif

protected:
  Section* m_section;
  const char* m_id;
};

template<typename T>
class Option : public OptionBase {
public:
  Option(Section* section, const char* id, const T& defaultValue = T())
    : OptionBase(section, id)
    , m_default(defaultValue)
    , m_value(defaultValue)
    , m_dirty(false)
  {
  }

  const T& operator()() const { return m_value; }

  const T& operator()(const T& newValue)
  {
    setValue(newValue);
    return m_value;
  }

  // operator=() changes the value and the default value of the
  // option.  E.g. it's used when we copy two complete Sections,
  // from default settings to user settings, or viceversa (the
  // default value is always involved).
  Option& operator=(const Option& opt)
  {
    setValueAndDefault(opt.m_value);
    return *this;
  }

  // Changes the default value and the current one.
  void setValueAndDefault(const T& value)
  {
    setDefaultValue(value);
    setValue(value, false);
  }

  const T& defaultValue() const { return m_default; }
  void setDefaultValue(const T& defValue) { m_default = defValue; }

  bool isDirty() const { return m_dirty; }
  void forceDirtyFlag() { m_dirty = true; }
  void cleanDirtyFlag() { m_dirty = false; }

  void setValue(const T& newValue, const bool touchDirty = true)
  {
#if TRACE_PREF_CHANGES
    if (touchDirty && m_section) {
      std::stringstream s;
      if constexpr (std::is_enum<T>::value) {
        s << std::underlying_type_t<T>(newValue);
      }
      else if constexpr (std::is_scalar<T>::value) {
        s << newValue;
      }
      else if constexpr (std::is_same<T, app::Color>::value) {
        s << newValue.toString();
      }
      else {
        s << newValue;
      }
      TRACE("%s.%s = %s\n", m_section->name(), id(), s.str().c_str());
    }
#endif // TRACE_PREF_CHANGES

    if (m_value == newValue) {
      if (touchDirty)
        m_dirty = true;
      return;
    }

    BeforeChange(newValue);
    if (m_section)
      m_section->BeforeChange();

    m_value = newValue;
    if (touchDirty)
      m_dirty = true;

    AfterChange(newValue);
    if (m_section)
      m_section->AfterChange();
  }

  void clearValue()
  {
    m_value = m_default;
    m_dirty = false;
  }

  void resetToDefault() override { setValue(m_default); }

#ifdef ENABLE_SCRIPTING
  void pushLua(lua_State* L) override { script::push_value_to_lua<T>(L, m_value); }
  void fromLua(lua_State* L, int index) override
  {
    setValue(script::get_value_from_lua<T>(L, index));
  }
#endif

  obs::signal<void(const T&)> BeforeChange;
  obs::signal<void(const T&)> AfterChange;

private:
  T m_default;
  T m_value;
  bool m_dirty;

  Option();
  Option(const Option& opt);
};

} // namespace app

#endif
