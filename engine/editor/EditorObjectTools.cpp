#include "editor/EditorObjectTools.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <imgui.h>

#include "core/Input.h"
#include "renderer/resources/Material.h"
#include "resources/AssetManager.h"
#include "scene/Scene.h"
#include "scene/components/Components.h"
#include "systems/Collectable.h"

namespace
{
    bool isTrackedEditorObject(const std::vector<std::unique_ptr<engine::Object>>& objects,
        const engine::Object* object,
        std::size_t initialObjectCount)
    {
        for (std::size_t i = initialObjectCount; i < objects.size(); ++i)
        {
            if (objects[i].get() == object)
            {
                return true;
            }
        }

        return false;
    }

    std::string buildHierarchyLabel(const engine::Object* object)
    {
        std::string label = object->name;

        if (const auto* parentTransform = object->transform.getParent())
        {
            if (const auto* parentObject = parentTransform->owner)
            {
                label += "  [child of ";
                label += parentObject->name;
                label += ']';
            }
            else
            {
                label += "  [child]";
            }
        }

        const std::size_t childCount = object->transform.getChildren().size();
        if (childCount > 0)
        {
            label += "  [" + std::to_string(childCount) + (childCount == 1 ? " child]" : " children]");
        }

        return label;
    }

    void drawHierarchyEntry(engine::Object* object,
        engine::Object*& selectedObject,
        const std::vector<std::unique_ptr<engine::Object>>& objects,
        std::size_t initialObjectCount)
    {
        if (!object || !isTrackedEditorObject(objects, object, initialObjectCount))
        {
            return;
        }

        const bool hasChildren = !object->transform.getChildren().empty();
        const bool isSelected = selectedObject == object;
        const std::string label = buildHierarchyLabel(object);

        ImGui::PushID(object);

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow;
        if (isSelected)
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        if (!hasChildren)
        {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }

        const bool open = ImGui::TreeNodeEx("##object", flags, "%s", label.c_str());
        if (ImGui::IsItemClicked())
        {
            selectedObject = object;
        }

        if (open && hasChildren)
        {
            for (auto* childTransform : object->transform.getChildren())
            {
                if (childTransform && childTransform->owner)
                {
                    drawHierarchyEntry(childTransform->owner, selectedObject, objects, initialObjectCount);
                }
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    bool rayIntersectsAabb(const glm::vec3& rayOrigin,
        const glm::vec3& rayDirection,
        const glm::vec3& boundsMin,
        const glm::vec3& boundsMax,
        float& outDistance)
    {
        float tMin = 0.0f;
        float tMax = std::numeric_limits<float>::max();

        for (int i = 0; i < 3; ++i)
        {
            if (std::fabs(rayDirection[i]) < 1e-6f)
            {
                if (rayOrigin[i] < boundsMin[i] || rayOrigin[i] > boundsMax[i])
                {
                    return false;
                }
                continue;
            }

            const float invDirection = 1.0f / rayDirection[i];
            float t1 = (boundsMin[i] - rayOrigin[i]) * invDirection;
            float t2 = (boundsMax[i] - rayOrigin[i]) * invDirection;
            if (t1 > t2)
            {
                std::swap(t1, t2);
            }

            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);
            if (tMax < tMin)
            {
                return false;
            }
        }

        outDistance = (tMin >= 0.0f) ? tMin : tMax;
        return outDistance >= 0.0f;
    }

    bool rayIntersectsSphere(const glm::vec3& rayOrigin,
        const glm::vec3& rayDirection,
        const glm::vec3& center,
        float radius,
        float& outDistance)
    {
        const glm::vec3 toCenter = rayOrigin - center;
        const float a = glm::dot(rayDirection, rayDirection);
        const float b = 2.0f * glm::dot(toCenter, rayDirection);
        const float c = glm::dot(toCenter, toCenter) - (radius * radius);
        const float discriminant = b * b - 4.0f * a * c;
        if (discriminant < 0.0f)
        {
            return false;
        }

        const float sqrtDiscriminant = std::sqrt(discriminant);
        const float t0 = (-b - sqrtDiscriminant) / (2.0f * a);
        const float t1 = (-b + sqrtDiscriminant) / (2.0f * a);
        outDistance = (t0 >= 0.0f) ? t0 : t1;
        return outDistance >= 0.0f;
    }

    bool rayIntersectsObject(const engine::Object& object,
        const glm::vec3& rayOrigin,
        const glm::vec3& rayDirection,
        float& outDistance)
    {
        const glm::vec3 worldPos = object.transform.getWorldPosition();
        const glm::vec3 worldScale = glm::abs(object.transform.getWorldScale());

        if (const auto* box = object.getComponent<engine::BoxCollider>())
        {
            const glm::vec3 center = worldPos + box->center;
            const glm::vec3 halfExtents = box->size * worldScale;
            return rayIntersectsAabb(
                rayOrigin,
                rayDirection,
                center - halfExtents,
                center + halfExtents,
                outDistance
            );
        }

        if (const auto* sphere = object.getComponent<engine::SphereCollider>())
        {
            const float radiusScale = std::max(worldScale.x, std::max(worldScale.y, worldScale.z));
            const float radius = sphere->radius * radiusScale;
            return rayIntersectsSphere(rayOrigin, rayDirection, worldPos + sphere->center, radius, outDistance);
        }

        const glm::vec3 fallbackHalfExtents = glm::max(worldScale * 0.5f, glm::vec3(0.25f));
        return rayIntersectsAabb(
            rayOrigin,
            rayDirection,
            worldPos - fallbackHalfExtents,
            worldPos + fallbackHalfExtents,
            outDistance
        );
    }

    engine::Object* pickObjectAtMouse(engine::Scene& scene, std::size_t startIndex)
    {
        engine::Camera* camera = scene.getMainCamera();
        if (!camera)
        {
            return nullptr;
        }

        int windowWidth = 0;
        int windowHeight = 0;
        if (GLFWwindow* window = glfwGetCurrentContext())
        {
            glfwGetWindowSize(window, &windowWidth, &windowHeight);
        }

        if (windowWidth <= 0 || windowHeight <= 0)
        {
            const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
            windowWidth = static_cast<int>(displaySize.x);
            windowHeight = static_cast<int>(displaySize.y);
        }

        if (windowWidth <= 0 || windowHeight <= 0)
        {
            return nullptr;
        }

        const glm::vec2 mousePos = engine::Input::getMousePos();
        const float ndcX = (2.0f * mousePos.x) / static_cast<float>(windowWidth) - 1.0f;
        const float ndcY = 1.0f - (2.0f * mousePos.y) / static_cast<float>(windowHeight);

        camera->updateViewMatrix();
        const auto& cameraData = camera->getCameraData();
        const glm::mat4 inverseViewProjection = glm::inverse(cameraData.projection * cameraData.view);

        glm::vec4 nearPoint = inverseViewProjection * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
        glm::vec4 farPoint = inverseViewProjection * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
        if (std::fabs(nearPoint.w) < 1e-6f || std::fabs(farPoint.w) < 1e-6f)
        {
            return nullptr;
        }

        nearPoint /= nearPoint.w;
        farPoint /= farPoint.w;

        const glm::vec3 rayOrigin = glm::vec3(nearPoint);
        const glm::vec3 rayDirection = glm::normalize(glm::vec3(farPoint - nearPoint));

        engine::Object* closestObject = nullptr;
        float closestDistance = std::numeric_limits<float>::max();

        const auto& objects = scene.getObjects();
        for (std::size_t i = startIndex; i < objects.size(); ++i)
        {
            const auto& object = objects[i];
            if (object->getComponent<engine::Camera>())
            {
                continue;
            }

            float hitDistance = 0.0f;
            if (rayIntersectsObject(*object, rayOrigin, rayDirection, hitDistance) && hitDistance < closestDistance)
            {
                closestDistance = hitDistance;
                closestObject = object.get();
            }
        }

        return closestObject;
    }

    bool isNameInUse(const engine::Scene& scene, const std::string& name)
    {
        for (const auto& object : scene.getObjects())
        {
            if (object->name == name)
            {
                return true;
            }
        }
        return false;
    }

    std::string makeUniqueName(const engine::Scene& scene, const std::string& baseName, const std::string& suffix)
    {
        int index = 0;
        while (isNameInUse(scene, baseName + suffix + std::to_string(index)))
        {
            ++index;
        }
        return baseName + suffix + std::to_string(index);
    }

    void deleteObject(engine::Scene& scene, engine::Object* objectToDelete)
    {
        if (objectToDelete)
            objectToDelete->markedForDeletion = true;
    }
}

namespace engine
{
    void updateEditorSelectionFromMouse(Scene& scene, std::size_t startIndex, Object*& selectedObject)
    {
        const bool mouseOverUi = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) || ImGui::IsAnyItemHovered();
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !mouseOverUi)
        {
            if (Object* pickedObject = pickObjectAtMouse(scene, startIndex))
            {
                selectedObject = pickedObject;
            }
        }
    }

