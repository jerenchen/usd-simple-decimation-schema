#include <pxr/pxr.h>
#include <pxr/usdImaging/usdImaging/meshAdapter.h>
#include <pxr/imaging/hd/meshUtil.h>

#include <igl/qslim.h>

#include <algorithm>

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PRIVATE_TOKENS(
  TriMeshTokens,
  (maxTriangles)
  (triangleIndices)
  (srcPointIndices)
);

class UsdTriMeshAdapter : public UsdImagingMeshAdapter
{
public:
  using BaseAdapter = UsdImagingMeshAdapter;

  UsdTriMeshAdapter() : BaseAdapter(){}
  virtual ~UsdTriMeshAdapter() = default;

  VtValue GetTopology(UsdPrim const& prim, SdfPath const& cachePath, UsdTimeCode time) const override
  {
    auto topology = UsdImagingMeshAdapter::GetTopology(prim, cachePath, time);
    const auto& topo = topology.Get<HdMeshTopology>();

    auto attr = prim.GetAttribute(TriMeshTokens->triangleIndices);
    bool hasTime = false;
    double lower, upper;
    attr.GetBracketingTimeSamples(time.GetValue(), &lower, &upper, &hasTime);
    if (!hasTime)
    {
      VtVec3iArray srcTriIndices;
      VtIntArray primitiveParams;
      HdMeshUtil meshUtil(&topo, prim.GetPath());
      meshUtil.ComputeTriangleIndices(&srcTriIndices, &primitiveParams);

      VtIntArray lowTriIndices, srcPntIndices;
      {
        Eigen::MatrixXi F(srcTriIndices.size(), 3);
        size_t i = 0;
        for (const auto& T_i : srcTriIndices)
          F.row(i++) << T_i[0], T_i[1], T_i[2];

        const auto points = _Get<VtVec3fArray>(prim, UsdGeomTokens->points, time);
        Eigen::MatrixXd V(points.size(), 3);
        i = 0;
        for (const auto& p : points)
          V.row(i++) << double(p[0]), double(p[1]), double(p[2]);

        // QSlim mesh decimation
        const auto max_m = _Get<int>(prim, TriMeshTokens->maxTriangles, time);
        Eigen::VectorXi I, J;
        igl::qslim(V, F, max_m, V, F, J, I);

        const Eigen::MatrixXi& F_t = F.transpose();
        lowTriIndices = VtIntArray(F_t.data(), F_t.data() + F_t.size());
        srcPntIndices = VtIntArray(I.data(), I.data() + I.size());
      }

      attr.Set(lowTriIndices, time);
      prim.GetAttribute(TriMeshTokens->srcPointIndices).Set(srcPntIndices, time);
    }

    const auto& triIndices = _Get<VtIntArray>(prim, TriMeshTokens->triangleIndices, time);

    HdMeshTopology meshTopo(
      topo.GetScheme(),
      topo.GetOrientation(),
      VtIntArray(triIndices.size()/3, 3),
      triIndices,
      topo.GetHoleIndices()
    );

    auto geomSubsets = meshTopo.GetGeomSubsets();
    if (!geomSubsets.empty())
      meshTopo.SetGeomSubsets(geomSubsets);

    return VtValue(meshTopo);
  }

  VtValue GetPoints(UsdPrim const& prim, UsdTimeCode time) const override
  { 
    auto points = _Get<VtVec3fArray>(prim, UsdGeomTokens->points, time);

    const auto& indices = _Get<VtIntArray>(prim, TriMeshTokens->srcPointIndices, time);
    // Indexing into source points of the original mesh using the erase-remove idiom
    auto it = indices.begin();
    const GfVec3f* p0 = &points[0];
    points.erase(
      std::remove_if(
        points.begin(),
        points.end(),
        [&](const GfVec3f& p)->bool
        {
          if (it == indices.end() || (&p - p0) != *it) return true;
          it++;
          return false;
        }
      ),
      points.end()
    );

    return VtValue(points);
  }
};

TF_REGISTRY_FUNCTION(TfType)
{
  using Adapter = UsdTriMeshAdapter;
  TfType t = TfType::Define<Adapter, TfType::Bases<Adapter::BaseAdapter> >();
  t.SetFactory< UsdImagingPrimAdapterFactory<Adapter> >();
}

PXR_NAMESPACE_CLOSE_SCOPE