cmake_minimum_required (VERSION 3.5)

# message("Running GtryFilesToResourceFile.cmake")
# message("Output: " ${OUTPUT})
# message("Source dir: " ${SOURCE_DIR})
# foreach (file ${FILES})
# 	message("data file: " ${file})
# endforeach()

set(header_content "")
string(APPEND header_content "#pragma once\n// Generated file, do not modify\n\n")
string(APPEND header_content "#include <span>\n#include <string_view>\n#include <array>\n#include <cstddef>\n\n")
string(APPEND header_content "namespace gtry::res {\n\n")


set(source_content "")
string(APPEND source_content "// Generated file, do not modify\n\n")
get_filename_component(include_filename ${OUTPUT} NAME)
string(APPEND source_content "#include \"" ${include_filename} ".h\"\n\n")
string(APPEND source_content "namespace gtry::res {\n\n")

set(manifest_content "")

foreach (file ${FILES})
	message("Procesing " ${file})

	file(SIZE ${SOURCE_DIR}/${file} file_content_length)

	string(REGEX REPLACE "[^a-zA-Z0-9_]" "_" noInvalidChars "${file}")
	string(REGEX REPLACE "^[^a-zA-Z_]+" "" identifier "${noInvalidChars}")

	string(APPEND header_content "extern const std::uint8_t " ${identifier} "_data[" ${file_content_length} "]\\;\n")
	string(APPEND header_content "static constexpr size_t " ${identifier} "_size = " ${file_content_length} "\\;\n")

	string(APPEND source_content "const std::uint8_t " ${identifier} "_data[" ${file_content_length} "] = {\n")

	foreach(chunk_start RANGE 0 ${file_content_length}-1 64)
		file(READ ${SOURCE_DIR}/${file} file_content OFFSET ${chunk_start} LIMIT 64 HEX)	
		string(REGEX REPLACE "(..)" "0x\\1, " file_content_formatted "${file_content}")
		string(APPEND source_content "    " ${file_content_formatted} "\n")
	endforeach()

	string(APPEND source_content "}\\;\n")

	string(APPEND manifest_content "   ManifestEntry{ .filename = \"" ${file} "\"sv, .data = std::span<const std::byte>((const std::byte*) " ${identifier} "_data, " ${identifier} "_size) },\n")
endforeach()

list(LENGTH FILES num_files)


string(APPEND source_content "using namespace std::literals\\;\n")
string(APPEND source_content "const std::array<ManifestEntry, " ${num_files} "> manifest = {\n")
string(APPEND source_content ${manifest_content})
string(APPEND source_content "}\\;\n")


string(APPEND header_content "\n\n")

string(APPEND header_content "struct ManifestEntry { std::string_view filename\\; std::span<const std::byte> data\\; }\\;\n\n")

string(APPEND header_content "extern const std::array<ManifestEntry, " ${num_files} "> manifest\\;\n\n")

string(APPEND header_content "}\n")
string(APPEND source_content "}\n")

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT}.h ${header_content})
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT}.cpp ${source_content})
