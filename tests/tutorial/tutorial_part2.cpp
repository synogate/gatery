/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2022 Michael Offel, Andreas Ley

	Gatery is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 3 of the License, or (at your option) any later version.

	Gatery is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this library; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/


#include "tutorial/pch.h"

#include <boost/test/unit_test.hpp>

using BoostUnitTestSimulationFixture = gtry::BoostUnitTestSimulationFixture;

using namespace gtry;

/*******************************************************************************
If any of these are updated, please also update the tutorial / documentation!!!
********************************************************************************/

BOOST_AUTO_TEST_CASE(tutorial_part2_signals)
{
    DesignScope design;

	UInt undefined_8_wide_uint = 8_b;
	UInt undefined_10_wide_uint = BitWidth(10);
	UInt undefined_12_wide_uint(12_b);
	UInt undefined_16_wide_uint;
	undefined_16_wide_uint = BitWidth(16);

    design.postprocess();
}

BOOST_FIXTURE_TEST_CASE(tutorial_part2_operators_1, BoostUnitTestSimulationFixture)
{
	Bit driving_bit;
	Bit driven_bit;

	driven_bit = driving_bit;

	UInt driving_bvec = 8_b;
	UInt driven_bvec;

	driven_bvec = driving_bvec;

	runEvalOnlyTest();
}

BOOST_FIXTURE_TEST_CASE(tutorial_part2_operators_2, BoostUnitTestSimulationFixture)
{
	Bit b;

	b = '1'; // true
	b = '0'; // false
	b = 'X'; // undefined

	runEvalOnlyTest();
}

BOOST_FIXTURE_TEST_CASE(tutorial_part2_operators_3, BoostUnitTestSimulationFixture)
{
	Bit b;

	// Assigning bool literals
	b = true;
	b = false;

	runEvalOnlyTest();
}

BOOST_FIXTURE_TEST_CASE(tutorial_part2_operators_4, BoostUnitTestSimulationFixture)
{
	UInt bv_1, bv_2, bv_3, bv_4, bv_5;

	bv_1 = "b1010"; // Binary, 4_b wide
	bv_2 = "xff0f0"; // Hex, 20_b wide
	bv_3 = "d42"; // Decimal, 6_b wide
	bv_4 = "64b0"; // 64 zero bits
	bv_5 = "6b00xx00"; // Mixture of zeros and undefined bits

	runEvalOnlyTest();
}

BOOST_FIXTURE_TEST_CASE(tutorial_part2_operators_5, BoostUnitTestSimulationFixture)
{
	UInt bv_1, bv_2, bv_3;

	bv_1 = 32_b;
	bv_1 = 42; // Still 32_b wide

	bv_2 = 42; // 6_b wide

	unsigned configuration_option = 41;
	bv_3 = 10_b;
	bv_3 = configuration_option; // 10_b wide

	runEvalOnlyTest();
}

BOOST_FIXTURE_TEST_CASE(tutorial_part2_operators_typecasts, BoostUnitTestSimulationFixture)
{
    UInt uint_signal = 42;
    BVec bvec_signal = (BVec) uint_signal;
    SInt sint_signal = (SInt) bvec_signal;


	runEvalOnlyTest();
}

BOOST_FIXTURE_TEST_CASE(tutorial_part2_operators_6, BoostUnitTestSimulationFixture)
{
	UInt bv;
	bv = "d42";

	// Prints "The width of bv is 6" to the console
	//std::cout << "The width of bv is " << bv.getWidth() << std::endl;
	BOOST_TEST(bv.width() == 6_b);

	runEvalOnlyTest();
}

BOOST_FIXTURE_TEST_CASE(tutorial_part2_operators_7, BoostUnitTestSimulationFixture)
{
	Bit a, b;
	a = '1';
	b = '0';

	// Logical and bitwise negation both do the same
	Bit not_a = ~a;
	Bit also_not_a = !a;

	// And, or, xor as usual
	Bit a_and_b = a & b;
	Bit a_or_b = a | b;
	Bit a_xor_b = a ^ b;

	// Composition and bracketing as usual
	Bit a_nand_b = ~(a & b);
	Bit a_nor_b = ~(a | b);

	runEvalOnlyTest();
}

