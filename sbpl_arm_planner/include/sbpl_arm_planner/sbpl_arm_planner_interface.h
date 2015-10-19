/*
 * Copyright (c) 2009, Willow Garage, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Willow Garage, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/** \author Benjamin Cohen */

#ifndef _SBPL_ARM_PLANNER_INTERFACE_H_
#define _SBPL_ARM_PLANNER_INTERFACE_H_

#include <iostream>
#include <map>
#include <ros/ros.h>
#include <tf/transform_datatypes.h>
#include <kdl/frames.hpp>
#include <angles/angles.h>
#include <leatherman/utils.h>
#include <leatherman/print.h>
#include <leatherman/viz.h>
#include <sbpl/planners/araplanner.h>
#include <sbpl_arm_planner/environment_robarm3d.h>
#include <sbpl_manipulation_components/post_processing.h>
#include <moveit/distance_field/propagation_distance_field.h>
#include <geometry_msgs/Pose.h>
#include <moveit_msgs/GetMotionPlan.h>
#include <moveit_msgs/PlanningScene.h>
#include <trajectory_msgs/JointTrajectory.h>

namespace sbpl_arm_planner{

class SBPLArmPlannerInterface
{
  public:

    SBPLArmPlannerInterface(
        RobotModel *rmodel,
        CollisionChecker *cc,
        ActionSet* as,
        distance_field::PropagationDistanceField* df);

    ~SBPLArmPlannerInterface();

    bool init();

    bool getParams();

    virtual bool planKinematicPath(const moveit_msgs::GetMotionPlan::Request &req, moveit_msgs::GetMotionPlan::Response &res);

    virtual bool solve(const moveit_msgs::PlanningSceneConstPtr& planning_scene, const moveit_msgs::GetMotionPlan::Request &req, moveit_msgs::GetMotionPlan::Response &res);

    bool canServiceRequest(const moveit_msgs::GetMotionPlan::Request &req);

    /// @brief Return planning statistics from the last call to solve.
    ///
    /// Possible keys to statistics include:
    ///     "initial solution planning time"
    ///     "initial epsilon"
    ///     "initial solution expansions"
    ///     "final epsilon planning time"
    ///     "final epsilon"
    ///     "solution epsilon"
    ///     "expansions"
    ///     "solution cost"
    ///
    /// @return The statistics
    virtual std::map<std::string, double>  getPlannerStats();

    visualization_msgs::MarkerArray getVisualization(std::string type);

    visualization_msgs::MarkerArray getCollisionModelTrajectoryMarker();

  protected:

    ros::NodeHandle nh_;

    /* params */
    bool planner_initialized_;
    int num_joints_;
    int solution_cost_;

    /* planner & environment */
    MDPConfig mdp_cfg_;
    SBPLPlanner *planner_;
    sbpl_arm_planner::EnvironmentROBARM3D *sbpl_arm_env_;
    sbpl_arm_planner::CollisionChecker *cc_;
    sbpl_arm_planner::OccupancyGrid *grid_;
    sbpl_arm_planner::RobotModel *rm_;
    sbpl_arm_planner::ActionSet *as_;
    sbpl_arm_planner::PlanningParams *prm_;
    distance_field::PropagationDistanceField* df_;

    moveit_msgs::MotionPlanRequest req_;
    moveit_msgs::GetMotionPlan::Response res_;
    moveit_msgs::PlanningScene pscene_;

    /** \brief Initialize the SBPL planner and the sbpl_arm_planner environment */
    virtual bool initializePlannerAndEnvironment();

    /** \brief Set start configuration */
    virtual bool setStart(const sensor_msgs::JointState &state);

    /** \brief Set goal(s) */
    virtual bool setGoalPosition(const moveit_msgs::Constraints &goals);

    virtual bool setGoalConfiguration(const moveit_msgs::Constraints& goal_constraints); //use this to set a 7dof goal!

    /** \brief Plan a path to a cartesian goal(s) */
    virtual bool planToPosition(const moveit_msgs::GetMotionPlan::Request &req, moveit_msgs::GetMotionPlan::Response &res);
    virtual bool planToConfiguration(const moveit_msgs::GetMotionPlan::Request &req, moveit_msgs::GetMotionPlan::Response &res);

    /** \brief Retrieve plan from sbpl */
    virtual bool plan(trajectory_msgs::JointTrajectory &traj);

    void extractGoalPoseFromGoalConstraints(const moveit_msgs::Constraints& goal_constraints,
                                            geometry_msgs::Pose& goal_pose_out) const;

    // extract tolerance as an array of 6 doubles: x, y, z, roll, pitch, yaw
    void extractGoalToleranceFromGoalConstraints(const moveit_msgs::Constraints& goal_constraints, double* tolerance_out);
};

}

#endif
