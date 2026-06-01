// Aseprite Code Generator
// Copyright (c) 2021-present Igara Studio S.A.
// Copyright (c) 2014-2017 David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#include "base/exception.h"
#include "base/file_handle.h"
#include "base/fs.h"
#include "base/program_options.h"
#include "base/string.h"
#include "gen/check_strings.h"
#include "gen/pref_types.h"
#include "gen/strings_class.h"
#include "gen/theme_class.h"
#include "gen/ui_class.h"
#include "tinyxml2.h"

#include <iostream>
#include <memory>

using PO = base::ProgramOptions;
using namespace tinyxml2;

static std::unique_ptr<XMLDocument> load_xml_doc(const std::string& filename)
{
  base::FileHandle inputFile(base::open_file(filename, "rb"));
  if (!inputFile)
    throw base::Exception(filename + " not found");

  auto doc = std::make_unique<XMLDocument>();
  if (doc->LoadFile(inputFile.get()) != XML_SUCCESS) {
    std::cerr << filename << ":" << doc->ErrorLineNum() << ": "
              << "error " << int(doc->ErrorID()) << ": " << doc->ErrorStr() << "\n";
    throw std::runtime_error("invalid xml file");
  }
  return doc;
}

static void run(int argc, const char* argv[])
{
  PO po;
  PO::Option& inputOpt = po.add("input").requiresValue("<filename>");
  PO::Option& widgetId = po.add("widgetid").requiresValue("<id>");
  PO::Option& prefH = po.add("pref-h");
  PO::Option& prefCpp = po.add("pref-cpp");
  PO::Option& theme = po.add("theme");
  PO::Option& strings = po.add("strings");
  PO::Option& commandIds = po.add("command-ids");
  PO::Option& widgetsDir = po.add("widgets-dir").requiresValue("<dir>");
  PO::Option& stringsDir = po.add("strings-dir").requiresValue("<dir>");
  PO::Option& guiFile = po.add("gui-file").requiresValue("<filename>");
  PO::Option& prefFile = po.add("pref-file").requiresValue("<filename>");
  po.parse(argc, argv);

  // Try to load the input XML file (pref.xml, theme.xml, widget.xml, etc.)
  std::unique_ptr<XMLDocument> doc;
  std::string inputFilename = po.value_of(inputOpt);
  if (!inputFilename.empty() && base::get_file_extension(inputFilename) == "xml")
    doc = load_xml_doc(inputFilename);

  if (doc) {
    // Generate widget class
    if (po.enabled(widgetId)) {
      std::unique_ptr<XMLDocument> prefDoc;
      if (const auto prefFn = po.value_of(prefFile); !prefFn.empty())
        prefDoc = load_xml_doc(prefFn);
      else
        throw base::Exception("please specify --pref-file option when using --widgetid");

      gen_ui_class(doc.get(), prefDoc.get(), inputFilename, po.value_of(widgetId));
    }
    // Generate preference header file
    else if (po.enabled(prefH))
      gen_pref_header(doc.get(), inputFilename);
    // Generate preference c++ file
    else if (po.enabled(prefCpp))
      gen_pref_impl(doc.get(), inputFilename);
    // Generate theme class
    else if (po.enabled(theme))
      gen_theme_class(doc.get(), inputFilename);
  }
  // Generate strings.ini.h file
  else if (po.enabled(strings)) {
    gen_strings_class(inputFilename);
  }
  // Generate command_ids.ini.h file
  else if (po.enabled(commandIds)) {
    gen_command_ids(inputFilename);
  }
  // Check all translation files (en.ini, es.ini, etc.)
  else if (po.enabled(widgetsDir) && po.enabled(stringsDir)) {
    check_strings(po.value_of(widgetsDir), po.value_of(stringsDir), po.value_of(guiFile));
  }
}

int main(int argc, const char* argv[])
{
  try {
    run(argc, argv);
    return 0;
  }
  catch (const std::exception& e) {
    std::cerr << e.what() << "\n";
    return 1;
  }
}
