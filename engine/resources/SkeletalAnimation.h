#pragma once

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <cstddef>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>
#include <iostream>

namespace engine
{
	struct KeyPosition
	{
		float time = 0.0f;
		glm::vec3 value = glm::vec3(0.0f);
	};

	struct KeyRotation
	{
		float time = 0.0f;
		glm::quat value = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	};

	struct KeyScale
	{
		float time = 0.0f;
		glm::vec3 value = glm::vec3(1.0f);
	};

	struct BoneTrack
	{
		std::string nodeName;
		std::vector<KeyPosition> positions;
		std::vector<KeyRotation> rotations;
		std::vector<KeyScale> scales;
	};

	struct SkeletonNode
	{
		std::string name;
		int parentIndex = -1;
		std::vector<int> children;
		glm::mat4 bindLocalTransform = glm::mat4(1.0f);
		glm::mat4 inverseBindMatrix = glm::mat4(1.0f);
		bool isBone = false;
		int boneIndex = -1;
	};

	struct Skeleton
	{
		std::vector<SkeletonNode> nodes;
		std::unordered_map<std::string, int> nodeLookup;
		glm::mat4 globalInverseTransform = glm::mat4(1.0f);
		int rootNodeIndex = -1;

// Returns the number of bones (max `boneIndex` + 1) in the skeleton.
// Used by the engine to size skinning matrices and allocate per-bone GPU buffers.

		std::size_t boneCount() const
		{
			std::size_t count = 0;
			for (const auto& node : nodes)
			{
				if (node.isBone && node.boneIndex >= 0)
				{
					count = std::max(count, static_cast<std::size_t>(node.boneIndex + 1));
				}
			}
			return count;
		}

// Find the index of a node by name in `nodes` using `nodeLookup`.
// Used to map animation tracks and scene operations to skeleton nodes.

		int findNodeIndex(const std::string& name) const
		{
			auto it = nodeLookup.find(name);
			if (it == nodeLookup.end())
			{
				return -1;
			}
			return it->second;
		}
	};

	struct AnimationClip
	{
		std::string name;
		float durationTicks = 0.0f;
		float ticksPerSecond = 25.0f;
		std::vector<BoneTrack> tracks;
		std::unordered_map<std::string, std::size_t> trackLookup;

// Find the `BoneTrack` for a given node name in this clip.
// Used by the engine during pose evaluation to get per-node keyframe data.

		const BoneTrack* findTrack(const std::string& nodeName) const
		{
			auto it = trackLookup.find(nodeName);
			if (it == trackLookup.end())
			{
				return nullptr;
			}
			return &tracks[it->second];
		}
	};

// Compose a local transform matrix from translation, rotation and scale.
// Used throughout the animation system to construct per-node local matrices.

	inline glm::mat4 composeTransform(const glm::vec3& position,
		const glm::quat& rotation,
		const glm::vec3& scale)
	{
		return glm::translate(glm::mat4(1.0f), position) *
			glm::mat4_cast(rotation) *
			glm::scale(glm::mat4(1.0f), scale);
	}

// Find the index of the position keyframe that comes immediately before `time`.
// This is a helper for position interpolation during animation evaluation.

	inline std::size_t findPositionKey(const std::vector<KeyPosition>& keys, float time)
	{
		for (std::size_t i = 0; i + 1 < keys.size(); ++i)
		{
			if (time < keys[i + 1].time)
			{
				return i;
			}
		}
		return keys.size() - 1;
	}

// Find the index of the rotation keyframe that comes immediately before `time`.
// Used by rotation interpolation helpers.

	inline std::size_t findRotationKey(const std::vector<KeyRotation>& keys, float time)
	{
		for (std::size_t i = 0; i + 1 < keys.size(); ++i)
		{
			if (time < keys[i + 1].time)
			{
				return i;
			}
		}
		return keys.size() - 1;
	}

// Find the index of the scale keyframe that comes immediately before `time`.
// Used by scale interpolation helpers.

