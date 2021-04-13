
workspace "gatery"

    startproject "gatery-stl-test"

include "source/premake5.lua"
include "tests/premake5.lua"

    project "*"
        GateryWorkspaceDefaults()
