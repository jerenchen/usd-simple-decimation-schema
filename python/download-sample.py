import os
import zipfile
from io import BytesIO

import numpy
import requests
import pyassimp
from pxr import Usd, UsdGeom

import logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("BUILD-DEMO")


def download_example(url, dest_path='demo'):
  logger.info(f'Downloading example from "{url}"...')

  response = requests.get(url)
  response.raise_for_status()

  with zipfile.ZipFile(BytesIO(response.content)) as zfile:
    os.makedirs(dest_path, exist_ok=True)
    zfile.extractall(dest_path)
    print(f"Extracted to {dest_path}")


def convert_objs_to_usd(usd_path, obj_path, frame_range=(1, 48)):

  stage = Usd.Stage.CreateNew(usd_path)
  
  start, end = frame_range
  stage.SetStartTimeCode(start)
  stage.SetEndTimeCode(end)

  root = UsdGeom.Scope.Define(stage, '/horse')
  stage.SetDefaultPrim(root.GetPrim())

  for frame in range(start, end+1):
    logger.info(f'Processing frame: {frame}...')
    with pyassimp.load(
      obj_path % frame,
      processing= \
        pyassimp.postprocess.aiProcess_Triangulate | \
        pyassimp.postprocess.aiProcess_JoinIdenticalVertices | \
        pyassimp.postprocess.aiProcess_FindInvalidData | \
        pyassimp.postprocess.aiProcess_OptimizeMeshes | \
        pyassimp.postprocess.aiProcess_ValidateDataStructure
    ) as S:
      for M in S.meshes:
        logger.info(f'Processing mesh: "{M.name}"...')
        mesh_path = f'{root.GetPath()}/{M.name}'
        mesh_prim = stage.GetPrimAtPath(mesh_path)
        if mesh_prim.IsValid():
          mesh = UsdGeom.Mesh(mesh_prim)
        else:
          mesh = UsdGeom.Mesh.Define(stage, mesh_path)
          mesh.GetFaceVertexCountsAttr().Set([3] * len(M.faces))
          mesh.GetFaceVertexIndicesAttr().Set(numpy.array(M.faces).ravel())

        points_attr = mesh.GetPointsAttr()
        points_attr.Set(M.vertices, time=frame)

  stage.GetRootLayer().Save()
  logger.info(f'Saved USD scene: "{usd_path}"')


if __name__ == "__main__":
  url = "https://people.csail.mit.edu/sumner/research/deftransfer/data/horse-gallop.zip"
  download_example(url, 'demo')
  convert_objs_to_usd(
    usd_path='demo/horse-gallop.usd',
    obj_path='demo/horse-gallop/horse-gallop-%02d.obj'
  )