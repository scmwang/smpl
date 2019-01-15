#include "test_scenario.h"

// standard includes
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// system includes
#include <eigen_conversions/eigen_msg.h>
#include <smpl/distance_map/euclid_distance_map.h>
#include <smpl_urdf_robot_model/robot_state_visualization.h>

// project includes
#include "pr2_allowed_collision_pairs.h"

// Configuration of the planning model
struct RobotModelConfig
{
    std::string kinematics_frame;
    std::string chain_tip_link;
};

template <class T>
bool FindParam(const ros::NodeHandle& nh, const std::string& key, T& out)
{
    auto full_param = std::string();
    if (!nh.searchParam(key, full_param)) {
        ROS_WARN("Failed to find '%s' key on the param server", key.c_str());
        return false;
    }

    if (!nh.getParam(full_param, out)) {
        ROS_WARN("Failed to retrieve param '%s' from the param server", full_param.c_str());
        return false;
    }

    return true;
}

static
bool ReadRobotModelConfig(const ros::NodeHandle& nh, RobotModelConfig& config)
{
    if (!FindParam(nh, "robot_model/kinematics_frame", config.kinematics_frame)) return false;
    if (!FindParam(nh, "robot_model/chain_tip_link", config.chain_tip_link)) return false;
    return true;
}

static
bool ReadInitialConfiguration(
    ros::NodeHandle& nh,
    moveit_msgs::RobotState& state)
{
    // joint_state
    XmlRpc::XmlRpcValue xlist;
    if (FindParam(nh, "initial_configuration/joint_state", xlist)) {
        if (xlist.getType() != XmlRpc::XmlRpcValue::TypeArray) {
            ROS_WARN("initial_configuration/joint_state is not an array.");
        }

        if (xlist.size() > 0) {
            for (int i = 0; i < xlist.size(); ++i) {
                state.joint_state.name.push_back(std::string(xlist[i]["name"]));

                if (xlist[i]["position"].getType() == XmlRpc::XmlRpcValue::TypeDouble) {
                    state.joint_state.position.push_back(double(xlist[i]["position"]));
                } else {
                    ROS_DEBUG("Doubles in the yaml file have to contain decimal points. (Convert '0' to '0.0')");
                    if (xlist[i]["position"].getType() == XmlRpc::XmlRpcValue::TypeInt) {
                        int pos = xlist[i]["position"];
                        state.joint_state.position.push_back(double(pos));
                    }
                }
            }
        }
    } else {
        ROS_WARN("initial_configuration/joint_state is not on the param server.");
    }

    // multi_dof_joint_state
    if (FindParam(nh, "initial_configuration/multi_dof_joint_state", xlist)) {
        if (xlist.getType() == XmlRpc::XmlRpcValue::TypeArray) {
            if (xlist.size() != 0) {
                auto &multi_dof_joint_state = state.multi_dof_joint_state;
                multi_dof_joint_state.joint_names.resize(xlist.size());
                multi_dof_joint_state.transforms.resize(xlist.size());
                for (int i = 0; i < xlist.size(); ++i) {
                    multi_dof_joint_state.joint_names[i] = std::string(xlist[i]["joint_name"]);

                    Eigen::Quaterniond q;
                    smpl::angles::from_euler_zyx(
                            (double)xlist[i]["yaw"], (double)xlist[i]["pitch"], (double)xlist[i]["roll"], q);

                    geometry_msgs::Quaternion orientation;
                    tf::quaternionEigenToMsg(q, orientation);

                    multi_dof_joint_state.transforms[i].translation.x = xlist[i]["x"];
                    multi_dof_joint_state.transforms[i].translation.y = xlist[i]["y"];
                    multi_dof_joint_state.transforms[i].translation.z = xlist[i]["z"];
                    multi_dof_joint_state.transforms[i].rotation.w = orientation.w;
                    multi_dof_joint_state.transforms[i].rotation.x = orientation.x;
                    multi_dof_joint_state.transforms[i].rotation.y = orientation.y;
                    multi_dof_joint_state.transforms[i].rotation.z = orientation.z;
                }
            } else {
                ROS_WARN("initial_configuration/multi_dof_joint_state array is empty");
            }
        } else {
            ROS_WARN("initial_configuration/multi_dof_joint_state is not an array.");
        }
    }

    ROS_INFO("Read initial state containing %zu joints and %zu multi-dof joints", state.joint_state.name.size(), state.multi_dof_joint_state.joint_names.size());
    return true;
}