	inline std::size_t findScaleKey(const std::vector<KeyScale>& keys, float time)
	{
		for (std::size_t i = 0; i + 1 < keys.size(); ++i)
		{
			if (time < keys[i + 1].time)
			{
				return i;
			}
		}
		return keys.size() - 1;
	}

// Interpolate position from keyframes at `time`. Returns local translation for a node.
// Engine uses this when building local transforms for skinning and animation blending.

	inline glm::vec3 interpolatePosition(const std::vector<KeyPosition>& keys, float time)
	{
		if (keys.empty())
		{
			return glm::vec3(0.0f);
		}

		if (keys.size() == 1)
		{
			return keys.front().value;
		}

		std::size_t idx = findPositionKey(keys, time);
		const KeyPosition& current = keys[idx];
		const KeyPosition& next = keys[std::min(idx + 1, keys.size() - 1)];
		float delta = next.time - current.time;
		float factor = (delta > 0.0f) ? ((time - current.time) / delta) : 0.0f;
		return glm::mix(current.value, next.value, glm::clamp(factor, 0.0f, 1.0f));
	}

// Interpolate rotation (spherical linear interpolation) from keyframes at `time`.
// Produces a normalized quaternion used in composing local node rotations.

	inline glm::quat interpolateRotation(const std::vector<KeyRotation>& keys, float time)
	{
		if (keys.empty())
		{
			return glm::quat(1.0f, 0.0f, 0.0f, 0.0f); 
		}

		if (keys.size() == 1)
		{
			return glm::normalize(keys.front().value);
		}

		std::size_t idx = findRotationKey(keys, time);
		const KeyRotation& current = keys[idx];
		const KeyRotation& next = keys[std::min(idx + 1, keys.size() - 1)];
		float delta = next.time - current.time;
		float factor = (delta > 0.0f) ? ((time - current.time) / delta) : 0.0f;
		return glm::normalize(glm::slerp(current.value, next.value, glm::clamp(factor, 0.0f, 1.0f)));
	}

// Convert or fix rotation handedness from importer if necessary.
// Currently normalizes the quaternion; keeps door open for handedness adjustments.

	inline glm::quat convertAnimationRotationHandedness(const glm::quat& q)
	{
		return glm::normalize(q);
	}

// Interpolate scale from keyframes at `time`. Returns local scale for a node.

	inline glm::vec3 interpolateScale(const std::vector<KeyScale>& keys, float time)
	{
		if (keys.empty())
		{
			return glm::vec3(1.0f);
		}

		if (keys.size() == 1)
		{
			return keys.front().value;
		}

		std::size_t idx = findScaleKey(keys, time);
		const KeyScale& current = keys[idx];
		const KeyScale& next = keys[std::min(idx + 1, keys.size() - 1)];
		float delta = next.time - current.time;
		float factor = (delta > 0.0f) ? ((time - current.time) / delta) : 0.0f;
		return glm::mix(current.value, next.value, glm::clamp(factor, 0.0f, 1.0f));
	}

// Evaluate a `BoneTrack` at `time` and return the composed local transform matrix.
// Used when a node has a track and we need its animated local transform.

	inline glm::mat4 evaluateTrack(const BoneTrack& track, float time)
	{
		return composeTransform(
			interpolatePosition(track.positions, time),
			interpolateRotation(track.rotations, time),
			interpolateScale(track.scales, time)
		);
	}

// Determine whether a node's animated rotation should be treated as absolute
// (not relative to bind basis). Primarily used for root/hips nodes to preserve intent.

	inline bool shouldUseAnimatedRotationAbsolute(const SkeletonNode& node)
	{
		// Root/hips are usually authored in absolute local orientation.
		if (node.parentIndex < 0)
		{
			return true;
		}
		return node.name.find("Hips") != std::string::npos;
	}

// Evaluate a track but preserve the node's bind translation when appropriate.
// This helps keep mesh vertices aligned with bind pose translation while applying animation.

