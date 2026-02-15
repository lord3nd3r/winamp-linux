#pragma once

// Build type definitions for Winamp Linux port
// Used to control debug assertions and feature enablement

#ifdef NDEBUG
  // Release build
  #undef _DEBUG
  #undef INTERNAL
  #undef BETA
#else
  // Debug build
  #define _DEBUG 1
  //#define INTERNAL 1
  //#define BETA 1
#endif