static
auto GetCollisionCube(
    const geometry_msgs::Pose& pose,
    std::vector<double>& dims,
    const std::string& frame_id,
    const std::string& id)
    -> moveit_msgs::CollisionObject
{
    auto object = moveit_msgs::CollisionObject();
    object.id = id;
    object.operation = moveit_msgs::CollisionObject::ADD;
    object.header.frame_id = frame_id;
    object.header.stamp = ros::Time::now();

    auto box_object = shape_msgs::SolidPrimitive();
    box_object.type = shape_msgs::SolidPrimitive::BOX;
    box_object.dimensions.resize(3);
    box_object.dimensions[0] = dims[0];
    box_object.dimensions[1] = dims[1];
    box_object.dimensions[2] = dims[2];

    object.primitives.push_back(box_object);
    object.primitive_poses.push_back(pose);
    return object;
}

static
auto GetCollisionCubes(
    std::vector<std::vector<double>>& objects,
    std::vector<std::string>& object_ids,
    const std::string& frame_id)
    -> std::vector<moveit_msgs::CollisionObject>
{
    auto objs = std::vector<moveit_msgs::CollisionObject>();
    auto dims = std::vector<double>(3, 0);
    auto pose = geometry_msgs::Pose();
    pose.orientation.x = 0;
    pose.orientation.y = 0;
    pose.orientation.z = 0;
    pose.orientation.w = 1;

    if (object_ids.size() != objects.size()) {
        ROS_INFO("object id list is not same length as object list. exiting.");
        return objs;
    }

    for (size_t i = 0; i < objects.size(); i++) {
        pose.position.x = objects[i][0];
        pose.position.y = objects[i][1];
        pose.position.z = objects[i][2];
        dims[0] = objects[i][3];
        dims[1] = objects[i][4];
        dims[2] = objects[i][5];

        objs.push_back(GetCollisionCube(pose, dims, frame_id, object_ids.at(i)));
    }
    return objs;
}

static
auto GetCollisionObjects(
    const std::string& filename,
    const std::string& frame_id)
    -> std::vector<moveit_msgs::CollisionObject>
{
    char sTemp[1024];
    int num_obs = 0;
    std::vector<std::string> object_ids;
    std::vector<std::vector<double> > objects;
    std::vector<moveit_msgs::CollisionObject> objs;

    FILE* fCfg = fopen(filename.c_str(), "r");

    if (fCfg == NULL) {
        ROS_INFO("ERROR: unable to open objects file. Exiting.\n");
        return objs;
    }

    // get number of objects
    if (fscanf(fCfg,"%s",sTemp) < 1) {
        printf("Parsed string has length < 1.\n");
    }

    num_obs = atoi(sTemp);

    ROS_INFO("%i objects in file",num_obs);

    //get {x y z dimx dimy dimz} for each object
    objects.resize(num_obs);
    object_ids.clear();
    for (int i = 0; i < num_obs; ++i) {
        if (fscanf(fCfg,"%s",sTemp) < 1) {
            printf("Parsed string has length < 1.\n");
        }
        object_ids.push_back(sTemp);

        objects[i].resize(6);
        for (int j=0; j < 6; ++j)
        {
            if (fscanf(fCfg,"%s",sTemp) < 1) {
                printf("Parsed string has length < 1.\n");
            }
            if (!feof(fCfg) && strlen(sTemp) != 0) {
                objects[i][j] = atof(sTemp);
            }
        }
    }

    return GetCollisionCubes(objects, object_ids, frame_id);
}

/////////////////////////
// Interface Functions //
/////////////////////////

TestScenarioBase::TestScenarioBase() :
    ph("~"),
    visualizer(nh, 100)
{
}

