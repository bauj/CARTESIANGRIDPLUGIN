// Copyright (C) 2026  VOXELPLUGIN authors
// LGPL 2.1 or later
#ifndef _VoxelPlugin_DEFS_HXX_
#define _VoxelPlugin_DEFS_HXX_

#ifdef WIN32
  #if defined VOXELPLUGIN_EXPORTS || defined VoxelEngine_EXPORTS || defined VoxelCore_EXPORTS
    #define VOXELPLUGIN_EXPORT __declspec( dllexport )
  #else
    #define VOXELPLUGIN_EXPORT __declspec( dllimport )
  #endif
#else
  #define VOXELPLUGIN_EXPORT
#endif

#endif
