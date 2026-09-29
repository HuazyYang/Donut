#
# Copyright (c) 2014-2021, NVIDIA CORPORATION. All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.


include(${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/shadertool/cmake/ShaderToolFunctions.cmake)

file(GLOB donut_shaders
	"*.hlsl"
	"passes/*.hlsl"
	"../include/donut/shaders/*.hlsli"
	"../include/donut/shaders/*.h"
)

if(DONUT_SHADERS_OUTPUT_DIR AND NOT DONUT_WITH_STATIC_SHADERS)
	set(output_base ${DONUT_SHADERS_OUTPUT_DIR})
else()
	set(output_base ${CMAKE_CURRENT_BINARY_DIR}/compiled_shaders)
endif()

if(DONUT_WITH_STATIC_SHADERS)
	set(output_mode "HEADER_BLOB")
else()
	set(output_mode "BINARY_BLOB")
endif()

set(donut_shader_backends)
if (DONUT_WITH_DX11)
	list(APPEND donut_shader_backends DXBC)
endif()
if (DONUT_WITH_DX12)
	list(APPEND donut_shader_backends DXIL)
endif()
if (DONUT_WITH_VULKAN)
	list(APPEND donut_shader_backends SPIRV)
endif()

if(DONUT_WITH_AFTERMATH OR DONUT_EMBED_SHADER_PDBS)
	set(shadertool_pdb_options EMBED_PDB)
else()
	set(shadertool_pdb_options)
endif()

if (donut_shader_backends)
	shadertool_add_shader_objects(
		TARGET donut_shaders
		CONFIG_FILE ${CMAKE_CURRENT_LIST_DIR}/DonutShaders.cfg
		BACKENDS ${donut_shader_backends}
		OUTPUT_MODE ${output_mode}
		OUTPUT_DIRECTORY ${output_base}
		SHADER_MODEL 6_5
		INCLUDE_DIRECTORIES ${DONUT_SHADER_INCLUDE_DIR}
		DEFINES_DXBC TARGET_D3D11
		DEFINES_DXIL TARGET_D3D12
		DEFINES_SPIRV SPIRV TARGET_VULKAN
		SOURCES ${donut_shaders}
		FOLDER Donut
		${shadertool_pdb_options}
	)
else()
	# No graphics backends enabled: keep the donut_shaders target so that
	# add_dependencies(...) in donut-engine/render/app keeps working, matching
	# the legacy branch which also creates the target unconditionally.
	add_custom_target(donut_shaders SOURCES ${donut_shaders})
	set_target_properties(donut_shaders PROPERTIES FOLDER Donut)
endif()

