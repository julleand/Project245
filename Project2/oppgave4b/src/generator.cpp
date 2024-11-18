// Written by K. M. Knausgård 2023-10-21
#include <array>
#include <bitset>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "mas245_logo_gimp_export.h"

namespace kmk
{
    /**
     * This part is based on C++ features template metaprogramming with variadic templates and parameter pack expansion.
     * (Not something you need to learn for MAS245 or MAS417).
     * 
     * Minimum C++ version for this is C++14.
     * 
     * Enables compile-time initialization of C++ std::array from C-style array, and also conversions. No run-time penalty,
     * and also memory efficient. Could do this directly in the teensy project with a proper compiler, for now it is a separate executable.
    */
    template<std::size_t N, std::size_t... Indices>
    constexpr auto to_array_impl(const unsigned char (&arr)[N], std::index_sequence<Indices...>) {
        return std::array<uint8_t, N>{arr[Indices]...};
    }

    template<std::size_t N>
    constexpr auto to_array(const unsigned char (&arr)[N]) {
        return to_array_impl(arr, std::make_index_sequence<N>{});
    }

    template<std::size_t N>
    constexpr auto compressBinaryArray(const std::array<uint8_t, N>& data) {
        std::array<uint8_t, N / 8> compressedArray{};
        for (std::size_t i = 0; i < N; i += 8) {
            for (std::size_t j = 0; j < 8; ++j) {
                compressedArray[i / 8] |= (data[i + j] << (7 - j)); // Fill in leftmost (MSB) bit first.
            }
        }
        return compressedArray;
    }
}


int main()
{
    std::cout << "Starting binary logo generator.." << std::endl;

    constexpr auto data_array = kmk::to_array(header_data);
    constexpr auto compressedArray = kmk::compressBinaryArray(data_array);

    std::filesystem::path dir("include");
    std::filesystem::path file("mas245_logo_bitmap.h");
    std::filesystem::path fullPath = dir / file;

    std::ofstream outFile(fullPath);
    if (!outFile) {
        std::cerr << "Error opening file for writing\n";
        return 1;
    }

    // Hard-coded template. A better option would be to create a template file for this using XML or JSON.
    outFile << "#ifndef MAS245_LOGO_BITMAP_H\n";
    outFile << "#define MAS245_LOGO_BITMAP_H\n\n";
    outFile << "#include <avr/pgmspace.h>\n\n";
    outFile << "namespace images {\n";
    outFile << "    namespace mas245splash {\n";
    outFile << "        constexpr uint8_t width{" << width << "};\n\n";
    outFile << "        constexpr uint8_t height{" << height << "};\n";
    outFile << "        static const uint8_t PROGMEM bitmap[] = {\n";
   

    for (const auto& byte : compressedArray) {
        outFile << "            " << "0b" << std::bitset<8>(byte) << ",\n";
    }

    outFile << "        };\n\n";
    outFile << "    };\n\n";
    outFile << "};\n\n";
    outFile << "#endif // MAS245_LOGO_BITMAP_H\n";

    outFile.close();

    std::cout << "Generated file " << fullPath << " successfully (hopefully, did not check for errors)." << std::endl;

    return 0;
}
