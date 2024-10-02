cmake_minimum_required (VERSION 3.5)

# message("Running GtryFilesToResourceFile.cmake")
# message("Output: " ${OUTPUT})
# message("Source dir: " ${SOURCE_DIR})
# foreach (file ${FILES})
# 	message("data file: " ${file})
# endforeach()

set(header_file ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT}.h)
set(source_file ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT}.cpp)

file(WRITE ${header_file} "")
file(WRITE ${source_file} "")

file(APPEND ${header_file} "#pragma once\n// Generated file, do not modify\n\n")
file(APPEND ${header_file} "#include <span>\n#include <string_view>\n#include <array>\n#include <cstddef>\n\n")
file(APPEND ${header_file} "namespace " ${NAMESPACE} " {\n\n")


file(APPEND ${source_file} "// Generated file, do not modify\n\n")
get_filename_component(include_filename ${OUTPUT} NAME)
file(APPEND ${source_file} "#include \"" ${include_filename} ".h\"\n\n")
file(APPEND ${source_file} "namespace " ${NAMESPACE} " {\n\n")

set(manifest_content "")

foreach (file ${FILES})
	message("Procesing " ${file})

	file(SIZE ${SOURCE_DIR}/${file} file_content_length)

	string(REGEX REPLACE "[^a-zA-Z0-9_]" "_" noInvalidChars "${file}")
	string(REGEX REPLACE "^[^a-zA-Z_]+" "" identifier "${noInvalidChars}")

	file(APPEND ${header_file} "extern const std::uint8_t " ${identifier} "_data[" ${file_content_length} "];\n")
	file(APPEND ${header_file} "static constexpr size_t " ${identifier} "_size = " ${file_content_length} ";\n")

	file(APPEND ${source_file} "const std::uint8_t " ${identifier} "_data[" ${file_content_length} "] = {\n")

	foreach(chunk_start RANGE 0 ${file_content_length}-1 64)
		file(READ ${SOURCE_DIR}/${file} file_content OFFSET ${chunk_start} LIMIT 64 HEX)	
		string(REGEX REPLACE "(..)" "0x\\1, " file_content_formatted "${file_content}")
		file(APPEND ${source_file} "    " ${file_content_formatted} "\n")
	endforeach()

	file(APPEND ${source_file} "};\n")

	string(APPEND manifest_content "   ManifestEntry{ .filename = \"" ${file} "\"sv, .data = std::span<const std::byte>((const std::byte*) " ${identifier} "_data, " ${identifier} "_size) },\n")
endforeach()

list(LENGTH FILES num_files)


file(APPEND ${source_file} "using namespace std::literals;\n")
file(APPEND ${source_file} "const std::array<ManifestEntry, " ${num_files} "> manifest = {\n")
file(APPEND ${source_file} ${manifest_content})
file(APPEND ${source_file} "};\n")


file(APPEND ${header_file} "\n\n")

file(APPEND ${header_file} "struct ManifestEntry { std::string_view filename; std::span<const std::byte> data; };\n\n")

file(APPEND ${header_file} "extern const std::array<ManifestEntry, " ${num_files} "> manifest;\n\n")

file(APPEND ${header_file} "}\n")
file(APPEND ${source_file} "}\n")
