
function GateryWorkspaceDefaults()

    configurations { "Debug", "Release" }
    architecture "x64"
    symbols "On"
    flags { "MultiProcessorCompile" }
    cppdialect "C++latest"

    filter "configurations:Debug"
        runtime "Debug"

    filter "configurations:Release"
        runtime "Release"
        optimize "On"

    filter "system:linux"
        buildoptions { "-std=c++2a", "-fcoroutines" }

end

function GateryProjectDefaults()

    targetdir "%{wks.location}/bin/%{cfg.system}-%{cfg.architecture}-%{cfg.longname}"
    objdir "%{wks.location}/obj/%{cfg.system}-%{cfg.architecture}-%{cfg.longname}"

end

project "gatery"
    kind "StaticLib"
    files { 
        "gatery/export/**.cpp", "gatery/export/**.c", "gatery/export/**.h",
        "gatery/frontend/**.cpp", "gatery/frontend/**.c", "gatery/frontend/**.h",
        "gatery/hlim/**.cpp", "gatery/hlim/**.c", "gatery/hlim/**.h",
        "gatery/scl/**.cpp", "gatery/scl/**.c", "gatery/scl/**.h",
        "gatery/simulation/**.cpp", "gatery/simulation/**.c", "gatery/simulation/**.h",
        "gatery/utils/**.cpp", "gatery/utils/**.c", "gatery/utils/**.h",
        "gatery/*"
    }

    pchsource "gatery/pch.cpp"
    pchheader "gatery/pch.h"

    includedirs "%{prj.location}/"
    GateryProjectDefaults()

    filter "files:**.c"
        flags {"NoPCH"}


require('vstudio')

local function MSVCToolsetVersion(prj)
    premake.w('<VCToolsVersion>14.28.29333</VCToolsVersion>')
end

premake.override(premake.vstudio.vc2010.elements, "configurationProperties", function(base, prj)
	local calls = base(prj)
	table.insertafter(calls, premake.vstudio.vc2010.platformToolset, MSVCToolsetVersion)
	return calls
end)
