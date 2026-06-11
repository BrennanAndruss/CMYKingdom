#pragma once

#include <string>
#include <filesystem>
#include <vector>
#include <unordered_map>
#include <memory>
#include <array>
#include "resources/Handle.h"

// Forward declarations of resource types
namespace engine
{
	class Shader;
	class Texture;
	class Cubemap;
	class Mesh;
	struct Skeleton;
	struct AnimationClip;
	struct AudioClip;
	struct Material;
	class Heightmap;
}

namespace engine
{
	template<typename T>
	struct AssetPool
	{
		std::vector<std::unique_ptr<T>> assets;
		std::unordered_map<std::string, Handle<T>> nameToHandle;
	};

	class AssetManager
	{
	public:
		AssetManager(const std::string& assetDir);
		~AssetManager();

		// Loading
		Handle<Shader> loadShader(const std::string& name,
								  const std::string& vertPath,
								  const std::string& fragPath);
		Handle<Shader> loadEngineShader(const std::string& name,
										const std::string& vertPath,
										const std::string& fragPath);

		Handle<Texture> loadTexture(const std::string& name,
									const std::string& path,
									bool alpha);
		Handle<Texture> loadTexture(const std::string& name,
									unsigned char* data,
									int width, int height);

		Handle<Texture> createSolidTexture(const std::string& name,
									  const std::array<unsigned char, 4>& rgba);

		Handle<Cubemap> loadCubemap(const std::string& name,
									const std::array<std::string, 6>& facePaths);

		Handle<Mesh> loadMesh(const std::string& name, const std::string& path);
		Handle<Mesh> loadMeshAssimp(const std::string& name, const std::string& path,
			unsigned int meshIndex = 0);
		std::vector<Handle<Mesh>> loadModelMeshesAssimp(const std::string& namePrefix,
			const std::string& path);
		Handle<Skeleton> loadSkeletonAssimp(const std::string& name, const std::string& path,
			unsigned int meshIndex = 0);
		Handle<AnimationClip> loadAnimationClipAssimp(const std::string& name, const std::string& path,
			unsigned int animationIndex = 0);
		Handle<AudioClip> loadAudioClip(const std::string& name, const std::string& path);
		Handle<Material> loadMaterial(const std::string& name);

		Handle<Heightmap> loadHeightmap(const std::string& name,
										const std::string& path,
										float heightScale);

		// Generation
		Handle<Mesh> createPlaneMesh(const std::string& name,
									 int planeRes,
									 float planeLen);

		Handle<Mesh> createHeightmapMesh(const std::string& name,
										 Handle<Heightmap> heightmapHandle,
										 int planeRes,
										 float planeLen);

		// Access
		Shader* getShader(Handle<Shader> handle) const;
		Shader* getShader(const std::string& name) const;
		Handle<Shader> getShaderHandle(const std::string& name) const;

		Texture* getTexture(Handle<Texture> handle) const;
		Texture* getTexture(const std::string& name) const;
		Handle<Texture> getTextureHandle(const std::string& name) const;
		std::string getTextureName(Handle<Texture> handle) const;

		Cubemap* getCubemap(Handle<Cubemap> handle) const;
		Cubemap* getCubemap(const std::string& name) const;
		Handle<Cubemap> getCubemapHandle(const std::string& name) const;

		Mesh* getMesh(Handle<Mesh> handle) const;
		Mesh* getMesh(const std::string& name) const;
		Handle<Mesh> getMeshHandle(const std::string& name) const;
		std::string getMeshName(Handle<Mesh> handle) const;

		Skeleton* getSkeleton(Handle<Skeleton> handle) const;
		Skeleton* getSkeleton(const std::string& name) const;
		Handle<Skeleton> getSkeletonHandle(const std::string& name) const;

		AnimationClip* getAnimationClip(Handle<AnimationClip> handle) const;
		AnimationClip* getAnimationClip(const std::string& name) const;
		Handle<AnimationClip> getAnimationClipHandle(const std::string& name) const;

		AudioClip* getAudioClip(Handle<AudioClip> handle) const;
		AudioClip* getAudioClip(const std::string& name) const;
		Handle<AudioClip> getAudioClipHandle(const std::string& name) const;

		Material* getMaterial(Handle<Material> handle) const;
		Material* getMaterial(const std::string& name) const;
		Handle<Material> getMaterialHandle(const std::string& name) const;
		std::string getMaterialName(Handle<Material> handle) const;

		Heightmap* getHeightmap(Handle<Heightmap> handle) const;
		Heightmap* getHeightmap(const std::string& name) const;
		Handle<Heightmap> getHeightmapHandle(const std::string& name) const;

		// Default assets
		Handle<Shader> getDefaultShader() const { return _defaultShader; }
		void setDefaultShader(Handle<Shader> shader);

		Handle<Texture> getWhiteTexture() const { return _whiteTexture; }
		Handle<Texture> getBlackTexture() const { return _blackTexture; }
		Handle<Texture> getNormalTexture() const { return _normalTexture; }
		Handle<Material> getDefaultMaterial() const { return _defaultMaterial; }

	private:
		AssetPool<Shader> _shaders;
		AssetPool<Texture> _textures;
		AssetPool<Cubemap> _cubemaps;
		AssetPool<Mesh> _meshes;
		AssetPool<Skeleton> _skeletons;
		AssetPool<AnimationClip> _animationClips;
		AssetPool<AudioClip> _audioClips;
		AssetPool<Material> _materials;
		AssetPool<Heightmap> _heightmaps;

		Handle<Shader> _defaultShader;
		Handle<Texture> _whiteTexture;
		Handle<Texture> _blackTexture;
		Handle<Texture> _normalTexture;
		Handle<Material> _defaultMaterial;
		void loadDefaults();

		Handle<Shader> loadShaderFromRoot(const std::filesystem::path& root,
			const std::string& name,
			const std::string& vertPath,
			const std::string& fragPath);

		std::filesystem::path _assetDir;
		std::filesystem::path _engineAssetDir;
		std::filesystem::path resolvePath(const std::string& rel) const;
	};
}
