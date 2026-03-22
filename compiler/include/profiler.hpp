#pragma once

//! # Tracy Profiler Integration
//!
//! Provides zero-cost profiling macros that compile out when TML_PROFILE is not defined.
//!
//! Usage in C++ compiler code:
//!   TML_ZONE("lexer");           // Named zone (appears in Tracy timeline)
//!   TML_ZONE_TEXT("parsing foo.tml");  // Zone with dynamic text
//!   TML_FRAME_MARK;              // Frame boundary (for per-frame analysis)
//!   TML_PLOT("cache_hits", n);   // Plot a value over time
//!   TML_MESSAGE("hello");        // Log message to Tracy timeline
//!   TML_ALLOC(ptr, size);        // Track allocation
//!   TML_FREE(ptr);               // Track deallocation

#ifdef TML_PROFILE

#include "tracy/Tracy.hpp"

// Zone macros — create a scoped zone that auto-closes at scope exit
#define TML_ZONE(name) ZoneNamedN(___tracy_zone, name, true)
#define TML_ZONE_SCOPED ZoneScoped
#define TML_ZONE_TEXT(text)                                                                        \
    ZoneScoped;                                                                                    \
    ZoneText(text, strlen(text))

// Frame and messaging
#define TML_FRAME_MARK FrameMark
#define TML_MESSAGE(text) TracyMessage(text, strlen(text))
#define TML_MESSAGE_L(text) TracyMessageL(text)

// Plots (time-series data in Tracy viewer)
#define TML_PLOT(name, value) TracyPlot(name, value)
#define TML_PLOT_CONFIG(name, type, step, fill, color)                                             \
    TracyPlotConfig(name, type, step, fill, color)

// Memory tracking
#define TML_ALLOC(ptr, size) TracyAlloc(ptr, size)
#define TML_FREE(ptr) TracyFree(ptr)

// Lock tracking
#define TML_LOCKABLE(type, var) tracy::Lockable<type> var
#define TML_LOCK_MARK(var) LockMark(var)

#else // TML_PROFILE not defined — everything compiles to nothing

#define TML_ZONE(name) (void)0
#define TML_ZONE_SCOPED (void)0
#define TML_ZONE_TEXT(text) (void)0
#define TML_FRAME_MARK (void)0
#define TML_MESSAGE(text) (void)0
#define TML_MESSAGE_L(text) (void)0
#define TML_PLOT(name, value) (void)0
#define TML_PLOT_CONFIG(name, type, step, fill, color) (void)0
#define TML_ALLOC(ptr, size) (void)0
#define TML_FREE(ptr) (void)0
#define TML_LOCKABLE(type, var) type var
#define TML_LOCK_MARK(var) (void)0

#endif // TML_PROFILE
