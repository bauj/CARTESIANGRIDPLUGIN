// Copyright (C) 2026  VOXELPLUGIN authors
// LGPL 2.1 or later
#ifndef _VoxelPlugin_GridText_HXX_
#define _VoxelPlugin_GridText_HXX_

#include <charconv>
#include <cmath>
#include <string>
#include <vector>

/*!
 * \brief Text format of the grid modes in the GUI.
 *        Coordinates: numbers separated by spaces and/or commas, "0 1 2.5 10".
 *        Spacing: functions and internal points alternating, separated by ';', "0.2 ; 0.5 ; 0.2+t".
 *        Numbers always use a decimal point, whatever the locale.
 */
namespace VoxelPlugin_GridText
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

  //! at least 2 finite numbers, order kept
  inline bool ParseCoordinates(const std::string& theText, std::vector<double>& theCoords,
                               std::string& theError)
  {
    theCoords.clear();
    std::string field;
    const std::string text = theText + ' ';
    for (char ch : text)
    {
      if (ch != ' ' && ch != ',' && ch != '\t')
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
      theCoords.push_back(v);
      field.clear();
    }
    if (theCoords.size() < 2)
    {
      theError = "At least 2 coordinates are needed";
      return false;
    }
    return true;
  }

  inline std::string FormatCoordinates(const std::vector<double>& theCoords)
  {
    std::string text;
    for (std::size_t i = 0; i < theCoords.size(); ++i)
      text += (i ? " " : "") + FormatNumber(theCoords[i]);
    return text;
  }

  //! odd number of non-empty fields: function ; point ; function ...
  inline bool ParseSpacing(const std::string& theText, std::vector<std::string>& theFunctions,
                           std::vector<double>& thePoints, std::string& theError)
  {
    theFunctions.clear();
    thePoints.clear();
    std::vector<std::string> fields;
    std::size_t start = 0;
    while (true)
    {
      const std::size_t sep = theText.find(';', start);
      fields.push_back(Trim(theText.substr(start, sep == std::string::npos ? std::string::npos : sep - start)));
      if (sep == std::string::npos)
        break;
      start = sep + 1;
    }
    for (const std::string& f : fields)
      if (f.empty())
      {
        theError = "Empty field in spacing";
        return false;
      }
    if (fields.size() % 2 == 0)
    {
      theError = "Spacing must alternate functions and internal points: f1 ; t1 ; f2";
      return false;
    }
    for (std::size_t i = 0; i < fields.size(); ++i)
    {
      if (i % 2 == 0)
      {
        theFunctions.push_back(fields[i]);
        continue;
      }
      double t;
      if (!ParseNumber(fields[i], t))
      {
        theError = "Bad internal point: " + fields[i];
        return false;
      }
      thePoints.push_back(t);
    }
    return true;
  }

  inline std::string FormatSpacing(const std::vector<std::string>& theFunctions,
                                   const std::vector<double>&      thePoints)
  {
    std::string text;
    for (std::size_t i = 0; i < theFunctions.size(); ++i)
    {
      if (i)
        text += " ; " + (i - 1 < thePoints.size() ? FormatNumber(thePoints[i - 1]) : std::string("?")) + " ; ";
      text += theFunctions[i];
    }
    return text;
  }
}

#endif
