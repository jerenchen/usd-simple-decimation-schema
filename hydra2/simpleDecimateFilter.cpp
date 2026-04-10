#include <pxr/imaging/hd/sceneIndexPluginRegistry.h>
#include <pxr/imaging/hd/sceneIndexPlugin.h>
#include <pxr/imaging/hd/filteringSceneIndex.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/containerDataSourceEditor.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/meshTopology.h>
#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/meshUtil.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/usd/sdf/path.h>

#include <igl/qslim.h>

#include <algorithm>
#include <unordered_map>

#include "simpleDecimate.h"

PXR_NAMESPACE_OPEN_SCOPE

// A map to store indices mapping decimated points into the source mesh by prim paths
using _PointIndicesMap = std::unordered_map<SdfPath, VtIntArray, SdfPath::Hash>;

/*
  A custom data source handling decimation in cases when mesh data is requested:
  1. Topology (HdMeshSchema): Reconstruct mesh for triangulation and then decimation,
      as well as storing the mapping of the indices to the source mesh points.
  2. Points (HdPrimvars): Remap decimated points using mapped source point indices.

  NOTE: Other primvars such as face vertex colors are not handled in this example,
    but they can be remapped using a similar way that source points are reindexed.
*/
class SimpleDecimateDataSource : public HdContainerDataSource {
public:
  HD_DECLARE_DATASOURCE(SimpleDecimateDataSource);

  TfTokenVector GetNames() override {return _input->GetNames();}

  HdDataSourceBaseHandle Get(const TfToken &name) override {

    if (name == HdMeshSchemaTokens->mesh) {

      const auto meshSchema = HdMeshSchema::GetFromParent(_input);
      if (meshSchema.IsDefined()) {
        const auto topoSchema = meshSchema.GetTopology();
        const auto topoOrient = topoSchema.GetOrientation()->GetValue(0.0).Get<TfToken>();
        const auto topoHoles = topoSchema.GetHoleIndices()->GetValue(0.0).Get<VtIntArray>();

        // construct the topology from data source and triangulate the mesh
        VtVec3iArray srcTriIndices;
        VtIntArray primitiveParams;
        {
          HdMeshTopology meshTopo(
            topoSchema.GetSchemaToken(),
            topoOrient,
            topoSchema.GetFaceVertexCounts()->GetValue(0.0).Get<VtIntArray>(),
            topoSchema.GetFaceVertexIndices()->GetValue(0.0).Get<VtIntArray>(),
            topoHoles
          );
          HdMeshUtil meshUtil(&meshTopo, _path);
          meshUtil.ComputeTriangleIndices(&srcTriIndices, &primitiveParams);
        }

        // points are required for decimation
        VtVec3fArray points;
        const auto primvarsSchema = HdPrimvarsSchema::GetFromParent(_input);
        if (primvarsSchema.IsDefined()) {
          const auto primvarSchema = primvarsSchema.GetPrimvar(HdPrimvarsSchemaTokens->points);
          points = primvarSchema.GetPrimvarValue()->GetValue(0).Get<VtVec3fArray>();
        }

        // get decimate API params
        int max_m = 5000;
        bool block = true;
        if (const auto decimateSource = _input->Get(_Tokens->simpleDecimate)) {
          if (const auto decimateData = HdContainerDataSource::Cast(decimateSource)) {
            const auto maxSource = decimateData->Get(_Tokens->maxTriangles);
            if (const auto maxData = HdSampledDataSource::Cast(maxSource))
              max_m = maxData->GetValue(0).Get<int>();
            const auto blockSource = decimateData->Get(_Tokens->blockIntersections);
            if (const auto blockData = HdSampledDataSource::Cast(blockSource))
              block = blockData->GetValue(0).Get<bool>();
          }
        }

        // decimation begins here...
        VtIntArray triIndices;
        {
          Eigen::MatrixXi F(srcTriIndices.size(), 3);
          size_t i = 0;
          for (const auto& T_i : srcTriIndices)
            F.row(i++) << T_i[0], T_i[1], T_i[2];

          Eigen::MatrixXd V(points.size(), 3);
          i = 0;
          for (const auto& p : points)
            V.row(i++) << double(p[0]), double(p[1]), double(p[2]);

          // QSlim mesh decimation
          Eigen::VectorXi I, J;
          igl::qslim(V, F, max_m, block, V, F, J, I);

          const Eigen::MatrixXi& F_t = F.transpose();
          triIndices = VtIntArray(F_t.data(), F_t.data() + F_t.size());

          // storing the indices into source mesh points for this prim
          auto& srcPntIndices = (*_srcPntIndicesMap)[_path];
          srcPntIndices = VtIntArray(I.data(), I.data() + I.size());
        }
        VtIntArray triCounts = VtIntArray(triIndices.size()/3, 3);

        // return the mesh data with the decimated topology
        const auto meshDataSource = HdContainerDataSource::Cast(_input->Get(name));
        HdContainerDataSourceEditor edit(meshDataSource);
        edit.Set(
          HdDataSourceLocator(HdMeshSchemaTokens->topology),
          HdMeshTopologySchema::BuildRetained(
            HdRetainedTypedSampledDataSource<VtIntArray>::New(triCounts),
            HdRetainedTypedSampledDataSource<VtIntArray>::New(triIndices),
            HdRetainedTypedSampledDataSource<VtIntArray>::New(topoHoles),
            HdRetainedTypedSampledDataSource<TfToken>::New(topoOrient)
          )
        );
        return edit.Finish();
      }
    } else if (name == HdPrimvarsSchemaTokens->primvars) {

      VtIntArray indicesMap = (*_srcPntIndicesMap)[_path];
      if (!indicesMap.size())
        return _input->Get(name);

      const auto primvarsSchema = HdPrimvarsSchema::GetFromParent(_input);
      if (primvarsSchema.IsDefined()) {
        const auto primvarSchema = primvarsSchema.GetPrimvar(HdPrimvarsSchemaTokens->points);
        auto points = primvarSchema.GetPrimvarValue()->GetValue(0).Get<VtVec3fArray>();

        // Index into source mesh using the erase-remove idiom for decimated points
        auto it = indicesMap.begin();
        const GfVec3f* p0 = &points[0];
        points.erase(
          std::remove_if(
            points.begin(),
            points.end(),
            [&](const GfVec3f& p)->bool
            {
              if (it == indicesMap.end() || (&p - p0) != *it) return true;
              it++;
              return false;
            }
          ),
          points.end()
        );

        // return the primvar data with the reduced points
        HdContainerDataSourceEditor edit(HdContainerDataSource::Cast(_input->Get(name)));
        edit.Set(
          HdDataSourceLocator(HdTokens->points),
          HdPrimvarSchema::BuildRetained(
            HdRetainedTypedSampledDataSource<VtVec3fArray>::New(points),
            nullptr,
            nullptr,
            primvarSchema.GetInterpolation(),
            primvarSchema.GetRole(),
            primvarSchema.GetColorSpace(),
            nullptr
          )
        );
        return edit.Finish();
      }
    }

    return _input->Get(name);
  }

private:
  SimpleDecimateDataSource(
    const SdfPath& path,
    const HdContainerDataSourceHandle& input,
    _PointIndicesMap& indicesMap
  ) : _path(path), _input(input), _srcPntIndicesMap(&indicesMap) {}

private:
  SdfPath _path;
  HdContainerDataSourceHandle _input;
  _PointIndicesMap* _srcPntIndicesMap;
};