BOOST_FIXTURE_TEST_CASE(tutorial_part2_operators_8, BoostUnitTestSimulationFixture)
{
	UInt a = 8_b;
	UInt b = 8_b;
	a = 2;
	b = 3;

	// Bitwise negation
	UInt not_a = ~a;

	// And, or, xor as usual
	UInt a_and_b = a & b;
	UInt a_or_b = a | b;
	UInt a_xor_b = a ^ b;

	// Composition and bracketing as usual
	UInt a_nand_b = ~(a & b);
	UInt a_nor_b = ~(a | b);

	runEvalOnlyTest();
}

BOOST_FIXTURE_TEST_CASE(tutorial_part2_operators_9, BoostUnitTestSimulationFixture)
{
	UInt a = 8_b;
	a = 4;

	// Whether or not to negate a
	Bit do_negate_a = '1';

	// xor every bit in a with do_negate_a
	UInt possibly_negated_a = a ^ do_negate_a;

	runEvalOnlyTest();
}

BOOST_FIXTURE_TEST_CASE(tutorial_part2_operators_10, BoostUnitTestSimulationFixture)
{
	UInt unsigned_8_wide = "8b0";
	// Zero extends by 2 bits
	UInt unsigned_10_wide = ext(unsigned_8_wide, +2_b);

	SInt signed_8_wide = (SInt) "8b0";
	// Sign extends by 2 bits
	SInt signed_10_wide = ext(signed_8_wide, +2_b);

	BOOST_TEST(unsigned_10_wide.width() == 10_b);
	BOOST_TEST(signed_10_wide.width() == 10_b);

	runEvalOnlyTest();
}

BOOST_FIXTURE_TEST_CASE(tutorial_part2_operators_11, BoostUnitTestSimulationFixture)
{
	UInt unsigned_8_wide = "8b0";
	// Zero extend unsigned integer
	UInt unsigned_10_wide = zext(unsigned_8_wide, +2_b);

	UInt signed_8_wide = "8b0";
	// Sign extend integer
	UInt signed_10_wide = sext(signed_8_wide, +2_b);

	UInt mask_8_wide = "8b0";
	// Extend with ones
	UInt mask_10_wide_one_extended = oext(mask_8_wide, +2_b);

	BOOST_TEST(unsigned_10_wide.width() == 10_b);
	BOOST_TEST(signed_10_wide.width() == 10_b);
	BOOST_TEST(mask_10_wide_one_extended.width() == 10_b);

	runEvalOnlyTest();
}

BOOST_FIXTURE_TEST_CASE(tutorial_part2_operators_12, BoostUnitTestSimulationFixture)
{
	Bit bit = '1';
	// Sign extends by 9 bits
	UInt ten_1 = sext(bit, +9_b);
	BOOST_TEST(ten_1.width() == 10_b);

	sim_assert(ten_1 == "b1111111111");

	runEvalOnlyTest();
}

BOOST_FIXTURE_TEST_CASE(tutorial_part2_operators_13, BoostUnitTestSimulationFixture)
{
	UInt a = "10b0";
	UInt b = "8b0";

	// This would be illegal because a nd b have different sizes:
	// UInt c = a & b;

	// This zero-extends b to the width of a (10-bits) and then performs the element wise or
	UInt a_or_b = a | zext(b);
	
	// The same works for sext and oext.
	UInt a_and_b = a & oext(b);
	BOOST_TEST(a_or_b.size() == 10);
	BOOST_TEST(a_and_b.size() == 10);

	runEvalOnlyTest();
}

BOOST_FIXTURE_TEST_CASE(tutorial_part2_operators_14, BoostUnitTestSimulationFixture)
{
	UInt a = 4_b;
	a = 0; // zero-extended to b0000

	UInt a_or_0001 = a | 1; // 1 is zero-extended to b0001
	
	unsigned int i = 2;
	UInt a_and_b = a & i; // i is zero-extended to b0010

	sim_assert(a_or_0001 == "b0001");
	sim_assert(a_and_b == "b0000");

	runEvalOnlyTest();
}


