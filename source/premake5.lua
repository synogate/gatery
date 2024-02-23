
function GateryWorkspaceDefaults()

    configurations { "Debug", "Release", "Coverage" }
    architecture "x64"
    symbols "On"
    flags { "MultiProcessorCompile" }
    cppdialect "c++20"

    defines { "_SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING", "_SILENCE_CXX23_DENORM_DEPRECATION_WARNING" }

    filter "configurations:Debug"
        runtime "Debug"

    filter "configurations:Coverage"
        runtime "Debug"

    filter "configurations:Release"
        runtime "Release"
        optimize "On"

    filter { "system:windows" }
        buildoptions { "/Zc:preprocessor", "/bigobj" }

    filter { "system:linux" }
        buildoptions { "-std=c++2a", "-fcoroutines" }
        defines { "BOOST_STACKTRACE_USE_ADDR2LINE" }
        links { 
            "boost_unit_test_framework", 
            "boost_program_options", 
            "boost_filesystem", 
            "boost_iostreams",
            "pthread",
            "yaml-cpp",
            "boost_json",
            "dl"
        }
		
	filter {}
end

function GateryProjectDefaults()
    targetdir "%{wks.location}/bin/%{cfg.system}-%{cfg.architecture}-%{cfg.longname}"
    objdir "%{wks.location}/obj/%{cfg.system}-%{cfg.architecture}-%{cfg.longname}"
    defines { "_SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING" }

    filter {"system:linux", "configurations:Coverage"}
		buildoptions { "--coverage", "-fprofile-arcs", "-ftest-coverage" }
        linkoptions { "--coverage" }
	
	filter {}
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

        removefiles {
            "gatery/scl/driver/linux/**.cpp"
        }

    filter "files:gatery/scl/driver/**.cpp"
        flags {"NoPCH"}

    filter "files:**.c"
        flags {"NoPCH"}

externalproject "gatery_driver"
    location ".."
    uuid "46dda5a3-61d7-480c-a2cd-8538a48ac9ea"
    language "C++"
    kind "StaticLib"
