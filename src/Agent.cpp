#include "Agent.hpp"

Agent::Agent(int agentNo) {
  this->agentNo = agentNo;
  nh.getParam("/dorca/totalAgents", totalAgents);
  std::string s = "/uav" + std::to_string(agentNo + 1);
  local_pos_pub = nh.advertise<geometry_msgs::PoseStamped>(s+"/mavros/setpoint_position/local", 10);
  cmd_vel = nh.advertise<geometry_msgs::Twist>(s+"/mavros/setpoint_velocity/cmd_vel_unstamped", 0);
  state_sub = nh.subscribe<mavros_msgs::State>(s+"/mavros/state", 10, &Agent::state_cb, this);

  for (int i = 0; i < totalAgents; i++) {
     s = "/uav" + std::to_string(i+1);
     if (i != agentNo)
     vel_sub =  nh.subscribe<geometry_msgs::Twist>(s+"/mavros/setpoint_velocity/cmd_vel_unstamped", 1, boost::bind(&Agent::velocity_cb, this, _1, i));
  }

  is_update_goal = true;
  publishInitialPose();
}

Agent::~Agent() {}

void Agent::state_cb(const mavros_msgs::State::ConstPtr& msg) {
  current_state = *msg;
}

void Agent::velocity_cb(const geometry_msgs::Twist::ConstPtr& msg, int i) {
  qvel[i].linear.x = msg->linear.x;
  qvel[i].linear.y = msg->linear.y;
  qvel[i].linear.z = msg->linear.z;
}

void Agent::setArmAndModeTopics() {
  std::string s = "/uav" + std::to_string(agentNo + 1);
  arming_client = nh.serviceClient<mavros_msgs::CommandBool>(s+"/mavros/cmd/arming");
  set_mode_client = nh.serviceClient<mavros_msgs::SetMode>(s+"/mavros/set_mode");
}

void Agent::publishInitialPose() {
  geometry_msgs::PoseStamped pose;

  pose.pose.position.x = 0;
  pose.pose.position.y = 0;
  pose.pose.position.z = 0;

  for (int i = 100; ros::ok() && i > 0; --i) {
    local_pos_pub.publish(pose);
  }
}

void Agent::setAgentGoal() {
  agentGoal = RVO::Vector3(qpose[agentNo].pose.position.x, qpose[agentNo].pose.position.y, 5);
  std::cout << "Agent " << agentNo << " Goal : " << agentGoal << "\n";
}

void Agent::publishAgentVelocity(RVO::Vector3 vel) {
  geometry_msgs::Twist velo;
  velo.linear.x = vel.x();
  velo.linear.y = vel.y();
  velo.linear.z = vel.z();
  cmd_vel.publish(velo);
}

// Set current preffered velocity for the agents/obstacles.
RVO::Vector3 Agent::getPreferredVelocity() {
  RVO::Vector3 goalVector = agentGoal;
  RVO::Vector3 posVector = RVO::Vector3(qpose[agentNo].pose.position.x, qpose[agentNo].pose.position.y, qpose[agentNo].pose.position.z);
  goalVector = goalVector - posVector;

  if (RVO::absSq(goalVector) > 1.0f) {
      goalVector = RVO::normalize(goalVector);
  }

  return goalVector;
}

void Agent::reachedGoal(double radius) {
  // Construct the current position vector of the agent.
  RVO::Vector3 posVector = RVO::Vector3(qpose[agentNo].pose.position.x, qpose[agentNo].pose.position.y, qpose[agentNo].pose.position.z);
  // Check if agent reached its goal.
  if (RVO::absSq(posVector - agentGoal) < radius * radius) {
    //std::cout << "Agent " << agentNo  << "reached its goal.\n";
    if (is_update_goal) {
      agentGoal = RVO::Vector3(-qpose[agentNo].pose.position.x, -qpose[agentNo].pose.position.y, 5);
      is_update_goal = false;
    }
  }
}

bool Agent::arm() {
  if (current_state.mode != "OFFBOARD") {
    mavros_msgs::SetMode offb_set_mode;
    offb_set_mode.request.custom_mode = "OFFBOARD";
    if (set_mode_client.call(offb_set_mode) && offb_set_mode.response.mode_sent) {
      return true;
    }
  } else {
    if (!current_state.armed) {
      mavros_msgs::CommandBool arm_cmd;
      arm_cmd.request.value = true;
      if (arming_client.call(arm_cmd) && arm_cmd.response.success) {
        return true;
      }
    }
  }
  return false;
}