BOOST_FIXTURE_TEST_CASE(tutorial_part2_operators_15, BoostUnitTestSimulationFixture)
{
    UInt ieee_float_32 = "32b0";

    UInt mantissa = ieee_float_32(0, 23_b); // Extract 23 bits from bit 0 onwards
    UInt exponent = ieee_float_32(23, 8_b); // Extract 8 bits from bit 23 onwards
    Bit sign = ieee_float_32[31];         // Extract bit 31

	runEvalOnlyTest();
}

BOOST_FIXTURE_TEST_CASE(tutorial_part2_operators_16, BoostUnitTestSimulationFixture)
{
    UInt bvec = "10b0";
    
    // Least and most significant bits, independent of size of bvec
    Bit bvec_lsb_1 = bvec[0];
    Bit bvec_msb_1 = bvec[-1];

    // Least and most significant bits, independent of size of bvec
    Bit bvec_lsb_2 = bvec.lsb();
    Bit bvec_msb_2 = bvec.msb();

    // Iterating over each bit in bvec in turn
    for (auto &b : bvec) {
        //do_sth_with_bit(b);
		b = '1';
    }

	runEvalOnlyTest();
}


BOOST_FIXTURE_TEST_CASE(tutorial_part2_operators_17, BoostUnitTestSimulationFixture)
{
    UInt bvec = "32b0";
    UInt index = "4b0";
    
    Bit bit = bvec[index];
    UInt subrange = bvec(index, 2_b);

	runEvalOnlyTest();
}

BOOST_FIXTURE_TEST_CASE(tutorial_part2_operators_cat_pack, BoostUnitTestSimulationFixture)
{
    UInt mantissa = "23b0";
    UInt exponent = "8b0";
    Bit sign = '1';

    // Concatenates all arguments, putting the last
    // argument (mantissa) into the least significant bits.
    UInt ieee_float_32 = cat(sign, exponent, mantissa);

    // Packs all arguments, putting the first
    // argument (mantissa) into the least significant bits.
    UInt same_ieee_float_32 = pack(mantissa, exponent, sign);

	sim_assert(ieee_float_32[-1] == '1');
	sim_assert(same_ieee_float_32[-1] == '1');

	runEvalOnlyTest();
}

BOOST_FIXTURE_TEST_CASE(tutorial_part2_operators_shift, BoostUnitTestSimulationFixture)
{
    UInt value = "10d8";

    UInt value_times_4 = value << 2;
    UInt value_div_4 = value >> 2;


	BOOST_TEST(value_times_4.width() == value.width());
	BOOST_TEST(value_div_4.width() == value.width());

	sim_assert(value_times_4 == 32);
	sim_assert(value_div_4 == 2);

	runEvalOnlyTest();	
}

BOOST_FIXTURE_TEST_CASE(tutorial_part2_operators_shiftr_signed, BoostUnitTestSimulationFixture)
{
    SInt value = (SInt)"2b10";

    SInt value_2 = value >> 1;


	BOOST_TEST(value_2.width() == value.width());
	sim_assert(value_2 == (SInt)"2b11");

	runEvalOnlyTest();	
}

BOOST_FIXTURE_TEST_CASE(tutorial_part2_operators_rot, BoostUnitTestSimulationFixture)
{
    UInt value = "5b11000";

    UInt value_rotated_left_2 = rotl(value, 2);
    UInt value_rotated_right_2 = rotr(value, 2);


	BOOST_TEST(value_rotated_left_2.width() == value.width());
	BOOST_TEST(value_rotated_right_2.width() == value.width());

	sim_assert(value_rotated_left_2 == "5b00011");
	sim_assert(value_rotated_right_2 == "5b00110");

	runEvalOnlyTest();	
}


