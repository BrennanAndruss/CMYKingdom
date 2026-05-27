#include "editor/Editor.h"
#include "editor/EditorObjectTools.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sys/stat.h>
#include <string>
#include <sstream>
#include <vector>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include "core/AppConfig.h"
#include "core/Time.h"
#include "renderer/resources/Material.h"
#include "resources/AssetManager.h"
#include "scene/Scene.h"
#include "scene/components/Components.h"
#include "systems/Collectable.h"

namespace engine
{
    namespace
    {
        std::string getSceneFilePath(const engine::AppConfig& config)
        {
            return config.assetsDir + "scenes/scene_objects.txt";
        }
    }

    static float getFPS()
    {
        static float frameCount = 0.0f;
        static float timeAccumulator = 0.0f;
        static int updatesPerSecond = 8;
        static float updateTime = 1.0f / (float)updatesPerSecond;
        static float fpsDisplay = 0.0f;

        frameCount += 1.0f;
        timeAccumulator += Time::deltaTime();

        // Update the display metrics only twice a second
        if (timeAccumulator >= updateTime)
        {
            fpsDisplay = frameCount / timeAccumulator;
            frameCount = 0.0f;
            timeAccumulator = 0.0f;
        }

        return fpsDisplay;
    }

    void Editor::initialize(GLFWwindow* windowHandle)
    {
        if (_initialized)
        {
            return;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForOpenGL(windowHandle, true);
        ImGui_ImplOpenGL3_Init("#version 150");

        _initialized = true;
    }

    void Editor::shutdown()
    {
        if (!_initialized)
        {
            return;
        }

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        _initialized = false;
    }

    void Editor::beginFrame()
    {
        if (!_initialized)
        {
            return;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void Editor::draw(Scene& scene, AssetManager& assets, const AppConfig& config)
    {
        if (!_initialized)
        {
            return;
        }

        if (!_capturedInitialSceneState)
        {
            _initialObjectCount = scene.getObjects().size();
            _capturedInitialSceneState = true;
        }

        updateEditorSelectionFromMouse(scene, _initialObjectCount, _selectedObject);
        ImGui::Begin("Editor");
            
        if (ImGui::Button("Save Scene"))
        {
            writeObjectsToFile(scene.getObjects(), getSceneFilePath(config), _initialObjectCount, assets);
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Scene"))
        {
            readObjectsFromFile(getSceneFilePath(config), scene, assets);
        }
        ImGui::Separator();
        drawEditorObjectControls(scene, assets, _initialObjectCount, _selectedObject);

    

        ImGui::End();

        ImGui::Begin("Transform");
        if (_selectedObject)
        {
            ImGui::Text("Selected: %s", _selectedObject->name.c_str());
            ImGui::Separator();

            if (auto* editorCamera = scene.getMainCamera())
            {
                if (editorCamera->owner && ImGui::Button("Move To Editor Camera"))
                {
                    _selectedObject->transform.setPosition(editorCamera->owner->transform.getPosition());
                }
            }

            glm::vec3 position = _selectedObject->transform.getPosition();
            glm::vec3 rotation = _selectedObject->transform.getEulerAngles();
            glm::vec3 scale = _selectedObject->transform.getScale();

            if (ImGui::DragFloat3("Position", &position.x, 0.1f))
            {
                _selectedObject->transform.setPosition(position);
            }

            if (ImGui::DragFloat3("Rotation", &rotation.x, 1.0f))
            {
                _selectedObject->transform.setEulerAngles(rotation);
            }

            if (ImGui::DragFloat3("Scale", &scale.x, 0.1f, 0.01f, 100.0f))
            {
                _selectedObject->transform.setScale(scale);
            }

            const bool hasCollider = _selectedObject->getComponent<BoxCollider>() != nullptr;
            const bool hasRigidBody = _selectedObject->getComponent<RigidBody>() != nullptr;
            bool physicsCollisionChecked = hasCollider && hasRigidBody;
            ImGui::BeginDisabled(physicsCollisionChecked);
            if (ImGui::Checkbox("Physics Collision", &physicsCollisionChecked) && physicsCollisionChecked)
            {
                if (!hasCollider)
                {
                    _selectedObject->addComponent<BoxCollider>();
                }

                if (!hasRigidBody)
                {
                    _selectedObject->addComponent<RigidBody>();
                }
            }
            ImGui::EndDisabled();

            const bool hasCollectable = _selectedObject->getComponent<Collectable>() != nullptr;
            bool collectableChecked = hasCollectable;
            ImGui::BeginDisabled(hasCollectable);
            if (ImGui::Checkbox("Collectable", &collectableChecked) && collectableChecked)
            {
                if (!hasCollider)
                {
                    auto& collider = _selectedObject->addComponent<BoxCollider>();
                    collider.isTrigger = true;
                }

                auto& collectable = _selectedObject->addComponent<Collectable>();
                if (auto* meshRenderer = _selectedObject->getComponent<MeshRenderer>())
                {
                    collectable.defaultMat = meshRenderer->material;
                    collectable.collectedMat = assets.getDefaultMaterial();
                }
                else
                {
                    collectable.defaultMat = assets.getDefaultMaterial();
                    collectable.collectedMat = assets.getDefaultMaterial();
                }
            }
            ImGui::EndDisabled();

            // Collectable color selection (available when object has collectable component)
            if (auto* collectable = _selectedObject->getComponent<Collectable>())
            {
                ImGui::Indent();
                int selectedType = static_cast<int>(collectable->type);
                const char* collectableTypes[] = { "Cyan", "Magenta", "Yellow" };
                if (ImGui::Combo("Collectable Color", &selectedType, collectableTypes, IM_ARRAYSIZE(collectableTypes)))
                {
                    collectable->type = static_cast<Collectable::Type>(selectedType);
                    Handle<engine::Material> colorMat = assets.getDefaultMaterial();
                    if (auto* meshRenderer = _selectedObject->getComponent<MeshRenderer>())
                    {
                        switch (selectedType)
                        {
                        case 0: // Cyan
                            colorMat = assets.getMaterialHandle("cyanMat");
                            break;
                        case 1: // Magenta
                            colorMat = assets.getMaterialHandle("magentaMat");
                            break;
                        case 2: // Yellow
                            colorMat = assets.getMaterialHandle("yellowMat");
                            break;
                        default:
                            break;
                        }
                        
                        collectable->defaultMat = colorMat.valid() ? colorMat : assets.getDefaultMaterial();
                        meshRenderer->material = colorMat.valid() ? colorMat : assets.getDefaultMaterial();
                    }
                }
                ImGui::Unindent();
            }

            

            if (auto* rigidBody = _selectedObject->getComponent<RigidBody>())
            {
                const char* bodyTypes[] = { "Static", "Kinematic", "Dynamic" };
                int selectedType = static_cast<int>(rigidBody->getBodyType());
                if (ImGui::Combo("RigidBody Type", &selectedType, bodyTypes, IM_ARRAYSIZE(bodyTypes)))
                {
                    rigidBody->setBodyType(static_cast<RigidBody::BodyType>(selectedType));
                }

                if (rigidBody->getBodyType() == RigidBody::BodyType::Dynamic)
                {
                    ImGui::DragFloat("Mass", &rigidBody->mass, 0.1f, 0.0f, 1000.0f);
                }
            }
            
        }
        else
        {
            ImGui::TextDisabled("Select an object to edit its transform.");
        }
        ImGui::End();

        // Debugging
        float fpsDisplay = getFPS();

        ImGui::Begin("Debug");
        ImGui::Text("FPS: %.1f", fpsDisplay);
        ImGui::Text("Frame Time: %.2f ms", (fpsDisplay > 0.0f) ? (1000.0f / fpsDisplay) : 0.0f);
        ImGui::End();
    }

    void Editor::endFrame()
    {
        if (!_initialized)
        {
            return;
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    bool Editor::writeObjectsToFile(const std::vector<std::unique_ptr<Object>>& objects,
        const std::string& filename,
        std::size_t startIndex,
        const AssetManager& assets)
    {
        try
        {
            const std::string tmpPath = filename + ".tmp";

            std::ofstream file(tmpPath, std::ios::binary);
            if (!file.is_open())
            {
                std::cerr << "Failed to open temp file for writing: " << tmpPath << std::endl;
                return false;
            }

            for (std::size_t i = startIndex; i < objects.size(); ++i)
            {
                const auto& object = objects[i];
                file << "Object: " << object->name << "\n";
                // transforms
                file << "  Position: " << object->transform.getPosition().x << ", "
                     << object->transform.getPosition().y << ", "
                     << object->transform.getPosition().z << "\n";
                file << "  Rotation: " << object->transform.getEulerAngles().x << ", "
                     << object->transform.getEulerAngles().y << ", "
                     << object->transform.getEulerAngles().z << "\n";
                file << "  Scale: " << object->transform.getScale().x << ", "
                     << object->transform.getScale().y << ", "
                     << object->transform.getScale().z << "\n";
                // mesh & material
                if (auto* meshRenderer = object->getComponent<MeshRenderer>())
                {                    
                    file << "  Mesh: " << (meshRenderer->mesh.valid() ? std::to_string(meshRenderer->mesh.index) : std::string("invalid")) << "\n";
                    const std::string materialName = assets.getMaterialName(meshRenderer->material);
                    file << "  Material: " << (!materialName.empty() ? materialName : std::string("invalid")) << "\n";
                    if (auto* material = assets.getMaterial(meshRenderer->material))
                    {
                        const std::string difTexName = assets.getTextureName(material->difTex);
                        const std::string specTexName = assets.getTextureName(material->specTex);
                        file << "  DiffuseTexture: " << (!difTexName.empty() ? difTexName : std::string("invalid")) << "\n";
                        file << "  SpecularTexture: " << (!specTexName.empty() ? specTexName : std::string("invalid")) << "\n";
                    }
                }
                if (auto* collider = object->getComponent<BoxCollider>())
                {
                    file << "  BoxCollider: size=" << collider->size.x << "," << collider->size.y << "," << collider->size.z
                         << " isTrigger=" << collider->isTrigger << "\n";
                }
                if (auto* rigidBody = object->getComponent<RigidBody>())
                {
                    file << "  RigidBody: type=" << static_cast<int>(rigidBody->getBodyType()) << " mass=" << rigidBody->mass << "\n";
                }
                if (auto* collectable = object->getComponent<Collectable>())
                {
                    file << "  Collectable: type=" << static_cast<int>(collectable->type) << "\n";
                }
                file << "\n";
                if (!file.good())
                {
                    std::cerr << "Write error while writing to temp file: " << tmpPath << std::endl;
                    file.close();
                    std::remove(tmpPath.c_str());
                    return false;
                }
            }

            file.flush();
            if (!file)
            {
                std::cerr << "Flush failed for temp file: " << tmpPath << std::endl;
                file.close();
                std::remove(tmpPath.c_str());
                return false;
            }

            file.close();

            if (std::rename(tmpPath.c_str(), filename.c_str()) != 0)
            {
                std::cerr << "Failed to rename temp file to target: " << filename << std::endl;
                std::remove(tmpPath.c_str());
                return false;
            }

            return true;
        }
        catch (const std::exception& ex)
        {
            std::cerr << "Exception writing objects to file: " << ex.what() << std::endl;
            return false;
        }
    }

    bool Editor::readObjectsFromFile(const std::string& filename, Scene& scene, AssetManager& assets)
    {
        try
        {
            std::ifstream file(filename);
            if (!file.is_open())
            {
                std::cerr << "Failed to open file for reading: " << filename << std::endl;
                return false;
            }

            auto parseVec3 = [](const std::string& value) -> glm::vec3
            {
                std::stringstream stream(value);
                float x = 0.0f;
                float y = 0.0f;
                float z = 0.0f;
                char comma = ',';
                stream >> x >> comma >> y >> comma >> z;
                return glm::vec3(x, y, z);
            };

            std::string line;
            const Handle<Material> defaultMaterial = assets.getDefaultMaterial();
            struct LoadedObject
            {
                std::string name;
                glm::vec3 position{0.0f};
                glm::vec3 rotation{0.0f};
                glm::vec3 scale{1.0f};

                bool hasMesh = false;
                Handle<Mesh> mesh;

                bool hasMaterial = false;
                Handle<Material> material;

                bool hasBoxCollider = false;
                glm::vec3 boxColliderSize{1.0f};
                bool boxColliderIsTrigger = false;

                bool hasRigidBody = false;
                RigidBody::BodyType rigidBodyType = RigidBody::BodyType::Static;
                float rigidBodyMass = 1.0f;

                bool isCollectable = false;
                int collectableType = 0;
            };

            auto flushObject = [&](const LoadedObject& data)
            {
                Object& object = scene.createObject(data.name);
                object.transform.setPosition(data.position);
                object.transform.setEulerAngles(data.rotation);
                object.transform.setScale(data.scale);

                auto getCollectableMaterial = [&assets](int collectableType) -> Handle<Material>
                {
                    switch (collectableType)
                    {
                    case 0:
                        return assets.getMaterialHandle("cyanMat");
                    case 1:
                        return assets.getMaterialHandle("magentaMat");
                    case 2:
                        return assets.getMaterialHandle("yellowMat");
                    default:
                        return assets.getDefaultMaterial();
                    }
                };

                if (data.hasMesh || data.hasMaterial)
                {
                    auto& meshRenderer = object.addComponent<MeshRenderer>();
                    if (data.hasMesh)
                    {
                        meshRenderer.mesh = data.mesh;
                    }
                    if (data.hasMaterial)
                    {
                        meshRenderer.material = data.material;
                    }
                    else
                    {
                        meshRenderer.material = defaultMaterial;
                    }
                }

                if (data.hasBoxCollider)
                {
                    auto& collider = object.addComponent<BoxCollider>();
                    collider.size = data.boxColliderSize;
                    collider.isTrigger = data.boxColliderIsTrigger;
                    collider.rebuild();
                }

                if (data.hasRigidBody)
                {
                    auto& rigidBody = object.addComponent<RigidBody>();
                    rigidBody.setBodyType(data.rigidBodyType);
                    rigidBody.mass = data.rigidBodyMass;
                }
                if (data.isCollectable)
                {
                    auto& collectable = object.addComponent<Collectable>();
                    collectable.type = static_cast<Collectable::Type>(data.collectableType);

                    if (auto* meshRenderer = object.getComponent<MeshRenderer>())
                    {
                        const Handle<Material> collectableMaterial = getCollectableMaterial(data.collectableType);
                        if (collectableMaterial.valid())
                        {
                            meshRenderer->material = collectableMaterial;
                        }
                    }
                }
            };

            std::unique_ptr<LoadedObject> currentObject;

            while (std::getline(file, line))
            {
                if (line.rfind("Object: ", 0) == 0)
                {
                    if (currentObject)
                    {
                        flushObject(*currentObject);
                    }

                    currentObject = std::make_unique<LoadedObject>();
                    currentObject->name = line.substr(8);
                    continue;
                }

                if (!currentObject)
                {
                    continue;
                }

                if (line.rfind("  Position: ", 0) == 0)
                {
                    currentObject->position = parseVec3(line.substr(12));
                }
                else if (line.rfind("  Rotation: ", 0) == 0)
                {
                    currentObject->rotation = parseVec3(line.substr(12));
                }
                else if (line.rfind("  Scale: ", 0) == 0)
                {
                    currentObject->scale = parseVec3(line.substr(9));
                }
                else if (line.rfind("  Mesh: ", 0) == 0)
                {
                    const std::string value = line.substr(8);
                    if (value != "invalid")
                    {
                        currentObject->mesh.index = static_cast<std::size_t>(std::stoull(value));
                        if (assets.getMesh(currentObject->mesh))
                        {
                            currentObject->hasMesh = true;
                        }
                    }
                }
                else if (line.rfind("  Material: ", 0) == 0)
                {
                    const std::string value = line.substr(12);
                    if (value != "invalid")
                    {
                        Handle<Material> materialHandle = assets.getMaterialHandle(value);
                        if (!materialHandle.valid())
                        {
                            try
                            {
                                materialHandle.index = static_cast<std::size_t>(std::stoull(value));
                            }
                            catch (const std::exception&)
                            {
                                materialHandle = {};
                            }
                        }

                        if (assets.getMaterial(materialHandle))
                        {
                            currentObject->material = materialHandle;
                            currentObject->hasMaterial = true;
                        }
                    }
                }
                else if (line.rfind("  BoxCollider: ", 0) == 0)
                {
                    const std::string value = line.substr(15);
                    const std::size_t sizePrefix = value.find("size=");
                    const std::size_t triggerPrefix = value.find(" isTrigger=");
                    if (sizePrefix == std::string::npos)
                    {
                        continue;
                    }

                    const std::string sizeValue = value.substr(sizePrefix + 5, triggerPrefix == std::string::npos
                        ? std::string::npos
                        : triggerPrefix - (sizePrefix + 5));
                    std::stringstream stream(sizeValue);
                    float x = 0.0f;
                    float y = 0.0f;
                    float z = 0.0f;
                    char comma = ',';
                    stream >> x >> comma >> y >> comma >> z;
                    currentObject->hasBoxCollider = true;
                    currentObject->boxColliderSize = glm::vec3(x, y, z);

                    if (triggerPrefix != std::string::npos)
                    {
                        const std::string triggerValue = value.substr(triggerPrefix + 11);
                        currentObject->boxColliderIsTrigger = triggerValue == "1" || triggerValue == "true";
                    }
                }
                else if (line.rfind("  RigidBody: ", 0) == 0)
                {
                    const std::string value = line.substr(13);
                    const std::size_t typePrefix = value.find("type=");
                    const std::size_t massPrefix = value.find(" mass=");
                    if (typePrefix == std::string::npos || massPrefix == std::string::npos)
                    {
                        continue;
                    }

                    const std::string typeValue = value.substr(typePrefix + 5, massPrefix - (typePrefix + 5));
                    const std::string massValue = value.substr(massPrefix + 6);
                    int bodyType = 0;
                    float mass = 0.0f;
                    bodyType = std::stoi(typeValue);
                    mass = std::stof(massValue);
                    currentObject->hasRigidBody = true;
                    currentObject->rigidBodyType = static_cast<RigidBody::BodyType>(bodyType);
                    currentObject->rigidBodyMass = mass;
                } else if (line.rfind("  Collectable: ", 0) == 0)
                {
                    const std::string value = line.substr(15);
                    const std::size_t typePrefix = value.find("type=");
                    if (typePrefix != std::string::npos)
                    {
                        const std::string typeValue = value.substr(typePrefix + 5);
                        try
                        {
                            currentObject->collectableType = std::stoi(typeValue);
                        }
                        catch (const std::exception&)
                        {
                            currentObject->collectableType = 0;
                        }
                    }
                    currentObject->isCollectable = true;
                }
            }

            if (currentObject)
            {
                flushObject(*currentObject);
            }

            return true;
        }
        catch (const std::exception& ex)
        {
            std::cerr << "Exception reading objects from file: " << ex.what() << std::endl;
            return false;
        }
    }
}


