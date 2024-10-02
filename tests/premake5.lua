

project "gatery-frontend-test"
    kind "ConsoleApp"
    files { "frontend/**.cpp", "frontend/**.h" }
    links { "gatery_core", "gatery_scl" }
    includedirs { "%{prj.location}/../source", "%{prj.location}/" }

    pchheader "frontend/pch.h"
    pchsource "frontend/pch.cpp"

    GateryProjectDefaults()

    defines "BOOST_TEST_DYN_LINK"
    filter "system:linux"
        links { "boost_unit_test_framework", "boost_filesystem", "boost_json", "pthread", "dl", "yaml-cpp" }

project "gatery-scl-test"
    kind "ConsoleApp"
    files { "scl/**.cpp", "scl/**.h" }
    links { "gatery_core", "gatery_scl" }
    includedirs { "%{prj.location}/../source", "%{prj.location}/" }

    pchheader "scl/pch.h"
    pchsource "scl/pch.cpp"

    GateryProjectDefaults()

    defines "BOOST_TEST_DYN_LINK"
    filter "system:linux"
        links { "boost_unit_test_framework", "boost_filesystem", "boost_json", "pthread", "dl", "yaml-cpp" }

project "gatery-tutorial-test"
    kind "ConsoleApp"
    files { "tutorial/**.cpp", "tutorial/**.h" }
    links { "gatery_core", "gatery_scl" }
    includedirs { "%{prj.location}/../source", "%{prj.location}/" }

    pchheader "tutorial/pch.h"
    pchsource "tutorial/pch.cpp"

    GateryProjectDefaults()

    defines "BOOST_TEST_DYN_LINK"
    filter "system:linux"
        links { "boost_unit_test_framework", "boost_filesystem", "boost_json", "pthread", "dl", "yaml-cpp" }