BOOST_FIXTURE_TEST_CASE(tutorial_part2_operators_arithmetic, BoostUnitTestSimulationFixture)
{
    UInt a = "23d10";
    UInt b = "23d4";

    UInt a_plus_b = a + b; // Exported to VHDL as a+b
    UInt a_minus_b = a - b; // Exported to VHDL as a-b
    UInt a_times_b = a * b; // Exported to VHDL as a*b so your mileage may vary
    UInt a_div_b = a / b; // Exported to VHDL as a/b so your mileage may vary

	sim_assert(a_plus_b == 14);
	sim_assert(a_minus_b == 6);
	sim_assert(a_times_b == 40);
	sim_assert(a_div_b == 2);

	runEvalOnlyTest();	
}

BOOST_FIXTURE_TEST_CASE(tutorial_part2_operators_comparisons, BoostUnitTestSimulationFixture)
{
    UInt a = "10b0";
    UInt b = "10b0";

    // Less / greater
    Bit a_lt_b = a < b;
    Bit a_gt_b = a > b;
    
    // Less or equal / greater or equal
    Bit a_le_b = a <= b;
    Bit a_ge_b = a >= b;

    // Equal / not equal
    Bit a_eq_b = a == b;
    Bit a_ne_b = a != b;

	runEvalOnlyTest();	
}

BOOST_FIXTURE_TEST_CASE(tutorial_part2_operators_mux, BoostUnitTestSimulationFixture)
{
    UInt idx = 2_b;
    idx = 2; // Can be anything from 0..3

    UInt a_0 = 10_b;
    UInt a_1 = 10_b;
    UInt a_2 = 10_b;
    UInt a_3 = 10_b;

    UInt a = mux(idx, {a_0, a_1, a_2, a_3});

	runEvalOnlyTest();	
}

BOOST_FIXTURE_TEST_CASE(tutorial_part2_operators_pins, BoostUnitTestSimulationFixture)
{
    UInt push_buttons = pinIn(4_b);
    Bit single_button = pinIn();

    UInt color_led = 3_b;
    color_led = 1;
    pinOut(color_led);

    Bit led = '0';
    pinOut(led);

	runEvalOnlyTest();	
}



BOOST_FIXTURE_TEST_CASE(tutorial_part2_mutable_expl, BoostUnitTestSimulationFixture)
{
    UInt value = 4_b;
    
    value = 0;
    UInt a = value;

    value = 1;
    UInt b = value;

    value = 2;
    UInt c = value;
    
    // a is 0, b is 1, c is 2

	sim_assert(a == 0);
	sim_assert(b == 1);
	sim_assert(c == 2);

	runEvalOnlyTest();
}

BOOST_FIXTURE_TEST_CASE(tutorial_part2_mutable_parity, BoostUnitTestSimulationFixture)
{
    UInt value = 10_b;
    value = 42;

    // Start with true
    Bit parity = true;

    // Xor all bits together by "accumulating" them
    // one by one into the parity.
    for (auto &b : value)
        parity = parity ^ b;

    // Now parity is true iff number of set bits in value is odd.

	sim_assert(parity == '0');

	runEvalOnlyTest();		
}


BOOST_FIXTURE_TEST_CASE(tutorial_part2_mutable_inplace, BoostUnitTestSimulationFixture)
{
    UInt a = 10_b;
    a = 41;
    UInt b = 10_b;
    b = 42;


    a &= b; // compute b & a and store in a
    a |= b; // compute b | a and store in a
    // ... 
    // Also holds for Bit    

    a += b; // add b to a and store in a
    a -= b; // subtract b from a and store in a
    // ...

    a <<= 2; // Shift a by 2 bits to the left and store in a
    a >>= 2; // Shift a by 2 bits to the right and store in a

	runEvalOnlyTest();		
}


