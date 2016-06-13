////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2015, Benjamin Cohen, Andrew Dornbush
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//     1. Redistributions of source code must retain the above copyright notice
//        this list of conditions and the following disclaimer.
//     2. Redistributions in binary form must reproduce the above copyright
//        notice, this list of conditions and the following disclaimer in the
//        documentation and/or other materials provided with the distribution.
//     3. Neither the name of the copyright holder nor the names of its
//        contributors may be used to endorse or promote products derived from
//        this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
////////////////////////////////////////////////////////////////////////////////

/// \author Benjamin Cohen
/// \author Andrew Dornbush

#ifndef sbpl_manip_motion_primitive_h
#define sbpl_manip_motion_primitive_h

// standard includes
#include <vector>
#include <sstream>

// system includes
#include <ros/console.h>
#include <leatherman/print.h>

// project includes
#include <sbpl_arm_planner/types.h>

namespace sbpl {
namespace manip {

struct MotionPrimitive
{
    enum Type
    {
        LONG_DISTANCE = -1,
        SNAP_TO_RPY = 0, // NOTE: start at 0 to use successive types as indices
        SNAP_TO_XYZ,
        SNAP_TO_XYZ_RPY,
        SHORT_DISTANCE,
        NUMBER_OF_MPRIM_TYPES
    };

    Type type;
    int id;
    Action action;

    void print() const;
};

inline std::ostream& operator<<(std::ostream& o, MotionPrimitive::Type type)
{
    switch (type) {
    case MotionPrimitive::LONG_DISTANCE:
        o << "LONG_DISTANCE";
        break;
    case MotionPrimitive::SNAP_TO_RPY:
        o << "SNAP_TO_RPY";
        break;
    case MotionPrimitive::SNAP_TO_XYZ:
        o << "SNAP_TO_XYZ";
        break;
    case MotionPrimitive::SNAP_TO_XYZ_RPY:
        o << "SNAP_TO_XYZ_RPY";
        break;
    case MotionPrimitive::SHORT_DISTANCE:
        o << "SHORT_DISTANCE";
        break;
    }
    return o;
}

inline
std::string to_string(MotionPrimitive::Type type)
{
    std::stringstream ss;
    ss << type;
    return ss.str();
}

inline
void MotionPrimitive::print() const
{
    ROS_INFO("type: %d  id: %d  nsteps: %d ", type, id, int(action.size()));
    std::stringstream os;
    for (std::size_t j = 0; j < action.size(); ++j) {
        os.str("");
        os << "[step: " << int(j+1) << "/" << int(action.size()) << "] ";
        for (std::size_t k = 0; k < action[j].size(); ++k) {
            os << std::setw(4) << std::setprecision(3) << std::fixed << action[j][k] << " ";
        }
        ROS_INFO_STREAM(os.str());
    }
}

} // namespace manip
} // namespace sbpl

#endif
