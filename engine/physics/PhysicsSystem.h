#pragma once

#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>
#include <BulletDynamics/Character/btKinematicCharacterController.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <functional>
#include <unordered_map>
#include <set>
#include <memory>

namespace engine
{
	class PhysicsSystem
	{
	public:
		PhysicsSystem();
		~PhysicsSystem() = default;

		void update(float deltaTime);

		
		btRigidBody* createBody(btCollisionShape* shape, glm::vec3 position, 
			float mass, bool isTrigger = false);
		btCollisionObject* createCollisionObject(btCollisionShape* shape, glm::vec3 position, 
			bool isTrigger = false);

		void addBody(btRigidBody* body) { _world->addRigidBody(body); }
		void addCollisionObject(btCollisionObject* object,
			int group = btBroadphaseProxy::DefaultFilter, int mask = btBroadphaseProxy::AllFilter);
		void addAction(btActionInterface* action) { _world->addAction(action); }

		void removeBody(btRigidBody* body);
		void removeCollisionObject(btCollisionObject* object);
		
		// Collision callbacks
		using CollisionCallback = std::function<void(btCollisionObject* other)>;
		void registerCallback(btCollisionObject* object, CollisionCallback callback);
		void unregisterCallback(btCollisionObject* object);
		btDiscreteDynamicsWorld* getWorld() const { return _world.get(); }

		// Type conversion helpers
		static glm::vec3 toGlm(const btVector3& v) { return glm::vec3(v.x(), v.y(), v.z()); }
		static btVector3 toBullet(glm::vec3 v) { return btVector3(v.x, v.y, v.z); }

		static glm::quat toGlm(const btQuaternion& q) { return glm::quat(q.x(), q.y(), q.z(), q.w()); }
		static btQuaternion toBullet(glm::quat q) { return btQuaternion(q.x, q.y, q.z, q.w);}

	private:
		// Bullet world components
		std::unique_ptr<btDefaultCollisionConfiguration> _collisionConfig;
		std::unique_ptr<btCollisionDispatcher> _dispatcher;
		std::unique_ptr<btBroadphaseInterface> _broadphase;
		std::unique_ptr<btSequentialImpulseConstraintSolver> _solver;
		std::unique_ptr<btDiscreteDynamicsWorld> _world;

		// Shape and motion states
		std::vector<std::unique_ptr<btCollisionShape>> _shapes;
		std::vector<std::unique_ptr<btMotionState>> _motionStates;

		std::unordered_map<btCollisionObject*, CollisionCallback> _callbacks;
		std::set<std::pair<btCollisionObject*, btCollisionObject*>> _activeContacts;

		void checkCollisions();
	};
}