	inline glm::mat4 evaluateTrackWithBindTranslation(const SkeletonNode& node, const BoneTrack& track, float time)
	{
		glm::vec3 position = interpolatePosition(track.positions, time);
		glm::quat rotation = convertAnimationRotationHandedness(interpolateRotation(track.rotations, time));
		glm::vec3 scale = interpolateScale(track.scales, time);

		if (!shouldUseAnimatedRotationAbsolute(node))
		{
			const glm::mat3 bindBasis(node.bindLocalTransform);
		}

		return composeTransform(
			position,
			rotation,
			scale
		);
	}

// Sample local transforms for every node in the skeleton from `clip` at `time`.
// If a node has no track, its bindLocalTransform is used. The resulting vector
// is indexed by node index (same order as `skeleton.nodes`). This is useful for
// performing per-node blending before computing skinning matrices.
inline void sampleLocalTransforms(const Skeleton& skeleton, const AnimationClip* clip, float time, std::vector<glm::mat4>& outLocalTransforms)
{
	outLocalTransforms.resize(skeleton.nodes.size());
	for (std::size_t i = 0; i < skeleton.nodes.size(); ++i)
	{
		outLocalTransforms[i] = skeleton.nodes[i].bindLocalTransform;
	}

	if (!clip)
		return;

	for (const auto& track : clip->tracks)
	{
		int nodeIdx = skeleton.findNodeIndex(track.nodeName);
		if (nodeIdx >= 0 && static_cast<std::size_t>(nodeIdx) < outLocalTransforms.size())
		{
			outLocalTransforms[static_cast<std::size_t>(nodeIdx)] = evaluateTrackWithBindTranslation(skeleton.nodes[static_cast<std::size_t>(nodeIdx)], track, time);
		}
	}
}

// Blend two local transform matrices by decomposing to TRS, interpolating each
// component (translation & scale linearly, rotation with slerp), and recomposing.
inline glm::mat4 blendLocalTransforms(const glm::mat4& A, const glm::mat4& B, float t)
{
	glm::vec3 TA = glm::vec3(A[3]);  
	glm::vec3 TB = glm::vec3(B[3]); 

	glm::mat3 MA = glm::mat3(A);
	glm::mat3 MB = glm::mat3(B);

	glm::vec3 SA(glm::length(MA[0]), glm::length(MA[1]), glm::length(MA[2]));
	glm::vec3 SB(glm::length(MB[0]), glm::length(MB[1]), glm::length(MB[2]));

	glm::mat3 RA = MA;
	if (SA.x > 0.0f) RA[0] /= SA.x; if (SA.y > 0.0f) RA[1] /= SA.y; if (SA.z > 0.0f) RA[2] /= SA.z;
	glm::mat3 RB = MB;
	if (SB.x > 0.0f) RB[0] /= SB.x; if (SB.y > 0.0f) RB[1] /= SB.y; if (SB.z > 0.0f) RB[2] /= SB.z;

	glm::quat QA = glm::normalize(glm::quat_cast(RA));
	glm::quat QB = glm::normalize(glm::quat_cast(RB));

	glm::vec3 T = glm::mix(TA, TB, glm::clamp(t, 0.0f, 1.0f));
	glm::quat R = glm::normalize(glm::slerp(QA, QB, glm::clamp(t, 0.0f, 1.0f)));
	glm::vec3 S = glm::mix(SA, SB, glm::clamp(t, 0.0f, 1.0f));

	return composeTransform(T, R, S);
}

// Compute skinning matrices (final bone matrices used for GPU skinning) from a
// per-node local transform array. Mirrors evaluateSkeletonPoseRecursive but uses
// the provided local transforms vector instead of sampling tracks.
inline void computeSkinMatricesFromLocalTransforms(
	const Skeleton& skeleton,
	const std::vector<glm::mat4>& localTransforms,
	std::vector<glm::mat4>& outBoneMatrices)
{
	if (skeleton.rootNodeIndex < 0)
		return;

	if (outBoneMatrices.size() < skeleton.boneCount())
		outBoneMatrices.resize(skeleton.boneCount(), glm::mat4(1.0f));

	// iterative stack to avoid recursion
	struct StackItem { int nodeIndex; glm::mat4 parentGlobal; };
	std::vector<StackItem> stack;
	stack.push_back({ skeleton.rootNodeIndex, glm::mat4(1.0f) });

	while (!stack.empty())
	{
		StackItem it = stack.back(); stack.pop_back();
		int nodeIndex = it.nodeIndex;
		if (nodeIndex < 0 || nodeIndex >= static_cast<int>(skeleton.nodes.size()))
			continue;
		const SkeletonNode& node = skeleton.nodes[static_cast<std::size_t>(nodeIndex)];
		glm::mat4 local = localTransforms[static_cast<std::size_t>(nodeIndex)];
		glm::mat4 global = it.parentGlobal * local;

		if (node.isBone && node.boneIndex >= 0 && static_cast<std::size_t>(node.boneIndex) < outBoneMatrices.size())
		{
			outBoneMatrices[static_cast<std::size_t>(node.boneIndex)] = skeleton.globalInverseTransform * global * node.inverseBindMatrix;
		}

		for (int child : node.children)
		{
			stack.push_back({ child, global });
		}
	}
}

// Blend two clips at given times with weight `t` (0..1) and produce skinning
// matrices into `outBoneMatrices`. This helper is a simple tweening function the
// engine can call to crossfade between animations on the CPU.
inline void blendClipsToSkinMatrices(
	const Skeleton& skeleton,
	const AnimationClip* a, float timeA,
	const AnimationClip* b, float timeB,
	float t,
	std::vector<glm::mat4>& outBoneMatrices)
{
	std::vector<glm::mat4> localA, localB, blendedLocal;
	sampleLocalTransforms(skeleton, a, timeA, localA);
	sampleLocalTransforms(skeleton, b, timeB, localB);

	blendedLocal.resize(skeleton.nodes.size());
	for (std::size_t i = 0; i < skeleton.nodes.size(); ++i)
	{
		const glm::mat4& A = (i < localA.size()) ? localA[i] : skeleton.nodes[i].bindLocalTransform;
		const glm::mat4& B = (i < localB.size()) ? localB[i] : skeleton.nodes[i].bindLocalTransform;
		blendedLocal[i] = blendLocalTransforms(A, B, t);
	}

	computeSkinMatricesFromLocalTransforms(skeleton, blendedLocal, outBoneMatrices);
}

// Recursively evaluate the skeleton pose at `time`, filling `outBoneMatrices` with
// skinning matrices (global-inverse * globalTransform * inverseBind). Called per-frame
// by the animation system to produce GPU-ready bone matrices for skinning.

