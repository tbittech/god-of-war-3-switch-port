/* Puente para glslang 11.0.0 (la fuente de kof2002um-nx): las versiones nuevas traen
 * glslang/Public/ResourceLimits.h con GetDefaultResources(); aca sale de StandAlone. */
#pragma once
#include <StandAlone/ResourceLimits.h>
static inline const TBuiltInResource* GetDefaultResources() { return &glslang::DefaultTBuiltInResource; }
