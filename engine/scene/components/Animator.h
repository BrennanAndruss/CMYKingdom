#pragma once

#include <cmath>
#include <vector>
#include <glm/glm.hpp>
#include <iostream>
#include "scene/components/Component.h"
#include "resources/AssetManager.h"
#include "resources/SkeletalAnimation.h"
#include "resources/Handle.h"

namespace engine
{
	class AssetManager;
	struct Skeleton;
	struct AnimationClip;
}

namespace engine
{
	class Animator : public Component
	{
	public:
		Handle<Skeleton> skeleton;
		Handle<AnimationClip> clip;
		// Crossfade target
		Handle<AnimationClip> targetClip;
		float blendDuration = 0.0f;
		float blendElapsed = 0.0f;
		bool isBlending = false;
		float playbackSpeed = 1.0f;
		bool loop = true;
		bool debugClipLogging = false;
		std::string debugClipFilter = "jump";

		void update(float deltaTime, AssetManager& assets) override
		{

			auto* skeletonData = assets.getSkeleton(skeleton);
			auto* clipData = assets.getAnimationClip(clip);

			if (!skeletonData)
			{
				_boneMatrices.clear();
				_currentTime = 0.0f;
				return;
			}

			// Blending path: if a crossfade is active, sample both clips and blend
			if (isBlending)
			{
				auto* sourceClipData = assets.getAnimationClip(clip);
				auto* tgtClipData = assets.getAnimationClip(targetClip);

				// advance source and target times
				if (sourceClipData)
				{
					const float sTicksPerSec = (sourceClipData->ticksPerSecond > 0.0f) ? sourceClipData->ticksPerSecond : 25.0f;
					_sourceTime += deltaTime * playbackSpeed * sTicksPerSec;
					if (sourceClipData->durationTicks > 0.0f)
					{
						if (loop)
						{
							_sourceTime = std::fmod(_sourceTime, sourceClipData->durationTicks);
							if (_sourceTime < 0.0f) _sourceTime += sourceClipData->durationTicks;
						}
						else if (_sourceTime > sourceClipData->durationTicks)
						{
							_sourceTime = sourceClipData->durationTicks;
						}
					}
				}

				if (tgtClipData)
				{
					const float tTicksPerSec = (tgtClipData->ticksPerSecond > 0.0f) ? tgtClipData->ticksPerSecond : 25.0f;
					_targetTime += deltaTime * playbackSpeed * tTicksPerSec;
					if (tgtClipData->durationTicks > 0.0f)
					{
						if (loop)
						{
							_targetTime = std::fmod(_targetTime, tgtClipData->durationTicks);
							if (_targetTime < 0.0f) _targetTime += tgtClipData->durationTicks;
						}
						else if (_targetTime > tgtClipData->durationTicks)
						{
							_targetTime = tgtClipData->durationTicks;
						}
					}
				}

				// advance blend timer
				blendElapsed += deltaTime;
				float raw = (blendDuration > 0.0f) ? glm::clamp(blendElapsed / blendDuration, 0.0f, 1.0f) : 1.0f;
				// ease in-out cubic
				auto easeInOutCubic = [](float x) {
					return x < 0.5f ? 4.0f*x*x*x : 1.0f - std::pow(-2.0f*x + 2.0f, 3) / 2.0f;
				};
				float wt = easeInOutCubic(raw);

				// If either clip is missing, fall back to single-clip sampling
				if (!sourceClipData || !tgtClipData)
				{
					auto* useClip = tgtClipData ? tgtClipData : sourceClipData;
					if (useClip)
					{
						const float ticksPerSecond = (useClip->ticksPerSecond > 0.0f) ? useClip->ticksPerSecond : 25.0f;
						_currentTime += deltaTime * playbackSpeed * ticksPerSecond;
						_boneMatrices.assign(skeletonData->boneCount(), glm::mat4(1.0f));
						evaluateSkeletonPose(*skeletonData, useClip, _currentTime, _boneMatrices, false);
					}
				}
				else
				{
					blendClipsToSkinMatrices(*skeletonData, sourceClipData, _sourceTime, tgtClipData, _targetTime, wt, _boneMatrices);
				}

				// compute globals for visualization using target if available
				if (tgtClipData)
				{
					_globalMatrices.assign(skeletonData->boneCount(), glm::mat4(1.0f));
					evaluateSkeletonGlobals(*skeletonData, tgtClipData, _targetTime, _globalMatrices);
				}

				if (raw >= 1.0f)
				{
					// finish blend
					clip = targetClip;
					_currentTime = _targetTime;
					targetClip = {};
					isBlending = false;
					blendElapsed = 0.0f;
				}
			}
			else if (clipData)
			{
				if (debugClipLogging && shouldLogClip(*clipData) && !_debugPrinted)
				{
					std::cout << "[Animator] clip='" << clipData->name << "' time=" << _currentTime
						<< " durationTicks=" << clipData->durationTicks
						<< " playbackSpeed=" << playbackSpeed
						<< " loop=" << (loop ? "true" : "false") << "\n";
					_debugPrinted = true;
				}

				const float ticksPerSecond = (clipData->ticksPerSecond > 0.0f) ? clipData->ticksPerSecond : 25.0f;
				_currentTime += deltaTime * playbackSpeed * ticksPerSecond;

				if (clipData->durationTicks > 0.0f)
				{
					if (loop)
					{
						_currentTime = std::fmod(_currentTime, clipData->durationTicks);
						if (_currentTime < 0.0f)
						{
							_currentTime += clipData->durationTicks;
						}
					}
					else if (_currentTime > clipData->durationTicks)
					{
						_currentTime = clipData->durationTicks;
					}
				}
			}

			// If we are blending, the blending branch already populated _boneMatrices/_globalMatrices.
			if (!isBlending)
			{
				// Refresh active clip data in case a blend just finished this frame and
				// `clip` switched from source to target.
				auto* activeClipData = assets.getAnimationClip(clip);
				if (activeClipData)
				{
					_boneMatrices.assign(skeletonData->boneCount(), glm::mat4(1.0f));
					evaluateSkeletonPose(*skeletonData, activeClipData, _currentTime, _boneMatrices, false);

					// Also compute per-bone global transforms for debug visualization
					_globalMatrices.assign(skeletonData->boneCount(), glm::mat4(1.0f));
					evaluateSkeletonGlobals(*skeletonData, activeClipData, _currentTime, _globalMatrices);
				}
				else
				{
					_boneMatrices.clear();
					_globalMatrices.clear();
				}
			}

			// Debug: print first few bone matrices on first evaluation
			// (added false to here and evalulateSkeletonPose to turn off logs)
			// (maybe a better logging system for the engine can be added later...)
			if (false && !_debugPrinted && !_boneMatrices.empty())
			{
				_debugPrinted = true;
				std::cout << "[Animator] First pose evaluation - playback time=" << _currentTime << " ticks\n";
				for (std::size_t i = 0; i < std::min(_boneMatrices.size(), std::size_t(5)); ++i)
				{
					const auto& m = _boneMatrices[i];
					std::cout << "  Bone " << i << ": pos[" 
						<< m[3][0] << ", " << m[3][1] << ", " << m[3][2] << "] "
						<< "scale[" << glm::length(glm::vec3(m[0])) << ", " 
						<< glm::length(glm::vec3(m[1])) << ", " 
						<< glm::length(glm::vec3(m[2])) << "]\n";
				}

				std::vector<int> nodeIndexByBone(_boneMatrices.size(), -1);
				for (std::size_t nodeIdx = 0; nodeIdx < skeletonData->nodes.size(); ++nodeIdx)
				{
					const auto& node = skeletonData->nodes[nodeIdx];
					if (node.isBone && node.boneIndex >= 0 && static_cast<std::size_t>(node.boneIndex) < nodeIndexByBone.size())
					{
						nodeIndexByBone[static_cast<std::size_t>(node.boneIndex)] = static_cast<int>(nodeIdx);
					}
				}

				std::vector<glm::mat4> bindGlobalByNode(skeletonData->nodes.size(), glm::mat4(1.0f));
				if (skeletonData->rootNodeIndex >= 0)
				{
					std::vector<int> stack = { skeletonData->rootNodeIndex };
					while (!stack.empty())
					{
						int idx = stack.back();
						stack.pop_back();
						const auto& node = skeletonData->nodes[static_cast<std::size_t>(idx)];
						glm::mat4 parent = glm::mat4(1.0f);
						if (node.parentIndex >= 0)
						{
							parent = bindGlobalByNode[static_cast<std::size_t>(node.parentIndex)];
						}
						bindGlobalByNode[static_cast<std::size_t>(idx)] = parent * node.bindLocalTransform;
						for (int child : node.children)
						{
							stack.push_back(child);
						}
					}
				}

				std::cout << "[Animator] Per-bone CPU compare (animatedGlobalOrigin vs final*bindGlobalOrigin):\n";
				for (std::size_t i = 0; i < std::min(_boneMatrices.size(), std::size_t(12)); ++i)
				{
					const int nodeIdx = nodeIndexByBone[i];
					const char* name = (nodeIdx >= 0) ? skeletonData->nodes[static_cast<std::size_t>(nodeIdx)].name.c_str() : "<unknown>";
					if (nodeIdx < 0)
					{
						continue;
					}

					const glm::vec3 animatedGlobalOrigin = glm::vec3(_globalMatrices[i] * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
					const glm::vec4 bindGlobalOriginH = bindGlobalByNode[static_cast<std::size_t>(nodeIdx)] * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
					const glm::vec3 finalFromBindOrigin = glm::vec3(_boneMatrices[i] * bindGlobalOriginH);
					const glm::vec3 delta = finalFromBindOrigin - animatedGlobalOrigin;
					std::cout << "  Bone " << i << " (" << name << ")"
						<< " animatedGlobalOrigin=[" << animatedGlobalOrigin.x << ", " << animatedGlobalOrigin.y << ", " << animatedGlobalOrigin.z << "]"
						<< " final*bindGlobalOrigin=[" << finalFromBindOrigin.x << ", " << finalFromBindOrigin.y << ", " << finalFromBindOrigin.z << "]"
						<< " delta=[" << delta.x << ", " << delta.y << ", " << delta.z << "]\n";
				}
			}
		}

		// Play a clip (reset playback position). Use this when switching animations
		// and you want playback to start from the beginning or a specific start time.
		void play(Handle<AnimationClip> inClip, bool inLoop = true, float startTime = 0.0f, float crossfade = 0.0f)
		{
			// If we have an existing clip and request a crossfade, set up blending
			if (crossfade > 0.0f && clip.valid())
			{
				targetClip = inClip;
				blendDuration = crossfade;
				blendElapsed = 0.0f;
				isBlending = true;
				_targetTime = startTime;
				_sourceTime = _currentTime;
				loop = inLoop;
				_debugPrinted = false;
				if (debugClipLogging)
				{
					//std::cout << "[Animator] crossfade start: sourceHandle=" << clip.index << " targetHandle=" << targetClip.index
					//	<< " duration=" << blendDuration << "\n";
				}
			}
			else
			{
				clip = inClip;
				targetClip = {};
				isBlending = false;
				blendElapsed = 0.0f;
				blendDuration = 0.0f;
				loop = inLoop;
				_currentTime = startTime;
				_debugPrinted = false;
				if (debugClipLogging)
				{
					//std::cout << "[Animator] play() clip handle index=" << clip.index
						//<< " loop=" << (loop ? "true" : "false")
						//<< " startTime=" << startTime << "\n";
				}
			}
		}

		const std::vector<glm::mat4>& getBoneMatrices() const { return _boneMatrices; }
		const std::vector<glm::mat4>& getGlobalMatrices() const { return _globalMatrices; }
		bool hasPose() const { return !_boneMatrices.empty(); }

	private:
		float _currentTime = 0.0f;
		float _sourceTime = 0.0f;
		float _targetTime = 0.0f;
		std::vector<glm::mat4> _boneMatrices;
		std::vector<glm::mat4> _globalMatrices;
		bool _debugPrinted = false;

		bool shouldLogClip(const AnimationClip& clipData) const
		{
			if (debugClipFilter.empty())
			{
				return true;
			}

			auto toLower = [](std::string value)
			{
				std::transform(value.begin(), value.end(), value.begin(),
					[](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
				return value;
			};

			return toLower(clipData.name).find(toLower(debugClipFilter)) != std::string::npos;
		}
	};
}