    void drawEditorObjectControls(Scene& scene, AssetManager& assets, std::size_t initialObjectCount, Object*& selectedObject)
    {
        ImGui::Text("Create object:");
        /*
        if (ImGui::Button("Create Cube"))
        {
            const std::string cubeName = makeUniqueName(scene, "Cube", "");
            auto& cube = scene.createObject(cubeName);
            auto& meshRenderer = cube.addComponent<MeshRenderer>();
            meshRenderer.mesh = assets.getMeshHandle("cube");
            meshRenderer.material = assets.getDefaultMaterial();
            selectedObject = &cube;
        }
        */
       
       if (ImGui::Button("Platform"))
        {
            const std::string platformName = makeUniqueName(scene, "Platform", "");
            auto& platform = scene.createObject(platformName);
            platform.transform.setScale(glm::vec3(5.0f, 2.0f, 5.0f));

            auto& meshRenderer = platform.addComponent<MeshRenderer>();
            meshRenderer.mesh = assets.getMeshHandle("square-platform");
            if (!meshRenderer.mesh.valid())
            {
                meshRenderer.mesh = assets.getMeshHandle("cube");
            }
            
            // Use CMYK platform material if available, otherwise use default
            auto platformMat = assets.getMaterialHandle("platformCMYKMaterial");
            meshRenderer.material = platformMat.valid() ? platformMat : assets.getDefaultMaterial();

            auto& collider = platform.addComponent<BoxCollider>();
            
            if (auto* mesh = assets.getMesh(meshRenderer.mesh))
            {
                const auto bounds = mesh->getBBox();
                collider.center = 0.5f * (bounds.max + bounds.min);
                glm::vec3 halfExtents = 0.5f * (bounds.max - bounds.min);
                collider.size = halfExtents;
            }
            else
            {
                collider.center = glm::vec3(0.0f);
                collider.size = glm::vec3(1.0f);
            }
            collider.rebuild();

            auto& rigidBody = platform.addComponent<RigidBody>();
            // Make platform kinematic so Bullet can resolve collisions and friction
            rigidBody.setBodyType(RigidBody::BodyType::Kinematic);
            rigidBody.friction = 1.0f;

            selectedObject = &platform;
        }
        ImGui::SeparatorText("General Nature");
        if (ImGui::Button("Rock")) {
            const std::string rockName = makeUniqueName(scene, "Rock", "");
            auto& rock = scene.createObject(rockName);
            rock.transform.setScale(glm::vec3(0.05f));
            
            rock.transform.setEulerAngles(0.0f, 0.0f, 0.0f);

            auto& meshRenderer = rock.addComponent<MeshRenderer>();
            meshRenderer.mesh = assets.getMeshHandle("rock");
            const Handle<Material> rockMaterial = assets.getMaterialHandle("rockMat");
            meshRenderer.material = rockMaterial.valid() ? rockMaterial : assets.getDefaultMaterial();

            auto& collider = rock.addComponent<BoxCollider>();
            collider.isTrigger = false;
            if (auto* mesh = assets.getMesh(meshRenderer.mesh))
            {
                const auto bounds = mesh->getBBox();
                collider.center = 0.5f * (bounds.max + bounds.min);
                collider.size = 0.5f * (bounds.max - bounds.min);
                collider.size.x *= 0.3f; 
                collider.size.z *= 0.3f; 
            }
            else
            {
                collider.center = glm::vec3(0.0f);
                collider.size = glm::vec3(1.0f);
            }
            collider.rebuild();

            auto& rigidBody = rock.addComponent<RigidBody>();
            rigidBody.setBodyType(RigidBody::BodyType::Static);
            rigidBody.friction = 1.0f;

            selectedObject = &rock;
        }


        if (ImGui::Button("Maple Tree")) {
            const std::string treeName = makeUniqueName(scene, "Maple Tree", "");
            auto& tree = scene.createObject(treeName);
            tree.transform.setScale(glm::vec3(0.5f));
            
            tree.transform.setEulerAngles(-90.0f, 0.0f, 0.0f);

            auto& meshRenderer = tree.addComponent<MeshRenderer>();
            meshRenderer.mesh = assets.getMeshHandle("tree1");
            const Handle<Material> treeMaterial = assets.getMaterialHandle("tree1Mat");
            meshRenderer.material = treeMaterial.valid() ? treeMaterial : assets.getDefaultMaterial();

            auto& collider = tree.addComponent<BoxCollider>();
            collider.isTrigger = false;
            if (auto* mesh = assets.getMesh(meshRenderer.mesh))
            {
                const auto bounds = mesh->getBBox();
                collider.center = 0.5f * (bounds.max + bounds.min);
                collider.size = 0.5f * (bounds.max - bounds.min);
            }
            else
            {
                collider.center = glm::vec3(0.0f);
                collider.size = glm::vec3(1.0f);
            }

            
            collider.rebuild();



            auto& rigidBody = tree.addComponent<RigidBody>();
            rigidBody.setBodyType(RigidBody::BodyType::Static);
            rigidBody.friction = 1.0f;

            selectedObject = &tree;
        }
        ImGui::SameLine();

        if (ImGui::Button("Winding Tree")) {
            const std::string treeName = makeUniqueName(scene, "Winding Tree", "");
            auto& tree = scene.createObject(treeName);
            tree.transform.setScale(glm::vec3(1.0f));
            
            tree.transform.setEulerAngles(0.0f, 0.0f, 0.0f);

            auto& meshRenderer = tree.addComponent<MeshRenderer>();
            meshRenderer.mesh = assets.getMeshHandle("tree2");
            const Handle<Material> treeMaterial = assets.getMaterialHandle("tree2Mat");
            meshRenderer.material = treeMaterial.valid() ? treeMaterial : assets.getDefaultMaterial();

            auto& collider = tree.addComponent<BoxCollider>();
            collider.isTrigger = false;
            if (auto* mesh = assets.getMesh(meshRenderer.mesh))
            {
                const auto bounds = mesh->getBBox();
                collider.center = 0.5f * (bounds.max + bounds.min);
                collider.size = 0.5f * (bounds.max - bounds.min);
            }
            else
            {
                collider.center = glm::vec3(0.0f);
                collider.size = glm::vec3(1.0f);
            }
            collider.rebuild();

            auto& rigidBody = tree.addComponent<RigidBody>();
            rigidBody.setBodyType(RigidBody::BodyType::Static);
            rigidBody.friction = 1.0f;

            selectedObject = &tree;
        }
        ImGui::SameLine();

        if (ImGui::Button("Pine Tree")) {
            const std::string treeName = makeUniqueName(scene, "Pine Tree", "");
            auto& tree = scene.createObject(treeName);
            tree.transform.setScale(glm::vec3(5.0f));
            
            tree.transform.setEulerAngles(0.0f, 0.0f, 0.0f);

            auto& meshRenderer = tree.addComponent<MeshRenderer>();
            meshRenderer.mesh = assets.getMeshHandle("tree3_0");
            if (!meshRenderer.mesh.valid())
            {
                meshRenderer.mesh = assets.getMeshHandle("tree3");
            }

            Handle<Material> treeMaterial = assets.getMaterialHandle("tree3Mat0");
            if (!treeMaterial.valid())
            {
                treeMaterial = assets.getMaterialHandle("tree3Mat");
            }
            meshRenderer.material = treeMaterial.valid() ? treeMaterial : assets.getDefaultMaterial();

            auto& collider = tree.addComponent<BoxCollider>();
            collider.isTrigger = false;
            if (auto* mesh = assets.getMesh(meshRenderer.mesh))
            {
                const auto bounds = mesh->getBBox();
                collider.center = 0.5f * (bounds.max + bounds.min);
                collider.size = 0.5f * (bounds.max - bounds.min);
            }
            else
            {
                collider.center = glm::vec3(0.0f);
                collider.size = glm::vec3(1.0f);
            }
            collider.rebuild();

            auto& rigidBody = tree.addComponent<RigidBody>();
            rigidBody.setBodyType(RigidBody::BodyType::Static);
            rigidBody.friction = 1.0f;

            selectedObject = &tree;
        }
        
        ImGui::SeparatorText("Desert");
        if (ImGui::Button("Cactus")) {
            const std::string cactusName = makeUniqueName(scene, "Cactus", "");
            auto& cactus = scene.createObject(cactusName);
            cactus.transform.setScale(glm::vec3(0.5f));
            
            cactus.transform.setEulerAngles(0.0f, 0.0f, 0.0f);

            auto& meshRenderer = cactus.addComponent<MeshRenderer>();
            meshRenderer.mesh = assets.getMeshHandle("cactus");
            const Handle<Material> cactusMaterial = assets.getMaterialHandle("cactusMat");
            meshRenderer.material = cactusMaterial.valid() ? cactusMaterial : assets.getDefaultMaterial();

            auto& collider = cactus.addComponent<BoxCollider>();
            collider.isTrigger = false;
            if (auto* mesh = assets.getMesh(meshRenderer.mesh))
            {
                const auto bounds = mesh->getBBox();
                collider.center = 0.5f * (bounds.max + bounds.min);
                collider.size = 0.5f * (bounds.max - bounds.min);
                collider.size.x *= 0.5f; // Adjust collider height to better fit cactus shape
                collider.size.z *= 0.5f; // Adjust collider height to better fit cactus shape
            }
            else
            {
                collider.center = glm::vec3(0.0f);
                collider.size = glm::vec3(1.0f);
            }
            collider.rebuild();

            auto& rigidBody = cactus.addComponent<RigidBody>();
            rigidBody.setBodyType(RigidBody::BodyType::Static);
            rigidBody.friction = 1.0f;

            selectedObject = &cactus;
        }
        ImGui::SameLine();
        if (ImGui::Button("Desert Rock")) {
            const std::string rockName = makeUniqueName(scene, "Desert Rock", "");
            auto& rock = scene.createObject(rockName);
            rock.transform.setScale(glm::vec3(1.0f));
            
            rock.transform.setEulerAngles(0.0f, 0.0f, 0.0f);

            auto& meshRenderer = rock.addComponent<MeshRenderer>();
            meshRenderer.mesh = assets.getMeshHandle("desert_rock");
            const Handle<Material> rockMaterial = assets.getMaterialHandle("desertRockMat");
            meshRenderer.material = rockMaterial.valid() ? rockMaterial : assets.getDefaultMaterial();

            auto& collider = rock.addComponent<BoxCollider>();
            collider.isTrigger = false;
            if (auto* mesh = assets.getMesh(meshRenderer.mesh))
            {
                const auto bounds = mesh->getBBox();
                collider.center = 0.5f * (bounds.max + bounds.min);
                collider.size = 0.5f * (bounds.max - bounds.min);
                collider.size.x *= 0.5f; 
                collider.size.z *= 0.5f; 
            }
            else
            {
                collider.center = glm::vec3(0.0f);
                collider.size = glm::vec3(1.0f);
            }
            collider.rebuild();

            auto& rigidBody = rock.addComponent<RigidBody>();
            rigidBody.setBodyType(RigidBody::BodyType::Static);
            rigidBody.friction = 1.0f;
            
            selectedObject = &rock;
        }
        ImGui::SameLine();

        if (ImGui::Button("Desert House")) {
            const std::string houseName = makeUniqueName(scene, "Desert House", "");
            auto& house = scene.createObject(houseName);
            house.transform.setScale(glm::vec3(0.04f));
            
            house.transform.setEulerAngles(0.0f, 0.0f, 0.0f);

            auto& meshRenderer = house.addComponent<MeshRenderer>();
            meshRenderer.mesh = assets.getMeshHandle("desert_house");
            const Handle<Material> houseMaterial = assets.getMaterialHandle("desertHouseMat");
            meshRenderer.material = houseMaterial.valid() ? houseMaterial : assets.getDefaultMaterial();

            auto& collider = house.addComponent<BoxCollider>();
            collider.isTrigger = false;
            if (auto* mesh = assets.getMesh(meshRenderer.mesh))
            {
                const auto bounds = mesh->getBBox();
                collider.center = 0.5f * (bounds.max + bounds.min);
                collider.size = 0.5f * (bounds.max - bounds.min);
                collider.size.x *= 0.8f; // Adjust collider size to better fit house shape
                collider.size.z *= 0.8f; // Adjust collider size to better fit house shape
            }
            else
            {
                collider.center = glm::vec3(0.0f);
                collider.size = glm::vec3(1.0f);
            }
            collider.rebuild();

            auto& rigidBody = house.addComponent<RigidBody>();
            rigidBody.setBodyType(RigidBody::BodyType::Static);
            rigidBody.friction = 1.0f;

            selectedObject = &house;
        }

        

        ImGui::SeparatorText("Mushroom Forest");

        

        if (ImGui::Button("Mushroom")) {
            const std::string mushroomName = makeUniqueName(scene, "Mushroom", "");
            auto& mushroom = scene.createObject(mushroomName);
            mushroom.transform.setScale(glm::vec3(0.1f));
            
            mushroom.transform.setEulerAngles(0.0f, 0.0f, 0.0f);

            auto& meshRenderer = mushroom.addComponent<MeshRenderer>();
            meshRenderer.mesh = assets.getMeshHandle("mushroom");
            const Handle<Material> mushroomMaterial = assets.getMaterialHandle("mushroomMat");
            meshRenderer.material = mushroomMaterial.valid() ? mushroomMaterial : assets.getDefaultMaterial();

            auto& collider = mushroom.addComponent<BoxCollider>();
            collider.isTrigger = false;
            if (auto* mesh = assets.getMesh(meshRenderer.mesh))
            {
                const auto bounds = mesh->getBBox();
                collider.center = 0.5f * (bounds.max + bounds.min);
                collider.size = 0.5f * (bounds.max - bounds.min);
                collider.size.x *= 0.3f; // Adjust collider height to better fit mushroom shape
                collider.size.z *= 0.3f; // Adjust collider height to better fit mushroom shape
            }
            else
            {
                collider.center = glm::vec3(0.0f);
                collider.size = glm::vec3(1.0f);
            }
            collider.rebuild();

            auto& rigidBody = mushroom.addComponent<RigidBody>();
            rigidBody.setBodyType(RigidBody::BodyType::Static);
            rigidBody.friction = 1.0f;

            selectedObject = &mushroom;
        }

        ImGui::SeparatorText("Collectables");
        if (ImGui::Button("Star"))
        {
            const std::string starName = makeUniqueName(scene, "Star", "");
            auto& star = scene.createObject(starName);
            star.transform.setScale(glm::vec3(0.5f));

            auto& meshRenderer = star.addComponent<MeshRenderer>();
            meshRenderer.mesh = assets.getMeshHandle("gem");
            meshRenderer.material = assets.getDefaultMaterial();

            auto& collider = star.addComponent<BoxCollider>();
            if (auto* mesh = assets.getMesh(meshRenderer.mesh))
            {
                const auto bounds = mesh->getBBox();
                collider.center = 0.5f * (bounds.max + bounds.min);
                glm::vec3 halfExtents = 0.5f * (bounds.max - bounds.min);
                collider.size = halfExtents;
            }
            else
            {
                collider.center = glm::vec3(0.0f);
                collider.size = glm::vec3(1.0f);
            }
            collider.rebuild();
            collider.isTrigger = true;

            auto& collectable = star.addComponent<Collectable>();
            collectable.type = Collectable::Type::Cyan;
            Handle<engine::Material> cyanMat = assets.getMaterialHandle("cyanMat");
            if (cyanMat.valid())
            {
                meshRenderer.material = cyanMat;
            }
            collectable.defaultMat = meshRenderer.material;
            collectable.collectedMat = assets.getDefaultMaterial();

            //add slight floating animation to make it more visually distinct
            auto& animatedVelocity = star.addComponent<AnimatedVelocity>();
            animatedVelocity.linearAmplitude.y = 0.1f;
            animatedVelocity.frequency = 0.5f;

            selectedObject = &star;
        }
        ImGui::SameLine();
        if (ImGui::Button("speedBoost"))
        {
            const std::string speedBoostName = makeUniqueName(scene, "speedBoost", "");
            auto& speedBoost = scene.createObject(speedBoostName);
            speedBoost.transform.setScale(glm::vec3(0.31f, 0.21f, 0.5f));

            auto& meshRenderer = speedBoost.addComponent<MeshRenderer>();
            meshRenderer.mesh = assets.getMeshHandle("speedBoost");
            meshRenderer.material = assets.getMaterialHandle("redMat");
            if (!meshRenderer.material.valid())
            {
                meshRenderer.material = assets.getDefaultMaterial();
            }

            auto& collider = speedBoost.addComponent<BoxCollider>();
            collider.size = glm::vec3(1.0f);
            collider.isTrigger = true;

            auto& collectable = speedBoost.addComponent<Collectable>();
            collectable.type = Collectable::Type::speedBoost;
            collectable.defaultMat = meshRenderer.material;
            collectable.collectedMat = assets.getDefaultMaterial();

            selectedObject = &speedBoost;
        }

        ImGui::SameLine();
        if (ImGui::Button("JumpBoost"))
        {
            const std::string jumpBoostName = makeUniqueName(scene, "JumpBoost", "");
            auto& jumpBoost = scene.createObject(jumpBoostName);
            jumpBoost.transform.setScale(glm::vec3(0.110f, 0.310f, 0.5f));

            auto& meshRenderer = jumpBoost.addComponent<MeshRenderer>();
            meshRenderer.mesh = assets.getMeshHandle("JumpBoost");
            meshRenderer.material = assets.getMaterialHandle("blueMat");
            if (!meshRenderer.material.valid())
            {
                meshRenderer.material = assets.getDefaultMaterial();
            }

            auto& collider = jumpBoost.addComponent<BoxCollider>();
            collider.size = glm::vec3(1.0f);
            collider.isTrigger = true;

            auto& collectable = jumpBoost.addComponent<Collectable>();
            collectable.type = Collectable::Type::JumpBoost;
            collectable.defaultMat = meshRenderer.material;
            collectable.collectedMat = assets.getDefaultMaterial();

            selectedObject = &jumpBoost;
        }

        

        ImGui::Separator();

        if (selectedObject)
        {
            if (ImGui::Button("Clear Selection"))
            {
                selectedObject = nullptr;
            }

            if (ImGui::Button("Duplicate Selection"))
            {
                const std::string copyName = makeUniqueName(scene, selectedObject->name, "_Copy");
                auto& newObject = scene.createObject(copyName);
                newObject.transform.setPosition(selectedObject->transform.getPosition() + glm::vec3(1.0f, 0.0f, 0.0f));
                newObject.transform.setRotation(selectedObject->transform.getRotation());
                newObject.transform.setScale(selectedObject->transform.getScale());

                if (auto* meshRenderer = selectedObject->getComponent<MeshRenderer>())
                {
                    auto& newMeshRenderer = newObject.addComponent<MeshRenderer>();
                    newMeshRenderer.mesh = meshRenderer->mesh;
                    newMeshRenderer.material = meshRenderer->material;
                }

                if (auto* boxCollider = selectedObject->getComponent<BoxCollider>())
                {
                    auto& newBoxCollider = newObject.addComponent<BoxCollider>();
                    newBoxCollider.center = boxCollider->center;
                    newBoxCollider.size = boxCollider->size;
                    newBoxCollider.isTrigger = boxCollider->isTrigger;
                    newBoxCollider.rebuild();
                }

                if (auto* sphereCollider = selectedObject->getComponent<SphereCollider>())
                {
                    auto& newSphereCollider = newObject.addComponent<SphereCollider>();
                    newSphereCollider.center = sphereCollider->center;
                    newSphereCollider.radius = sphereCollider->radius;
                    newSphereCollider.isTrigger = sphereCollider->isTrigger;
                }

                if (auto* collectable = selectedObject->getComponent<Collectable>())
                {
                    auto& newCollectable = newObject.addComponent<Collectable>();
                    newCollectable.type = collectable->type;
                    newCollectable.defaultMat = collectable->defaultMat;
                    newCollectable.collectedMat = collectable->collectedMat;
                }

                if (auto* animatedVelocity = selectedObject->getComponent<AnimatedVelocity>())
                {
                    auto& newAnimatedVelocity = newObject.addComponent<AnimatedVelocity>();
                    newAnimatedVelocity.enabled = animatedVelocity->enabled;
                    newAnimatedVelocity.useLocalSpace = animatedVelocity->useLocalSpace;
                    newAnimatedVelocity.savedPosition = newObject.transform.getPosition();
                    newAnimatedVelocity.linearBase = animatedVelocity->linearBase;
                    newAnimatedVelocity.linearAmplitude = animatedVelocity->linearAmplitude;
                    newAnimatedVelocity.angularBase = animatedVelocity->angularBase;
                    newAnimatedVelocity.angularAmplitude = animatedVelocity->angularAmplitude;
                    newAnimatedVelocity.waveMode = animatedVelocity->waveMode;
                    newAnimatedVelocity.frequency = animatedVelocity->frequency;
                    newAnimatedVelocity.phase = animatedVelocity->phase;
                    newAnimatedVelocity.timeScale = animatedVelocity->timeScale;
                }

                selectedObject = &newObject;
            }

            if (ImGui::Button("Delete Object"))
            {
                Object* toDelete = selectedObject;
                selectedObject = nullptr;
                deleteObject(scene, toDelete);
            }
        }

        ImGui::Separator();
        ImGui::Text("Objects:");

        const auto& objects = scene.getObjects();
        if (selectedObject)
        {
            if (!isTrackedEditorObject(objects, selectedObject, initialObjectCount))
            {
                selectedObject = nullptr;
            }
        }

        for (std::size_t i = initialObjectCount; i < objects.size(); ++i)
        {
            const auto& object = objects[i];
            if (!object || object->transform.getParent())
            {
                continue;
            }

            drawHierarchyEntry(object.get(), selectedObject, objects, initialObjectCount);
        }
    }
}
