

project "gatery-frontend-test"
    kind "ConsoleApp"
    files { "frontend/**.cpp", "frontend/**.h" }
    links "gatery"
    includedirs "%{prj.location}/../source"

    GateryProjectDefaults()

    defines "BOOST_TEST_DYN_LINK"
    filter "system:linux"
        links { "boost_unit_test_framework", "dl" }


project "gatery-stl-test"
    kind "ConsoleApp"
    files { "scl/**.cpp", "scl/**.h" }
    links "gatery"
    includedirs "%{prj.location}/../source"

    GateryProjectDefaults()

    defines "BOOST_TEST_DYN_LINK"
    filter "system:linux"
        links { "boost_unit_test_framework", "dl" }
