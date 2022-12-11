#include <assimp/Exporter.hpp>
#include <assimp/Importer.hpp>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>

using namespace std;
typedef glm::vec3 Vertex;
struct Triangle {
    uint v0, v1, v2;
};

//////////////////////////////////////////
/// \brief The MeshOctree class
///
///

class Mesh {
public: // functions
    Mesh() = default;
    Mesh(const Mesh& other) = default;

public: // data
    std::vector<Vertex> m_vertices;
    std::vector<Triangle> m_triangles;
};

//////////////////////////////////////////
/// \brief The MeshOctree class
///
///

class MeshOctree {
    struct GridData { // types
        int vertexIndex;
        glm::vec3 pos;
    };

public: // functions
    MeshOctree() = default;
    inline glm::ivec3 toGridCell(const glm::vec3& pos) { return pos * gridScale; }
    void appendMesh(const Mesh& mesh)
    {
        int indexOffset = m_pointGrid.size();
        for (const auto& t : mesh.m_triangles) {
            const auto& vertices = mesh.m_vertices;
            glm::vec3 vertexPos[3] = { vertices[t.v0], vertices[t.v1], vertices[t.v2] };
            glm::ivec3 gridPos[3] = { toGridCell(vertexPos[0]), toGridCell(vertexPos[1]), toGridCell(vertexPos[2]) };

            if (gridPos[0] == gridPos[1] || gridPos[1] == gridPos[2] || gridPos[2] == gridPos[0]) // triangle too small
                continue;

            uint vertexIndices[3];
            for (int i = 0; i < 3; ++i) {
                const glm::ivec3& gPos = gridPos[i];
                const glm::vec3& realPos = vertexPos[i];

                const auto& foundCell = m_pointGrid.find(gPos);
                if (foundCell != m_pointGrid.end()) { // if found
                    const GridData& gridData = foundCell->second;
                    vertexIndices[i] = gridData.vertexIndex;
                } else {
                    m_pointGrid.insert({ gPos, { indexOffset, realPos } });
                    vertexIndices[i] = indexOffset;
                    indexOffset++;
                }
            }
            m_triangles.push_back({ vertexIndices[0], vertexIndices[1], vertexIndices[2] });
        }
    }
    Mesh getMesh() const
    {
        Mesh result;
        result.m_vertices.resize(m_pointGrid.size());
        for (const auto& v : m_pointGrid) {
            const glm::vec3& posFromGrid = gridSize * glm::vec3(v.first);
            const GridData& gridData = v.second;
            result.m_vertices[gridData.vertexIndex] = gridData.pos;
            //result.m_vertices[gridData.vertexIndex] = posFromGrid ;
        }
        result.m_triangles = m_triangles;
        return result;
    }

public: // data
    std::unordered_map<glm::ivec3, GridData> m_pointGrid;
    std::vector<Triangle> m_triangles;

private:
    const float gridScale = 10.f;
    const float gridSize = 1 / gridScale;
};

//////////////////////////////////////////
/// \brief The MeshOctree class
///
///

class MeshWelder {
public:
    bool import(const std::string& readPath, std::vector<Mesh>& outMeshes)
    {
        Assimp::Importer importer;
        const aiScene* importScene = importer.ReadFile(readPath, aiProcess_Triangulate | aiProcess_FlipUVs);
        if (!importScene || importScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !importScene->mRootNode) {
            std::cerr << "Failed to load file" << std::endl;
            return false;
        }

        outMeshes.clear();

        for (int meshIndex = 0; meshIndex < importScene->mNumMeshes; ++meshIndex) {
            aiMesh* assimpMesh = importScene->mMeshes[meshIndex];
            if (!assimpMesh)
                continue;

            outMeshes.emplace_back();
            Mesh& mesh = outMeshes.back();

            mesh.m_vertices.resize(assimpMesh->mNumVertices);
            for (int vertexIndex = 0; vertexIndex < assimpMesh->mNumVertices; ++vertexIndex) {
                const aiVector3D& pos = assimpMesh->mVertices[vertexIndex];
                mesh.m_vertices[vertexIndex] = { pos.x, pos.y, pos.z };
            }

            mesh.m_triangles.resize(assimpMesh->mNumFaces);
            for (int faceIndex = 0; faceIndex < assimpMesh->mNumFaces; ++faceIndex) {
                const aiFace& face = assimpMesh->mFaces[faceIndex];
                mesh.m_triangles[faceIndex] = { face.mIndices[0], face.mIndices[1], face.mIndices[2] };
            }
        }

        bool successRead = static_cast<bool>(outMeshes.size());
        return successRead;
    }

    Mesh weldMeshes(const std::vector<Mesh>& meshes)
    {
        MeshOctree result;
        for (const auto& mesh : meshes) {
            result.appendMesh(mesh);
            // append to finalMesh
        }
        return result.getMesh();
    }

    void exportMesh(const Mesh& finalMesh, const std::string& writePath)
    {
        aiMesh* exportMesh = new aiMesh;

        // copy vertices
        exportMesh->mNumVertices = finalMesh.m_vertices.size();
        exportMesh->mVertices = new aiVector3D[exportMesh->mNumVertices];
        for (int vertexIndex = 0; vertexIndex < exportMesh->mNumVertices; ++vertexIndex) {
            const glm::vec3& pos = finalMesh.m_vertices[vertexIndex];
            exportMesh->mVertices[vertexIndex] = { pos.x, pos.y, pos.z };
        }

        // copy triangles
        exportMesh->mNumFaces = finalMesh.m_triangles.size();
        exportMesh->mFaces = new aiFace[exportMesh->mNumFaces];
        for (int faceIndex = 0; faceIndex < exportMesh->mNumFaces; ++faceIndex) {
            const Triangle& triangle = finalMesh.m_triangles[faceIndex];
            exportMesh->mFaces[faceIndex].mNumIndices = 3;
            exportMesh->mFaces[faceIndex].mIndices = new uint[3];
            exportMesh->mFaces[faceIndex].mIndices[0] = triangle.v0;
            exportMesh->mFaces[faceIndex].mIndices[1] = triangle.v1;
            exportMesh->mFaces[faceIndex].mIndices[2] = triangle.v2;
        }

        aiScene exportScene;
        exportScene.mRootNode = new aiNode();
        exportScene.mMeshes = new aiMesh*[1];
        exportScene.mMeshes[0] = exportMesh;
        exportScene.mMeshes[0]->mMaterialIndex = 0;
        exportScene.mNumMeshes = 1;

        exportScene.mMaterials = new aiMaterial*[1];
        exportScene.mMaterials[0] = new aiMaterial();
        exportScene.mNumMaterials = 1;

        exportScene.mRootNode->mMeshes = new unsigned int[1];
        exportScene.mRootNode->mMeshes[0] = 0;
        exportScene.mRootNode->mNumMeshes = 1;

        Assimp::Exporter mAiExporter;
        Assimp::ExportProperties* properties = new Assimp::ExportProperties;
        // properties->SetPropertyBool(AI_CONFIG_EXPORT_POINT_CLOUDS, true);
        mAiExporter.Export(&exportScene, "obj", writePath, 0, properties);
        delete properties;
    }
};

int main(int argc, char** argv)
{
    std::string readPath, writePath;
    if (argc >= 3) {
        readPath = argv[1];
        writePath = argv[2];
    }

    MeshWelder meshWelder;
    std::vector<Mesh> importedMeshes;
    meshWelder.import(readPath, importedMeshes);
    Mesh resultMesh = meshWelder.weldMeshes(importedMeshes);
    meshWelder.exportMesh(resultMesh, writePath);

    return 0;
}
