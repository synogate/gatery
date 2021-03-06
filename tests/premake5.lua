

project "gatery-frontend-test"
    kind "ConsoleApp"
    files { "frontend/**.cpp", "frontend/**.h" }
    links "gatery"
    includedirs { "%{prj.location}/../source", "%{prj.location}/" }

    pchheader "frontend/pch.h"
    pchsource "frontend/pch.cpp"

    GateryProjectDefaults()

    defines "BOOST_TEST_DYN_LINK"
    filter "system:linux"
        links { "boost_unit_test_framework", "dl" }

project "gatery-scl-test"
    kind "ConsoleApp"
    files { "scl/**.cpp", "scl/**.h" }
    links "gatery"
    includedirs { "%{prj.location}/../source", "%{prj.location}/" }

    pchheader "scl/pch.h"
    pchsource "scl/pch.cpp"

    GateryProjectDefaults()

    defines "BOOST_TEST_DYN_LINK"
    filter "system:linux"
        links { "boost_unit_test_framework", "dl" }
