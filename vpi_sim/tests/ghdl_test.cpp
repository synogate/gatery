/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    Gatery is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#define BOOST_TEST_MAIN
#include <boost/process.hpp>
#include <boost/process/environment.hpp>
#include <boost/test/included/unit_test.hpp> 
#include <fstream>

#include <GhdlSimulation.h>

namespace bp = boost::process;
namespace fs = boost::filesystem;

BOOST_AUTO_TEST_CASE(prepare_vhdl_test_entity)
{
    {
        std::ofstream f{ "test_entity.vhd" };
        f << R"(
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity test_entity is
    generic (
        G_TEST : natural := 0
    );
    port (
        clk : in std_logic;
        rst : in std_logic;
        a : in std_logic_vector(31 downto 0);
        b : in std_logic_vector(31 downto 0);

        c_0 : out std_logic_vector(31 downto 0);
        c_1 : out std_logic_vector(31 downto 0)
    );
end test_entity ;

architecture arch of test_entity is
    signal s_local : std_logic_vector(31 downto 0);
begin

    s_local <= not b;

    p_0 : process(a, b)
    begin
        c_0 <= std_logic_vector(unsigned(a) + unsigned(s_local));
    end process ; -- p_0

    p_1 : process( clk )
    begin
        if( rising_edge(clk) ) then
            c_1 <= std_logic_vector(unsigned(a) + unsigned(s_local));
        end if ;
    end process ; -- p_1

end architecture ; -- arch
)";
    }

    auto ghdl_path = bp::search_path("ghdl");
    BOOST_ASSERT(!ghdl_path.empty());

    BOOST_TEST(bp::system(ghdl_path, "-i", "test_entity.vhd") == 0);
}

BOOST_AUTO_TEST_CASE(ghdl_load_vpi_module, *boost::unit_test::depends_on("prepare_vhdl_test_entity"))
{
    vpi_client::GhdlSimulation sim;
    sim.launch("test_entity", {});
    BOOST_TEST(sim.exit() == 0);
}
