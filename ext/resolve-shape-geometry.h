
#pragma once

#include "../core/Shape.h"
#include <skia/core/SkPath.h>

#ifdef MSDFGEN_USE_SKIA

namespace msdfgen {

/// Resolves any intersections within the shape by subdividing its contours using the Skia library and makes sure its contours have a consistent winding.
bool resolveShapeGeometry(Shape &shape);

void shapeFromSkiaPath(Shape& shape, const SkPath& skPath);

}

#endif