	inline void evaluateSkeletonPoseRecursive(
		const Skeleton& skeleton,
		const AnimationClip* clip,
		int nodeIndex,
		const glm::mat4& parentTransform,
		float time,
		std::vector<glm::mat4>& outBoneMatrices,
		bool debugFirstFrame = false,
		int debugDepth = 0)
	{
		if (nodeIndex < 0 || nodeIndex >= static_cast<int>(skeleton.nodes.size()))
		{
			return;
		}

		const SkeletonNode& node = skeleton.nodes[nodeIndex];
		glm::mat4 localTransform = node.bindLocalTransform; // start with bind pose matrix for vertices to go from model space to bone space

		bool hasTrack = false;
		if (clip)
		{
			if (const BoneTrack* track = clip->findTrack(node.name)) // if animation track for this node exists, 
			{
				localTransform = evaluateTrackWithBindTranslation(node, *track, time);
				hasTrack = true;
			}
		}

		glm::mat4 globalTransform = parentTransform * localTransform; // model space -> bone space -> model space

		if (node.isBone && node.boneIndex >= 0 && node.boneIndex < static_cast<int>(outBoneMatrices.size())) // check if valid bone index and if a bone 
		{
			// Compute both conventions for debug, but use the global-inverse version for skinning.
			glm::mat4 finalWithGlobalInv = skeleton.globalInverseTransform * globalTransform * node.inverseBindMatrix;
			glm::mat4 finalWithoutGlobalInv = globalTransform * node.inverseBindMatrix;

			// Apply the global inverse in the active skinning path.
			outBoneMatrices[node.boneIndex] = finalWithGlobalInv;

			// Debug output for first frame
			if (debugFirstFrame && debugDepth < 3)
			{
				std::cout << "  [Pose] Bone " << node.boneIndex << " (" << node.name << "):\n"
					<< "    Local[3]: [" << localTransform[3][0] << ", " << localTransform[3][1] << ", " << localTransform[3][2] << "]"
					<< (hasTrack ? " (from track)" : " (bind pose)") << "\n"
					<< "    Global[3]: [" << globalTransform[3][0] << ", " << globalTransform[3][1] << ", " << globalTransform[3][2] << "]\n"
					<< "    FinalWithoutGlobalInv[3]: [" << finalWithoutGlobalInv[3][0] << ", " 
					<< finalWithoutGlobalInv[3][1] << ", " 
					<< finalWithoutGlobalInv[3][2] << "]\n"
					<< "    FinalWithGlobalInv[3]: [" << finalWithGlobalInv[3][0] << ", " 
					<< finalWithGlobalInv[3][1] << ", " 
					<< finalWithGlobalInv[3][2] << "]\n";
			}
		}

		for (int childIndex : node.children)
		{
			evaluateSkeletonPoseRecursive(skeleton, clip, childIndex, globalTransform, time, outBoneMatrices, debugFirstFrame, debugDepth + 1);
		}
	}

// Evaluate the full skeleton pose for `clip` at `time` and populate `outBoneMatrices`.
// Handles root setup and ensures `outBoneMatrices` has correct size for skinning.

