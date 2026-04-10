#pragma once

#include <pxr/imaging/hd/dataSourceTypeDefs.h>
#include <pxr/base/tf/token.h>

PXR_NAMESPACE_OPEN_SCOPE

// Private tokens for locating API schema attr data
TF_DEFINE_PRIVATE_TOKENS(
  _Tokens,
  (simpleDecimate)
  ((maxTriangles, "simpleDecimate:maxTriangles"))
  ((blockIntersections, "simpleDecimate:blockIntersections"))
);

PXR_NAMESPACE_CLOSE_SCOPE