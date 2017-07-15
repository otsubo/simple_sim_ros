/**
  Copyright 2016 Lucas Walter

  First step:

  Create a bullet simulation, default with an infinite plane.

  ros messages or service calls create body primitives in the simulation.
  Send the position and attitude of each body to tf.

  Maybe publish Markers for rviz, but could leave that to client- ideally 
  no visualization at all would be done here.

  No joints initially, or actuation, or sensors.

  Publish to clock if use_sim_time is set, otherwise try to keep up with
  wall clock time as much as possible (maybe issue a warning if the sim
  gets more than 5 or 10 seconds behind current time).
*/

#ifndef BULLET_SERVER_BULLET_SERVER_H
#define BULLET_SERVER_BULLET_SERVER_H

#include <boost/functional/hash.hpp>
#include <bullet/btBulletDynamicsCommon.h>
#include <bullet/BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h>
#include <bullet/BulletSoftBody/btSoftBodyRigidBodyCollisionConfiguration.h>
#include <bullet/BulletSoftBody/btSoftRigidDynamicsWorld.h>
#include <bullet_server/AddBody.h>
#include <bullet_server/AddCompound.h>
#include <bullet_server/AddConstraint.h>
#include <bullet_server/AddHeightfield.h>
#include <bullet_server/AddImpulse.h>
#include <bullet_server/Anchor.h>
#include <bullet_server/Body.h>
#include <bullet_server/Constraint.h>
#include <bullet_server/Heightfield.h>
#include <bullet_server/Impulse.h>
#include <bullet_server/SoftBody.h>
#include <bullet_server/SoftConfig.h>
#include <bullet_server/Material.h>
#include <bullet_server/Node.h>
#include <bullet_server/Link.h>
#include <bullet_server/Face.h>
#include <bullet_server/Tetra.h>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <cv_bridge/cv_bridge.h>
#include <geometry_msgs/Pose.h>
#include <map>
#include <ros/ros.h>
#include <std_msgs/Float64.h>
#include <string>
#include <tf/transform_broadcaster.h>
#include <visualization_msgs/Marker.h>
#include <visualization_msgs/MarkerArray.h>

class Body;
class Constraint;
class SoftBody;

// TODO(lucasw) replace this with something better, numbers aren't random enough
// http://stackoverflow.com/questions/2535284/how-can-i-hash-a-string-to-an-int-using-c
unsigned short hash(const char *str);

class BulletServer
{
  ros::NodeHandle nh_;
  ros::ServiceServer add_compound_;
  bool addCompound(bullet_server::AddCompound::Request& req,
                   bullet_server::AddCompound::Response& res);
  ros::Subscriber body_sub_;
  void bodyCallback(const bullet_server::Body::ConstPtr& msg);
  void softBodyCallback(const bullet_server::SoftBody::ConstPtr& msg);
  ros::Subscriber constraint_sub_;
  void constraintCallback(const bullet_server::Constraint::ConstPtr& msg);
  ros::Subscriber impulse_sub_;
  void impulseCallback(const bullet_server::Impulse::ConstPtr& msg);
  ros::Subscriber heightfield_sub_;
  void heightfieldCallback(const bullet_server::Heightfield::ConstPtr& msg);

  tf::TransformBroadcaster br_;
  float period_;
  // ros::Publisher marker_pub_;
  ros::Publisher marker_array_pub_;

  bool rigid_only_;
  btBroadphaseInterface* broadphase_;

  btDefaultCollisionConfiguration* collision_configuration_;
  btSoftBodyRigidBodyCollisionConfiguration* soft_rigid_collision_configuration_;

  btCollisionDispatcher* dispatcher_;
  // only used with opencl
  // btSoftBodySolver* soft_body_solver_;
  btSequentialImpulseConstraintSolver* solver_;

  btDiscreteDynamicsWorld* dynamics_world_;

  // TODO(lucasw) make this configurable by addCompound
  // instead of hard coded
  btCollisionShape* ground_shape_;
  btDefaultMotionState* ground_motion_state_;
  btRigidBody* ground_rigid_body_;

  btSoftBodyWorldInfo soft_body_world_info_;
  std::map<std::string, SoftBody*> soft_bodies_;
  std::map<std::string, Constraint*> constraints_;

