function Gtry_getFileSize(name)
    local f = assert(io.open(name, "rb"))
    return f:seek("end")
end

function Gtry_ReadFileAsArray(name, out_file)
    local first_line = true
    local f = assert(io.open(name, "rb"))
    while true do
        local bytes = f:read(24)
        if not bytes then 
            break 
        end

        if first_line then
            first_line = false
        else
            out_file:write(',')
        end
        out_file:write("\n\t")
        for i = 1, #bytes do
            if i ~= 1 then
                out_file:write(',')
            end
            out_file:write(string.format("0x%02X", bytes:byte(i)))
        end
        --out_file:write("")
    end
end

function Gtry_EmbedResources(name, namespace, prefix_path, pattern)

    local header = io.open(name .. ".h", "wb")
    local source = io.open(name .. ".cpp", "wb")

    header:write([[
#pragma once
// Generated file, do not modify
#include <span>
#include <string_view>
#include <array>
#include <cstddef>

namespace ]])
    header:write(namespace .. "{\n")
    source:write("#include \"" .. path.getbasename(name) .. ".h\"\n")
    source:write("namespace " .. namespace .. " {\n\n")

    local manifest_content = ""

    local resource_files = os.matchfiles(prefix_path .. pattern)
    for i = 1, #resource_files do
        local file_name = resource_files[i]:sub(#prefix_path + 1)
        local symbol_name = file_name:gsub('[%p%c%s]', '_')

        io.write("Embed into " .. name .. ": " .. file_name .. "\n")

        local content_size = Gtry_getFileSize(resource_files[i])

        header:write("extern const std::uint8_t " .. symbol_name .. "_data[" .. content_size .. "];\n")
        header:write("static constexpr size_t " .. symbol_name .. "_size = " .. content_size .. ";\n")

        source:write("const std::uint8_t " .. symbol_name .. "_data[" .. content_size .. "] = {\n")
        Gtry_ReadFileAsArray(resource_files[i], source)
        source:write("};\n")

        manifest_content = manifest_content .. "    ManifestEntry{ .filename = \"" .. file_name .. "\"sv, .data = std::span<const std::byte>((const std::byte*) " .. symbol_name .. "_data, " .. symbol_name .. "_size) },\n"
    end

    source:write("using namespace std::literals;\n")
    source:write("const std::array<ManifestEntry, " .. #resource_files .. "> manifest = {\n" .. manifest_content .. "};\n")


    header:write("\n\n")
    header:write("struct ManifestEntry { std::string_view filename; std::span<const std::byte> data; };\n\n")
    header:write("extern const std::array<ManifestEntry, " .. #resource_files .. "> manifest;\n\n")


    header:write("}\n")
    source:write("}\n")


    files(name .. ".h")
    files(name .. ".cpp")
end




function GateryWorkspaceDefaults()

    newoption {
        trigger = "with-driver",
        description = "On windows, enables the driver project"
    }

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

project "gatery_core"
    kind "StaticLib"

    files { 
        "gatery/export/**.cpp", "gatery/export/**.c", "gatery/export/**.h",
        "gatery/frontend/**.cpp", "gatery/frontend/**.c", "gatery/frontend/**.h",
        "gatery/hlim/**.cpp", "gatery/hlim/**.c", "gatery/hlim/**.h",
        "gatery/simulation/**.cpp", "gatery/simulation/**.c", "gatery/simulation/**.h",
        "gatery/utils/**.cpp", "gatery/utils/**.c", "gatery/utils/**.h",
        "gatery/debug/**.cpp", "gatery/debug/**.c", "gatery/debug/**.h",
        "gatery/*"
    }

    removefiles {
        "gatery/scl/**.cpp", "gatery/scl/**.c", "gatery/scl/**.h",
        "gatery/scl_pch.cpp", "gatery/scl_pch.h"
    }

    pchsource "gatery/pch.cpp"
    pchheader "gatery/pch.h"

    includedirs "%{prj.location}/"
    GateryProjectDefaults()

    Gtry_EmbedResources("gen/res/gtry_resources", "gtry::res", "../", "data/**")
    includedirs "gen/"

    filter "system:windows"
        flags { "FatalCompileWarnings" }
    
    filter "files:gen/**.cpp"
        flags {"NoPCH"}

    filter "files:**.c"
        flags {"NoPCH"}

project "gatery_scl"
    kind "StaticLib"

    files { 
        "gatery/scl/**.cpp", "gatery/scl/**.c", "gatery/scl/**.h",
        "gatery/scl_pch.cpp", "gatery/scl_pch.h"
    }

    pchsource "gatery/scl_pch.cpp"
    pchheader "gatery/scl_pch.h"

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

    kind "None"
    filter "options:with-driver"
        kind "StaticLib"