TF_DECLARE_REF_PTRS(SimpleDecimateSceneIndex);
/*
  A filtering scene index to observe meshes with decimate API to further
    process them for decimation while maintaining the indices mapping to
    source mesh points through the session.
*/
class SimpleDecimateSceneIndex : public HdSingleInputFilteringSceneIndexBase 
{
  mutable _PointIndicesMap _srcPntIndicesByPrims;

public:
  static SimpleDecimateSceneIndexRefPtr
  New(const HdSceneIndexBaseRefPtr &inputSceneIndex) {
    return TfCreateRefPtr(new SimpleDecimateSceneIndex(inputSceneIndex));
  }

  HdSceneIndexPrim GetPrim(const SdfPath &primPath) const override {
    HdSceneIndexPrim prim = _GetInputSceneIndex()->GetPrim(primPath);
    if (prim.primType == HdPrimTypeTokens->mesh) {
        prim.dataSource = SimpleDecimateDataSource::New(
          primPath, prim.dataSource, _srcPntIndicesByPrims
        );
    }
    return prim;
  }

  SdfPathVector GetChildPrimPaths(const SdfPath &primPath) const override {
    return _GetInputSceneIndex()->GetChildPrimPaths(primPath);
  }

protected:
  SimpleDecimateSceneIndex(const HdSceneIndexBaseRefPtr &inputSceneIndex)
    : HdSingleInputFilteringSceneIndexBase(inputSceneIndex) {}

protected:
  void _PrimsAdded(
    const HdSceneIndexBase& sender,
    const HdSceneIndexObserver::AddedPrimEntries& entries
  ) override {
    _SendPrimsAdded(entries);
  }

  void _PrimsRemoved(
    const HdSceneIndexBase& sender,
    const HdSceneIndexObserver::RemovedPrimEntries& entries
  ) override {
    _SendPrimsRemoved(entries);
  }

  void _PrimsDirtied(
    const HdSceneIndexBase& sender,
    const HdSceneIndexObserver::DirtiedPrimEntries& entries
  ) override {
    _SendPrimsDirtied(entries);
  }
};

// plugin to add the decimate filter to Hydra
class SimpleDecimateSceneIndexPlugin : public HdSceneIndexPlugin {
public:
  SimpleDecimateSceneIndexPlugin() = default;

protected:
  HdSceneIndexBaseRefPtr _AppendSceneIndex(
    const HdSceneIndexBaseRefPtr &inputScene,
    const HdContainerDataSourceHandle &inputArgs
  ) override {
    return SimpleDecimateSceneIndex::New(inputScene);
  }
};

TF_REGISTRY_FUNCTION(TfType) {
  HdSceneIndexPluginRegistry::Define<SimpleDecimateSceneIndexPlugin>();
}

TF_REGISTRY_FUNCTION(HdSceneIndexPlugin)
{
  const HdSceneIndexPluginRegistry::InsertionPhase insertionPhase = 1000;
 
  HdSceneIndexPluginRegistry::GetInstance().RegisterSceneIndexForRenderer(
    "GL", // or "" for any
    TfToken("SimpleDecimateSceneIndexPlugin"),
    nullptr,
    insertionPhase,
    HdSceneIndexPluginRegistry::InsertionOrderAtEnd
  );
}

PXR_NAMESPACE_CLOSE_SCOPE