BOOST_FIXTURE_TEST_CASE(tutorial_part2_mutable_write_slice, BoostUnitTestSimulationFixture)
{
    UInt ieee_float_32 = "32b0";

    // Lets build a 1.0f float
    ieee_float_32[31] = '0';  // The sign is positive
    ieee_float_32(0, 23_b) = 0; // The mantissa is all 0 (the "1." is implicit with floats)
    ieee_float_32(23, 8_b) = 127; // The exponent is exactly the bias to end up with 2^0.

	sim_assert(ieee_float_32 == "32b00111111100000000000000000000000") << "got " << ieee_float_32;

	runEvalOnlyTest();		
}

BOOST_FIXTURE_TEST_CASE(tutorial_part2_mutable_condition_scopes, BoostUnitTestSimulationFixture)
{
    UInt value = 4_b;
    value = 1;

    Bit do_mul_2 = '1';

    // Do the multiplication only if do_mul_2 is asserted
    IF (do_mul_2)
        value <<= 1; // Left shift by one bit to multiply with 2

	sim_assert(value == 2);

	runEvalOnlyTest();		
}

BOOST_FIXTURE_TEST_CASE(tutorial_part2_mutable_condition_scopes_2, BoostUnitTestSimulationFixture)
{
    UInt value = 4_b;
    value = 1;

    Bit do_mul_2_inc = '1';

    IF (do_mul_2_inc) {
        value <<= 1; // Left shift by one bit to multiply with 2
        value += 1; // Increment
    }

	sim_assert(value == 3);

	runEvalOnlyTest();		
}

BOOST_FIXTURE_TEST_CASE(tutorial_part2_mutable_condition_scopes_3, BoostUnitTestSimulationFixture)
{
    UInt value = 4_b;
    value = 1;

    Bit some_condition = '1';

    IF (some_condition) {
        value <<= 1;
    } ELSE {
        value += 1;
    }

	sim_assert(value == 2);

	runEvalOnlyTest();		
}

BOOST_FIXTURE_TEST_CASE(tutorial_part2_structs_packing, BoostUnitTestSimulationFixture)
{
	struct MyFloat {
		// Signals
		UInt mantissa = 23_b;
		UInt exponent = 8_b;
		Bit sign;

		// Meta Information
		unsigned biasOffset = 127;
	};

    MyFloat myFloat;
//    myFloat.mantissa = ...;

    // Packs the struct into one 32-bit word with the
    // last member (mantissa) in the most significant bits
    UInt packed_float = pack(myFloat);
	
	BOOST_TEST(packed_float.size() == 32);

	runEvalOnlyTest();		
}


BOOST_FIXTURE_TEST_CASE(tutorial_part2_structs_unpacking, BoostUnitTestSimulationFixture)
{
    struct MyFloat {
        // Signals
        UInt mantissa = 23_b; // This works like the constructor in the previous example
        UInt exponent = 8_b;
        Bit sign;

        // Meta Information
        unsigned biasOffset = 127;
    };

    MyFloat myFloat; // Constructor resizes all members
    myFloat.mantissa = 42;

    // Packs the struct into one 32-bit word with the
    // last member (mantissa) in the most significant bits
    UInt packed_float = pack(myFloat);

    MyFloat myFloat2; // Constructor resizes all members

    // Unpacks the contents of packed_float into the signals of myFloat2
    unpack(packed_float, myFloat2);
	
	BOOST_TEST(packed_float.size() == 32);
	sim_assert(myFloat2.mantissa == 42);

	runEvalOnlyTest();
}

BOOST_FIXTURE_TEST_CASE(tutorial_part2_structs_constructFrom, BoostUnitTestSimulationFixture)
{
    struct MyFloat {
        // Signals
        UInt mantissa = 23_b; // This works like the constructor in the previous example
        UInt exponent = 8_b;
        Bit sign;

        // Meta Information
        unsigned biasOffset = 127;
    };

    MyFloat myFloat;
    myFloat.exponent = "10b0";
    myFloat.biasOffset = 511;


    MyFloat myFloat2 = constructFrom(myFloat);

	BOOST_TEST(myFloat2.exponent.size() == 10);
	BOOST_TEST(myFloat2.biasOffset == 511);

	runEvalOnlyTest();
}
