
function GateryWorkspaceDefaults()

    configurations { "Debug", "Release" }
    architecture "x64"
    symbols "On"
    flags { "MultiProcessorCompile" }
    cppdialect "c++latest"

    filter "configurations:Debug"
        runtime "Debug"

    filter "configurations:Release"
        runtime "Release"
        optimize "On"

    filter { "system:linux" }
        buildoptions { "-std=c++2a", "-fcoroutines" }

    filter {}

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
        "gatery/debug/**.cpp", "gatery/debug/**.c", "gatery/debug/**.h",
        "gatery/*"
    }

    pchsource "gatery/pch.cpp"
    pchheader "gatery/pch.h"

    includedirs "%{prj.location}/"
    GateryProjectDefaults()

    filter "system:windows"
        flags { "FatalCompileWarnings" }

    filter "files:**.c"
        flags {"NoPCH"}