// Perform initialization of a Test Scenario that is independent of the
// specific RobotModel implementation being used.
static
bool InitTestScenarioPrePlanningModel(TestScenarioBase* scenario)
{
    smpl::viz::set_visualizer(&scenario->visualizer);
    ros::Duration(0.1).sleep(); // let the publisher setup

    ////////////////////
    // Load the scene //
    ////////////////////

    if (!ReadInitialConfiguration(scenario->ph, scenario->start_state)) {
        ROS_ERROR("Failed to get initial configuration.");
        return false;
    }

    return true;
}

static
auto GetRobotDescription(const ros::NodeHandle& nh) -> std::pair<std::string, bool>
{
    auto robot_description_key = "robot_description";
    auto robot_description = std::string();
    if (!FindParam(nh, robot_description_key, robot_description)) {
        return std::make_pair("", false);
    }
    return std::make_pair(std::move(robot_description), true);
}

static
bool InitTestScenarioPostPlanningModel(
    TestScenarioBase* scenario,
    smpl::RobotModel* planning_model)
{
    auto planning_frame = std::string();
    if (!FindParam(scenario->ph, "planning_frame", planning_frame)) {
        ROS_ERROR("Failed to retrieve param 'planning_frame' from the param server");
        return false;
    }
    ROS_INFO("planning frame = %s", planning_frame.c_str());

    ////////////////////
    // Load the Robot //
    ////////////////////

    // Robot description required to initialize collision checker and robot
    // model... TODO: we end up often reading this in twice, once to create the
    // collision model and again to create the planning model. Since we don't
    // know the planning model type, we don't know whether it requires the URDF
    // (the string, a ModelInterface, or the RobotCollisionModel). If we  defer
    // setting the planning group in the CollisionSpace, we can load the
    // ModelInterface, use it to initialize the CollisionSpace, the planning
    // model can use it, if possible, and we can then update the planning group
    // after the planning model is determined.
    auto robot_description = std::string();
    auto found = false;
    std::tie(robot_description, found) = GetRobotDescription(scenario->ph);
    if (!found) {
        return false;
    }

    ////////////////////////////////////////////////////////
    // Initialize the Collision Checker used for planning //
    ////////////////////////////////////////////////////////

    {
        ROS_INFO("Initialize Occupancy Grid");

        auto df_size_x = 3.0;
        auto df_size_y = 3.0;
        auto df_size_z = 3.0;
        auto df_res = 0.02;
        auto df_origin_x = -0.75;
        auto df_origin_y = -1.5;
        auto df_origin_z = 0.0;
        auto max_distance = 1.8;

        using DistanceMapType = smpl::EuclidDistanceMap;

        auto df = std::make_shared<DistanceMapType>(
                df_origin_x, df_origin_y, df_origin_z,
                df_size_x, df_size_y, df_size_z,
                df_res,
                max_distance);

        auto ref_counted = false;
        scenario->grid = smpl::OccupancyGrid(std::move(df), ref_counted);

        scenario->grid.setReferenceFrame(planning_frame);
        SV_SHOW_INFO(scenario->grid.getBoundingBoxVisualization());
    }

    auto cc_conf = smpl::collision::CollisionModelConfig();
    if (!smpl::collision::CollisionModelConfig::Load(scenario->ph, cc_conf)) {
        ROS_ERROR("Failed to load Collision Model Config");
        return false;
    }

    auto group_name = std::string();
    if (!FindParam(scenario->ph, "group_name", group_name)) {
        ROS_ERROR("Failed to read 'group_name' from the param server");
        return false;
    }

    if (!scenario->collision_model.init(
            &scenario->grid,
            robot_description,
            cc_conf,
            group_name,
            planning_model->getPlanningJoints()))
    {
        ROS_ERROR("Failed to initialize Collision Space");
        return false;
    }

    if (scenario->collision_model.robotCollisionModel()->name() == "pr2") {
        auto acm = smpl::collision::AllowedCollisionMatrix();
        for (auto& pair : PR2AllowedCollisionPairs) {
            acm.setEntry(pair.first, pair.second, true);
        }
        scenario->collision_model.setAllowedCollisionMatrix(acm);
    }

    // TODO: This retention is kinda stupid...
    scenario->scene.SetCollisionSpace(&scenario->collision_model);

    // TODO: ...and is why objects must be later added instead of after
    // CollisionSpaceScene initialization
    // Read in collision objects from file and add to the scene...
    auto object_filename = std::string();
    FindParam(scenario->ph, "object_filename", object_filename);

    if (!object_filename.empty()) {
        auto objects = GetCollisionObjects(object_filename, planning_frame);
        for (auto& object : objects) {
            scenario->scene.ProcessCollisionObjectMsg(object);
        }
    }

    // Set reference state in the collision model...
    // TODO: this retention is also stupid?
    if (!scenario->scene.SetRobotState(scenario->start_state)) {
        ROS_ERROR("Failed to set start state on Collision Space Scene");
        return false;
    }

    scenario->collision_model.setWorldToModelTransform(Eigen::Affine3d::Identity());

    SV_SHOW_DEBUG(scenario->grid.getDistanceFieldVisualization(0.2));
    SV_SHOW_DEBUG(scenario->collision_model.getCollisionRobotVisualization());
    SV_SHOW_INFO(scenario->collision_model.getCollisionWorldVisualization());
    SV_SHOW_INFO(scenario->collision_model.getOccupiedVoxelsVisualization());
    return true;
}

