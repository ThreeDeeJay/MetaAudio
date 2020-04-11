#include <metahook.h>

#include "snd_local.h"
#include "Utilities/VectorUtils.hpp"
#include "Loaders/SteamAudioMapMeshLoader.hpp"

namespace MetaAudio
{
  constexpr const float EPSILON = 0.000001f;

  static IPLhandle context{ nullptr };

  bool VectorEquals(const alure::Vector3& left, const alure::Vector3& right)
  {
    return left[0] == right[0] && left[1] == right[1] && left[2] == right[2];
  }

  bool VectorApproximatelyEquals(const alure::Vector3& left, const alure::Vector3& right)
  {
    return (left[0] - right[0]) < EPSILON && (left[1] - right[1]) < EPSILON && (left[2] - right[2]) < EPSILON;
  }

  SteamAudioMapMeshLoader::SteamAudioMapMeshLoader(IPLhandle sa_context, IPLSimulationSettings simulSettings) : sa_simul_settings(simulSettings), sa_context(sa_context)
  {
    current_env = std::make_tuple("", nullptr);
  }

  alure::Vector3 SteamAudioMapMeshLoader::Normalize(const alure::Vector3& vector)
  {
    float length = vector.getLength();
    if (length == 0)
    {
      return alure::Vector3(0, 0, 1);
    }
    length = 1 / length;
    return alure::Vector3(vector[0] * length, vector[1] * length, vector[2] * length);
  }

  float SteamAudioMapMeshLoader::DotProduct(const alure::Vector3& left, const alure::Vector3& right)
  {
    return left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
  }

  void SteamAudioMapMeshLoader::update()
  {
    // check map
    bool paused = false;
    {
      cl_entity_s* map = gEngfuncs.GetEntityByIndex(0);
      if (map == nullptr
        || map->model == nullptr
        || map->model->needload
        || gEngfuncs.pfnGetLevelName() == nullptr
        || _stricmp(gEngfuncs.pfnGetLevelName(), map->model->name) != 0)
      {
        paused = true;
      }
      else
      {
        auto mapModel = map->model;
        if (std::get<0>(current_env) == mapModel->name)
        {
          return;
        }
        else
        {
          auto search = map_cache.find(mapModel->name);
          if (search != map_cache.end())
          {
            current_env = std::make_tuple(search->first, search->second->Env());
            return;
          }
        }

        std::vector<IPLTriangle> triangles;
        std::vector<alure::Vector3> triangulatedVerts;

        for (int i = 0; i < mapModel->nummodelsurfaces; ++i)
        {
          auto surface = mapModel->surfaces[mapModel->firstmodelsurface + i];
          glpoly_t* poly = surface.polys;
          std::vector<alure::Vector3> surfaceVerts;
          while (poly)
          {
            if (poly->numverts <= 0)
              continue;

            for (int j = 0; j < poly->numverts; j++)
            {
              surfaceVerts.emplace_back(MetaAudio::AL_UnpackVector(poly->verts[j]));
            }

            poly = poly->next;

            // escape rings
            if (poly == surface.polys)
              break;
          }

          // triangulation

          // Get rid of duplicate vertices
          surfaceVerts.erase(std::unique(surfaceVerts.begin(), surfaceVerts.end(), VectorEquals), surfaceVerts.end());

          // Skip invalid face
          if (surfaceVerts.size() < 3)
          {
            continue;
          }

          // Triangulate
          alure::Vector3 origin{ 0,0,0 };
          alure::Vector<alure::Vector3> newVerts;
          { // remove colinear
            for (size_t i = 0; i < surfaceVerts.size(); ++i)
            {
              alure::Vector3 vertexBefore = origin + surfaceVerts[(i > 0) ? (i - 1) : (surfaceVerts.size() - 1)];
              alure::Vector3 vertex = origin + surfaceVerts[i];
              alure::Vector3 vertexAfter = origin + surfaceVerts[(i < (surfaceVerts.size() - 1)) ? (i + 1) : 0];

              alure::Vector3 v1 = Normalize(vertexBefore - vertex);
              alure::Vector3 v2 = Normalize(vertexAfter - vertex);

              float vertDot = DotProduct(v1, v2);
              if (std::fabs(vertDot + 1.f) < EPSILON)
              {
                // colinear, drop it
              }
              else
              {
                newVerts.emplace_back(vertex);
              }
            }
          }

          // Skip invalid face, it is just a line
          if (newVerts.size() < 3)
          {
            continue;
          }

          { // generate indices
            int indexoffset = triangulatedVerts.size();
            auto actualNormal = MetaAudio::AL_CopyVector(surface.plane->normal);

            for (size_t i = 0; i < newVerts.size() - 2; ++i)
            {
              auto& triangle = triangles.emplace_back();

              triangle.indices[0] = indexoffset + i + 2;
              triangle.indices[1] = indexoffset + i + 1;
              triangle.indices[2] = indexoffset;
            }

            // Add vertices to final array
            triangulatedVerts.insert(triangulatedVerts.end(), newVerts.begin(), newVerts.end());
          }
        }

        auto data = MapData{ triangulatedVerts, triangles };

        IPLerror error;
        IPLhandle scene = nullptr;
        error = iplCreateScene(sa_context, nullptr, sa_simul_settings, 1, materials.data(), nullptr, nullptr, nullptr, nullptr, nullptr, &scene);
        if (error)
        {
          throw std::exception("Error creating scene: " + error);
        }

        IPLhandle staticmesh = nullptr;
        error = iplCreateStaticMesh(scene, data.vertices.size() * 3, data.triangles.size(), reinterpret_cast<IPLVector3*>(data.vertices.data()), data.triangles.data(), std::vector<int>(data.triangles.size(), 0).data(), &staticmesh);

        IPLhandle env = nullptr;
        error = iplCreateEnvironment(sa_context, nullptr, sa_simul_settings, scene, nullptr, &env);

        map_cache[mapModel->name] = std::make_shared<CacheItem>(env, scene, staticmesh);
        current_env = std::make_tuple(mapModel->name, map_cache[mapModel->name]->Env());
      }
    }
  }

  IPLhandle SteamAudioMapMeshLoader::get_current_environment()
  {
    return std::get<1>(current_env);
  }

  void SteamAudioMapMeshLoader::PurgeCache()
  {
    current_env = std::make_tuple("", nullptr);
    map_cache.clear();
  }
}