  int init();
public:
  BulletServer();
  ~BulletServer();
  void update();
  void removeConstraint(const Constraint* constraint, const bool remove_from_bodies);
  std::map<std::string, Body*> bodies_;
};

class Body
{
  BulletServer* parent_;
  tf::TransformBroadcaster* br_;
  // ros::Publisher* marker_pub_;
  ros::Publisher* marker_array_pub_;
  visualization_msgs::MarkerArray marker_array_;

  // TODO(lucasw) put this in a child class
  // for triangle meshes
  btVector3* vertices_;
  int* indices_;
  btTriangleIndexVertexArray* index_vertex_arrays_;

  std::map<std::string, Constraint*> constraints_;
  btCollisionShape* shape_;
  btDefaultMotionState* motion_state_;

  btDiscreteDynamicsWorld* dynamics_world_;
  int state_;
public:
  Body(BulletServer* parent,
      const std::string name,
      unsigned int type,
      const float mass,
      geometry_msgs::Pose pose,
      geometry_msgs::Twist twist,
      geometry_msgs::Vector3 scale,
      btDiscreteDynamicsWorld* dynamics_world,
      tf::TransformBroadcaster* br,
      ros::Publisher* marker_array_pub_);
  // heightfield
  Body(BulletServer* parent,
    const std::string name,
    // unsigned int type,
    // geometry_msgs::Pose pose,
    // geometry_msgs::Vector3 scale,
    cv::Mat& image,
    const float resolution,
    const float height_scale,
    const bool flip_quad_edges,
    btDiscreteDynamicsWorld* dynamics_world,
    tf::TransformBroadcaster* br,
    ros::Publisher* marker_array_pub);
  ~Body();

  // keep track of constraints attached to this body
  void addConstraint(Constraint* constraint);
  void removeConstraint(const Constraint* constraint);
  const std::string name_;
  btRigidBody* rigid_body_;
  void update();
};

// TODO(lucasw) make a common base class
class SoftBody
{
  BulletServer* parent_;
  btSoftRigidDynamicsWorld* dynamics_world_;
  tf::TransformBroadcaster* br_;
  ros::Publisher* marker_array_pub_;
  visualization_msgs::MarkerArray marker_array_;
  const std::string name_;
public:
  SoftBody(BulletServer* parent,
      const std::string name,
      btSoftBodyWorldInfo* soft_body_world_info,
      const std::vector<bullet_server::Node>& nodes,
      const std::vector<bullet_server::Link>& links,
      const std::vector<bullet_server::Face>& faces,
      const std::vector<bullet_server::Tetra>& tetras,
      const std::vector<bullet_server::Material>& materials,
      const std::vector<bullet_server::Anchor>& anchors,
      const bullet_server::SoftConfig& config,
      btSoftRigidDynamicsWorld* dynamics_world,
      tf::TransformBroadcaster* br,
      ros::Publisher* marker_array_pub);
  ~SoftBody();
  void update();
  btSoftBody* soft_body_;
};

class Constraint
{
  ros::NodeHandle nh_;
  btTypedConstraint* constraint_;

  btDiscreteDynamicsWorld* dynamics_world_;
  ros::Publisher* marker_array_pub_;
  std::map<std::string, float> command_;
  std::map<std::string, ros::Publisher> pubs_;
  std::map<std::string, ros::Subscriber> subs_;
  visualization_msgs::MarkerArray marker_array_;
  float max_motor_impulse_;
  void commandCallback(const std_msgs::Float64::ConstPtr msg, const std::string motor_name);
public:
  Constraint(
      const std::string name,
      unsigned int type,
      Body* body_a,
      Body* body_b,
      geometry_msgs::Point pivot_in_a,
      geometry_msgs::Point pivot_in_b,
      geometry_msgs::Vector3 axis_in_a,
      geometry_msgs::Vector3 axis_in_b,
      const double lower_lin_lim,
      const double upper_lin_lim,
      const double lower_ang_lim,
      const double upper_ang_lim,
      const float max_motor_impulse,
      btDiscreteDynamicsWorld* dynamics_world,
      ros::Publisher* marker_array_pub);
  ~Constraint();

  const std::string name_;
  Body* body_a_;
  Body* body_b_;
  void update();
};

#endif  // BULLET_SERVER_BULLET_SERVER_H