bool InitRobotState(
    smpl::urdf::RobotState* robot_state,
    const smpl::urdf::RobotModel* robot_model,
    const moveit_msgs::RobotState* state_msg)
{
    if (!InitRobotState(robot_state, robot_model)) {
        ROS_ERROR("Failed to initialize Robot State");
        return false;
    }
    for (auto i = 0; i < state_msg->joint_state.name.size(); ++i) {
        auto* var = GetVariable(robot_model, &state_msg->joint_state.name[i]);
        if (var == NULL) {
            ROS_WARN("Variable '%s' not found in the Robot Model", state_msg->joint_state.name[i].c_str());
            return false;
        }
        ROS_INFO("Set joint %s to %f", state_msg->joint_state.name[i].c_str(), state_msg->joint_state.position[i]);
        SetVariablePosition(robot_state, var, state_msg->joint_state.position[i]);
    }
    return true;
}

bool InitTestScenario(TestScenarioKDL* scenario)
{
    if (!InitTestScenarioPrePlanningModel(scenario)) {
        return false; // errors logged within
    }

    auto robot_description = std::string();
    auto found = false;
    std::tie(robot_description, found) = GetRobotDescription(scenario->ph);
    if (!found) {
        return false;
    }

    auto robot_config = RobotModelConfig();
    if (!ReadRobotModelConfig(scenario->ph, robot_config)) {
        ROS_ERROR("Failed to read robot model config from param server");
        return false;
    }

    ROS_INFO("Construct KDL Robot Model");
    if (!InitKDLRobotModel(
            &scenario->planning_model,
            robot_description,
            robot_config.kinematics_frame,
            robot_config.chain_tip_link))
    {
        ROS_ERROR("Failed to initialize robot model");
        return false;
    }

    // Set reference state in the robot planning model...
    auto reference_state = smpl::urdf::RobotState();
    InitRobotState(&reference_state, &scenario->planning_model.robot_model, &scenario->start_state);
    SetReferenceState(&scenario->planning_model, GetVariablePositions(&reference_state));

    if (!InitTestScenarioPostPlanningModel(scenario, &scenario->planning_model)) {
        return false; // errors logged within
    }

    return true;
}

bool InitTestScenario(TestScenarioPR2* scenario)
{
    if (!InitTestScenarioPrePlanningModel(scenario)) {
        return false; // errors logged within
    }

    auto robot_description = std::string();
    auto found = false;
    std::tie(robot_description, found) = GetRobotDescription(scenario->ph);
    if (!found) {
        return false;
    }

    auto robot_config = RobotModelConfig();
    if (!ReadRobotModelConfig(ros::NodeHandle(scenario->ph, "robot_model"), robot_config)) {
        ROS_ERROR("Failed to read robot model config from param server");
        return false;
    }

    ROS_INFO("Construct KDL Robot Model");
    if (!InitPR2RobotModel(
            &scenario->planning_model,
            robot_description,
            robot_config.kinematics_frame,
            robot_config.chain_tip_link))
    {
        ROS_ERROR("Failed to initialize robot model");
        return false;
    }

    // Set reference state in the robot planning model...
    auto reference_state = smpl::urdf::RobotState();
    InitRobotState(&reference_state, &scenario->planning_model.kdl_model.robot_model, &scenario->start_state);
    SetReferenceState(&scenario->planning_model, GetVariablePositions(&reference_state));

    if (!InitTestScenarioPostPlanningModel(scenario, &scenario->planning_model)) {
        return false; // errors logged within
    }

    return true;
}

