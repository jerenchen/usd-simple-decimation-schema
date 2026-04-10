#include <pxr/usdImaging/usdImaging/apiSchemaAdapter.h>
#include <pxr/usdImaging/usdImaging/adapterRegistry.h>
#include <pxr/usdImaging/usdImaging/dataSourcePrim.h>
#include <pxr/imaging/hd/dataSourceTypeDefs.h>
#include <pxr/imaging/hd/retainedDataSource.h>

#include <iostream>

#include "simpleDecimate.h"

PXR_NAMESPACE_OPEN_SCOPE

/* 
  A scene index adapter forwarding decimate API schema attr data
    from USD stage to Hydra
*/
class SimpleDecimateAdapter : public UsdImagingAPISchemaAdapter {
public:
  using BaseAdapter = UsdImagingAPISchemaAdapter;

  HdContainerDataSourceHandle GetImagingSubprimData(
    UsdPrim const& prim,
    TfToken const& subprim,
    TfToken const& appliedInstanceName,
    const UsdImagingDataSourceStageGlobals &stageGlobals
  ) override {
    if (subprim.IsEmpty()) {
      int max_m = 5000;
      if (auto attr = prim.GetAttribute(_Tokens->maxTriangles))
        attr.Get(&max_m);
      bool block = true;
      if (auto attr = prim.GetAttribute(_Tokens->blockIntersections))
        attr.Get(&block);

      return HdRetainedContainerDataSource::New(
        _Tokens->simpleDecimate,
        HdRetainedContainerDataSource::New(
          _Tokens->maxTriangles,
          HdRetainedTypedSampledDataSource<int>::New(max_m),
          _Tokens->blockIntersections,
          HdRetainedTypedSampledDataSource<bool>::New(block)
        )
      );
    }
    return nullptr;
  }
};

TF_REGISTRY_FUNCTION(TfType)
{
  using Adapter = SimpleDecimateAdapter;
  TfType t = TfType::Define<Adapter, TfType::Bases<Adapter::BaseAdapter> >();
  t.SetFactory< UsdImagingAPISchemaAdapterFactory<Adapter> >();
}

PXR_NAMESPACE_CLOSE_SCOPE