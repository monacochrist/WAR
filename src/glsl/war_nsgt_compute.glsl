//-----------------------------------------------------------------------------
//
// WAR - make music with vim motions
// Copyright (C) 2025 Nick Monaco
// 
// This file is part of WAR 1.0 software.
// WAR 1.0 software is licensed under the GNU Affero General Public License
// version 3, with the following modification: attribution to the original
// author is waived.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// 
// For the full license text, see LICENSE-AGPL and LICENSE-CC-BY-SA and LICENSE.
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// src/glsl/war_nsgt_compute.glsl
//-----------------------------------------------------------------------------

#version 450

layout(local_size_x = 64) in;

layout(binding = 0) buffer LBuffer { float l_data[]; };
layout(binding = 1) buffer RBuffer { float r_data[]; };
layout(binding = 2) buffer DiffBuffer { float diff[]; }; // optional undo

layout(push_constant) uniform PushConstants {
    int operation_type;
    int channel;
    int bin_start;
    int bin_end;
    int frame_start;
    int frame_end;
    float param1;
    float param2;
} pc;

void main() {
    uint idx = gl_GlobalInvocationID.x;

    // bounds check
    if(idx >= (pc.frame_end - pc.frame_start) * (pc.bin_end - pc.bin_start))
        return;

    // Example: simple gain on selected channel
    if(pc.channel == 0) l_data[idx] *= pc.param1;
    else if(pc.channel == 1) r_data[idx] *= pc.param1;
    else {
        l_data[idx] *= pc.param1;
        r_data[idx] *= pc.param1;
    }

    // Optionally write undo diff
    // diff[idx] = ...;
}
