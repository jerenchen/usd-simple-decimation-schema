#include <pxr/pxr.h>
#include <pxr/usdImaging/usdImaging/meshAdapter.h>
#include <pxr/imaging/hd/meshUtil.h>

#include <igl/qslim.h>
#include <igl/remove_unreferenced.h>

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
      Eigen::MatrixXi F;
      Eigen::VectorXi I;
      {
        // Triangulation
        HdMeshUtil meshUtil(&topo, prim.GetPath());
        VtVec3iArray triangleIndices;
        VtIntArray primitiveParams;
        meshUtil.ComputeTriangleIndices(&triangleIndices, &primitiveParams);

        // Decimation via libigl/qslim 
        size_t i = 0;
        F.resize(triangleIndices.size(), 3);
        for (const auto& Ti : triangleIndices)
          F.row(i++) << Ti[0], Ti[1], Ti[2];

        const auto points = _Get<VtVec3fArray>(prim, UsdGeomTokens->points, time);
        Eigen::MatrixXd V(points.size(), 3);
        i = 0;
        for (const auto& p : points)
          V.row(i++) << double(p[0]), double(p[1]), double(p[2]);

        const auto max_m = _Get<int>(prim, TriMeshTokens->maxTriangles, time);
        Eigen::VectorXi J;
        igl::qslim(V, F, max_m, V, F, J, I);
      }

      VtIntArray triFaceIndices(F.rows() * 3);
      size_t idx = 0;
      for (size_t i = 0; i < F.rows(); ++i)
        for (size_t j = 0; j < 3; ++j)
          triFaceIndices[idx++] = F(i, j);
      attr.Set(triFaceIndices, time);

      prim.GetAttribute(
        TriMeshTokens->srcPointIndices
      ).Set(VtIntArray(I.data(), I.data() + I.size()), time);
    }

    const auto& triFaceIndices = _Get<VtIntArray>(prim, TriMeshTokens->triangleIndices, time);

    HdMeshTopology meshTopo(
      topo.GetScheme(),
      topo.GetOrientation(),
      VtIntArray(triFaceIndices.size()/3, 3),
      triFaceIndices,
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