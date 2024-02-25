cmake_minimum_required(VERSION 3.20)

project("LSPConvert" VERSION 1.25)

set(SourceFiles
    src/LSPConvert.cpp
    src/LSPDecoder.cpp
    src/LSPDecoder.h
    src/LSPEncoder.cpp
    src/LSPEncoder.h
    src/LSPTypes.h

    src/WindowsCompat.cpp
    src/WindowsCompat.h

    src/MemoryStream.cpp
    src/MemoryStream.h
    src/PackingEstimate.cpp
    src/Paula.cpp
    src/Paula.h
    src/ValueEncoder.cpp
    src/ValueEncoder.h
    src/WavWriter.cpp
    src/WavWriter.h
    src/crc32.cpp
    src/crc32.h
    src/external/micromod/micromod.cpp
    src/external/micromod/micromod.h
    src/external/Shrinkler/AmigaWords.h
    src/external/Shrinkler/assert.h
    src/external/Shrinkler/Coder.h
    src/external/Shrinkler/CountingCoder.h
    src/external/Shrinkler/CuckooHash.h
    src/external/Shrinkler/DataFile.h
    src/external/Shrinkler/Decoder.h
    src/external/Shrinkler/DecrunchHeaders.h
    src/external/Shrinkler/doshunks.h
    src/external/Shrinkler/Heap.h
    src/external/Shrinkler/HunkFile.h
    src/external/Shrinkler/LZDecoder.h
    src/external/Shrinkler/LZEncoder.h
    src/external/Shrinkler/LZParser.h
    src/external/Shrinkler/MatchFinder.h
    src/external/Shrinkler/Pack.h
    src/external/Shrinkler/RangeCoder.h
    src/external/Shrinkler/RangeDecoder.h
    src/external/Shrinkler/SizeMeasuringCoder.h
    src/external/Shrinkler/SuffixArray.h
)

add_executable(${PROJECT_NAME} ${SourceFiles})

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)

set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE INTERNAL "")

if(UNIX)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _MAX_PATH=260
        _MAX_DRIVE=3
        _MAX_DIR=256
        _MAX_FNAME=256
        _MAX_EXT=256
        WINDOWS=0
        MACOS_LINUX=1
        LSP_MAJOR_VERSION=${PROJECT_VERSION_MAJOR}
        LSP_MINOR_VERSION=${PROJECT_VERSION_MINOR}
    )
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        WINDOWS=1
        LSP_MAJOR_VERSION=${PROJECT_VERSION_MAJOR}
        LSP_MINOR_VERSION=${PROJECT_VERSION_MINOR}
    )
endif()