// Update the RobotState to include point. Maybe smpl::RobotModel should
// include a visualization function, much like CollisionChecker.
static
void UpdateRobotState(
    smpl::urdf::RobotState* robot_state,
    const smpl::RobotModel* model,
    const smpl::RobotState* state)
{
    for (auto i = 0; i < model->getPlanningJoints().size(); ++i) {
        auto& var_name = model->getPlanningJoints()[i];
        auto val = (*state)[i];
        auto* var = GetVariable(GetRobotModel(robot_state), &var_name);
        assert(var != NULL);
        SetVariablePosition(robot_state, var, val);
    }
}

static
auto MakeRobotState(
    const smpl::urdf::RobotState* robot_state,
    const smpl::RobotModel* model)
    -> smpl::RobotState
{
    auto state = smpl::RobotState();
    return state;
}

auto MakeRobotState(
    const moveit_msgs::RobotState* robot_state,
    const smpl::RobotModel* model)
    -> std::pair<smpl::RobotState, bool>
{
    auto state = smpl::RobotState();
    for (auto& var_name : model->getPlanningJoints()) {
        auto found = false;
        for (auto i = 0; i < robot_state->joint_state.name.size(); ++i) {
            if (robot_state->joint_state.name[i] == var_name) {
                state.push_back(robot_state->joint_state.position[i]);
                found = true;
                break;
            }
        }
        if (!found) {
            ROS_ERROR("Joint variable '%s' was not found in robot state", var_name.c_str());
            return std::make_pair(smpl::RobotState(), false);
        }
    }

    return std::make_pair(state, true);
}

int WritePathCSV(
    const smpl::RobotModel* model,
    const std::vector<smpl::RobotState>* path,
    const char* filepath)
{
    auto* f = fopen(filepath, "w");
    if (f == NULL) return -1;

    for (auto i = 0; i < model->getPlanningJoints().size(); ++i) {
        if (i != 0) {
            if (fputs(",", f) < 0) return -1;
        }
        auto& var = model->getPlanningJoints()[i];
        if (fputs(var.c_str(), f) < 0) return -1;
    }
    if (fputs("\n", f) < 0) return -1;
    for (auto& point : *path) {
        for (auto i = 0; i < point.size(); ++i) {
            if (i != 0) {
                if (fputs(",", f) < 0) return -1;
            }
            if (fprintf(f, "%f", point[i]) < 0) return -1;
        }
        if (fputs("\n", f) < 0) return -1;
    }

    fclose(f);
    return 0;
}

int AnimateSolution(
    TestScenarioBase* scenario,
    const smpl::urdf::RobotModel* robot_model,
    smpl::RobotModel* planning_model,
    const std::vector<smpl::RobotState>* path)
{
    ROS_INFO("Animate path");

    auto curr_state = smpl::urdf::RobotState();
    if (!InitRobotState(&curr_state, robot_model, &scenario->start_state)) {
        return 1;
    }

    auto pidx = 0;
    while (ros::ok()) {
        auto& point = (*path)[pidx];
#if 1
        auto markers = scenario->collision_model.getCollisionRobotVisualization(point);
        for (auto& m : markers.markers) {
            m.ns = "path_animation";
        }
#else
        auto markers = visualization_msgs::MarkerArray();
#endif

        UpdateRobotState(&curr_state, planning_model, &point);
        UpdateVisualBodyTransforms(&curr_state);
        auto id = (int32_t)markers.markers.size();
        SV_SHOW_INFO(MakeRobotVisualization(&curr_state, smpl::visual::Color{ 0.0f, 1.0f, 0.0f, 1.0f }, "map", "path_animation", &id));

        SV_SHOW_INFO(markers);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        pidx++;
        pidx %= path->size();
    }

    return 0;
}