	inline void evaluateSkeletonPose(
		const Skeleton& skeleton,
		const AnimationClip* clip,
		float time,
		std::vector<glm::mat4>& outBoneMatrices,
		bool debugFirstFrame = false)
	{
		if (skeleton.rootNodeIndex < 0)
		{
			return;
		}

		if (outBoneMatrices.size() < skeleton.boneCount())
		{
			outBoneMatrices.resize(skeleton.boneCount(), glm::mat4(1.0f));
		}

		evaluateSkeletonPoseRecursive(
			skeleton,
			clip,
			skeleton.rootNodeIndex,
			glm::mat4(1.0f),
			time,
			outBoneMatrices,
			debugFirstFrame
		);
	}

	// Evaluate global transforms (animated) for each bone (indexed by boneIndex)
	inline void evaluateSkeletonGlobalsRecursive(
		const Skeleton& skeleton,
		const AnimationClip* clip,
		int nodeIndex,
		const glm::mat4& parentTransform,
		float time,
		std::vector<glm::mat4>& outGlobalTransforms)
	{
		if (nodeIndex < 0 || nodeIndex >= static_cast<int>(skeleton.nodes.size()))
		{
			return;
		}

		const SkeletonNode& node = skeleton.nodes[nodeIndex];
		glm::mat4 localTransform = node.bindLocalTransform;
		if (clip)
		{
			if (const BoneTrack* track = clip->findTrack(node.name))
			{
				localTransform = evaluateTrackWithBindTranslation(node, *track, time);
			}
		}

		glm::mat4 globalTransform = parentTransform * localTransform;

		if (node.isBone && node.boneIndex >= 0 && node.boneIndex < static_cast<int>(outGlobalTransforms.size()))
		{
			outGlobalTransforms[node.boneIndex] = globalTransform;
		}

		for (int childIndex : node.children)
		{
			evaluateSkeletonGlobalsRecursive(skeleton, clip, childIndex, globalTransform, time, outGlobalTransforms);
		}
	}



// Compute per-bone global transforms (animated) at `time` and store them in
// `outGlobalTransforms`. This is useful for debugging systems or non-skinning queries.

	inline void evaluateSkeletonGlobals(
		const Skeleton& skeleton,
		const AnimationClip* clip,
		float time,
		std::vector<glm::mat4>& outGlobalTransforms)
	{
		if (skeleton.rootNodeIndex < 0) return;
		if (outGlobalTransforms.size() < skeleton.boneCount())
		{
			outGlobalTransforms.resize(skeleton.boneCount(), glm::mat4(1.0f));
		}

		evaluateSkeletonGlobalsRecursive(
			skeleton,
			clip,
			skeleton.rootNodeIndex,
			glm::mat4(1.0f),
			time,
			outGlobalTransforms
		);
	}

}
