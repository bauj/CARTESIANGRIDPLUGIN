// Copyright (C) 2026  CARTESIANGRIDPLUGIN authors
// LGPL 2.1 or later
#ifndef _CartesianGridPlugin_GridText_HXX_
#define _CartesianGridPlugin_GridText_HXX_

#include <charconv>
#include <cmath>
#include <string>
#include <vector>

/*!
 * \brief Numbers as text in the GUI and the Python dump: always a decimal point, whatever
 *        the locale.
 */
namespace CartesianGridPlugin_GridText
{
  inline std::string Trim(const std::string& s)
  {
    const std::size_t b = s.find_first_not_of(" \t");
    if (b == std::string::npos)
      return std::string();
    return s.substr(b, s.find_last_not_of(" \t") - b + 1);
  }

  //! whole string is one finite number, locale independent
  inline bool ParseNumber(const std::string& s, double& v)
  {
    const char* first = s.c_str();
    const char* last = first + s.size();
    if (first != last && *first == '+') // from_chars refuses a leading '+'
      ++first;
    const std::from_chars_result r = std::from_chars(first, last, v);
    return first != last && r.ec == std::errc() && r.ptr == last && std::isfinite(v);
  }

  //! shortest text reading back exactly, locale independent
  inline std::string FormatNumber(double v)
  {
    char buf[32];
    const std::to_chars_result r = std::to_chars(buf, buf + sizeof(buf), v);
    return std::string(buf, r.ptr);
  }

  //! finite numbers separated by spaces, tabs, new lines, commas or semicolons (a column or
  //! row copied from a text file or a spreadsheet), order kept; false with theError on a bad one
  inline bool ParseNumbers(const std::string& theText, std::vector<double>& theValues,
                           std::string& theError)
  {
    theValues.clear();
    std::string field;
    const std::string text = theText + ' ';
    for (char ch : text)
    {
      if (ch != ' ' && ch != ',' && ch != ';' && ch != '\t' && ch != '\n' && ch != '\r')
      {
        field += ch;
        continue;
      }
      if (field.empty())
        continue;
      double v;
      if (!ParseNumber(field, v))
      {
        theError = "Bad number: " + field;
        return false;
      }
      theValues.push_back(v);
      field.clear();
    }
    return true;
  }
}

#endif
