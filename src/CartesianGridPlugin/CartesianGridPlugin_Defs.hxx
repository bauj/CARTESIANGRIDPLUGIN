// Copyright (C) 2026  CARTESIANGRIDPLUGIN authors
// LGPL 2.1 or later
#ifndef _CartesianGridPlugin_DEFS_HXX_
#define _CartesianGridPlugin_DEFS_HXX_

#ifdef WIN32
  #if defined CARTESIANGRIDPLUGIN_EXPORTS || defined CartesianGridEngine_EXPORTS || defined CartesianGridCore_EXPORTS
    #define CARTESIANGRIDPLUGIN_EXPORT __declspec( dllexport )
  #else
    #define CARTESIANGRIDPLUGIN_EXPORT __declspec( dllimport )
  #endif
#else
  #define CARTESIANGRIDPLUGIN_EXPORT
#endif

#endif
