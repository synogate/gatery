
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
        "hcl/export/**.cpp", "hcl/export/**.c", "hcl/export/**.h",
        "hcl/frontend/**.cpp", "hcl/frontend/**.c", "hcl/frontend/**.h",
        "hcl/hlim/**.cpp", "hcl/hlim/**.c", "hcl/hlim/**.h",
        "hcl/scl/**.cpp", "hcl/scl/**.c", "hcl/scl/**.h",
        "hcl/simulation/**.cpp", "hcl/simulation/**.c", "hcl/simulation/**.h",
        "hcl/utils/**.cpp", "hcl/utils/**.c", "hcl/utils/**.h",
        "hcl/*"
    }

    pchsource "hcl/pch.cpp"
    pchheader "hcl/pch.h"

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
