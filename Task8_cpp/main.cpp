#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <cstdint>
#include <iomanip>

const uint16_t WINDOW_SIZE = 4095;
const uint8_t LOOKAHEAD_SIZE = 15;
const std::string MAGIC_HEADER = "LZ77"; // для проверки архива

//--------------------------
// Чтение файла в память
//--------------------------
std::vector<uint8_t> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    if (file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return buffer;
    }
    throw std::runtime_error("Error reading file: " + filename);
}

//--------------------------
// Запись файла из памяти
//--------------------------
void writeFile(const std::string& filename, const std::vector<uint8_t>& data) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for writing: " + filename);
    }
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
}

bool isASCII(const std::vector<uint8_t>& data) {
    for (uint8_t c : data) {
        if (c > 127) return false;
    }
    return true;
}

//--------------------------
// Сжатие LZ77
//--------------------------
std::vector<uint8_t> compressLZ77(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> compressed;

    compressed.insert(compressed.end(), MAGIC_HEADER.begin(), MAGIC_HEADER.end());
    uint64_t originalSize = data.size();
    for (int i = 0; i < 8; ++i) {
        compressed.push_back(static_cast<uint8_t>((originalSize >> (i * 8)) & 0xFF));
    }

    std::vector<std::vector<int>> charPositions(128);

    int pos = 0;
    while (pos < data.size()) {
        int bestOffset = 0;
        int bestLength = 0;

        uint8_t currentChar = data[pos];
        int searchStart = std::max(0, pos - WINDOW_SIZE);

        for (int p = charPositions[currentChar].size() - 1; p >= 0; --p) {
            int matchPos = charPositions[currentChar][p];
            if (matchPos < searchStart) break;

            int len = 0;
            while (len < LOOKAHEAD_SIZE && pos + len < data.size() && data[matchPos + len] == data[pos + len]) {
                len++;
            }

            if (len > bestLength) {
                bestLength = len;
                bestOffset = pos - matchPos;
            }
            if (bestLength == LOOKAHEAD_SIZE) break;
        }

        uint8_t nextChar = (pos + bestLength < data.size()) ? data[pos + bestLength] : 0;

        uint8_t byte1 = static_cast<uint8_t>(bestOffset >> 4);
        uint8_t byte2 = static_cast<uint8_t>(((bestOffset & 0x0F) << 4) | (bestLength & 0x0F));
        uint8_t byte3 = nextChar;

        compressed.push_back(byte1);
        compressed.push_back(byte2);
        compressed.push_back(byte3);

        for (int i = 0; i <= bestLength && pos + i < data.size(); ++i) {
            uint8_t c = data[pos + i];
            if (c <= 127) {
                charPositions[c].push_back(pos + i);
            }
        }

        pos += bestLength + 1;
    }

    return compressed;
}

//--------------------------
// Распаковка LZ77
//--------------------------
std::vector<uint8_t> decompressLZ77(const std::vector<uint8_t>& compressed) {
    if (compressed.size() < MAGIC_HEADER.size() + 8) {
        throw std::invalid_argument("The archive is corrupt!");
    }

    std::string header(compressed.begin(), compressed.begin() + MAGIC_HEADER.size());
    if (header != MAGIC_HEADER) {
        throw std::invalid_argument("It is not a .compressed archive!");
    }

    uint64_t originalSize = 0;
    int metaOffset = MAGIC_HEADER.size();
    for (int i = 0; i < 8; ++i) {
        originalSize |= (static_cast<uint64_t>(compressed[metaOffset + i]) << (i * 8));
    }

    std::vector<uint8_t> decompressed;
    decompressed.reserve(originalSize);

    int pos = metaOffset + 8;
    while (pos < compressed.size()) {
        if (pos + 2 >= compressed.size()) {
            throw std::invalid_argument("The archive is corrupt!");
        }

        uint8_t byte1 = compressed[pos++];
        uint8_t byte2 = compressed[pos++];
        uint8_t byte3 = compressed[pos++];

        int offset = (byte1 << 4) | (byte2 >> 4);
        int length = byte2 & 0x0F;
        uint8_t nextChar = byte3;

        int currentDecPos = decompressed.size();
        for (int i = 0; i < length; ++i) {
            if (currentDecPos - offset + i < 0 || currentDecPos - offset + i >= decompressed.size()) {
                 throw std::invalid_argument("The archive is corrupt!");
            }
            decompressed.push_back(decompressed[currentDecPos - offset + i]);
        }
        
        if (decompressed.size() < originalSize) {
             decompressed.push_back(nextChar);
        }
    }

    if (decompressed.size() != originalSize) {
        throw std::invalid_argument("The archive is corrupt!");
    }

    return decompressed;
}

void printHelp() {
    std::cout << "This is LZ77 coding compressor for ASCII text files only.\n\n"
              << "General options:\n"
              << "  -c [ --compress ]          Compress file\n"
              << "                              Example:\n"
              << "                              -c input.txt output.compressed\n\n"
              << "  -d [ --decompress ]        Decompress file\n"
              << "                              Example:\n"
              << "                              -d input.compressed output.txt\n\n"
              << "  -h [ --help ]              Show this help\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printHelp();
        return 0;
    }

    std::string flag = argv[1];

    if (flag == "-h" || flag == "--help") {
        printHelp();
        return 0;
    }

    if (flag == "-c" || flag == "--compress" || flag == "-d" || flag == "--decompress") {
        if (argc < 4) {
            std::cout << "Fail! The output file must be specified!\n";
            return 1;
        }

        std::string inputFile = argv[2];
        std::string outputFile = argv[3];

        try {
            std::vector<uint8_t> inputData = readFile(inputFile);

            if (flag == "-c" || flag == "--compress") {
                if (!isASCII(inputData)) {
                    std::cout << "Fail! The input file must be ASCII with 128 characters only!\n";
                    return 1;
                }

                auto start = std::chrono::high_resolution_clock::now();
                std::vector<uint8_t> compressedData = compressLZ77(inputData);
                auto end = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double> diff = end - start;

                writeFile(outputFile, compressedData);

                double ratio = static_cast<double>(inputData.size()) / compressedData.size();

                std::cout << "Done!\n";
                std::cout << "Compression ratio: " << inputData.size() << " bytes / " 
                          << compressedData.size() << " bytes = " 
                          << std::fixed << std::setprecision(2) << ratio << "\n";
                std::cout << "Compression time: " << diff.count() << " sec\n";

            } else {
                auto start = std::chrono::high_resolution_clock::now();
                std::vector<uint8_t> decompressedData = decompressLZ77(inputData);
                auto end = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double> diff = end - start;

                writeFile(outputFile, decompressedData);

                std::cout << "Done!\n";
                std::cout << "Decompression time: " << std::fixed << std::setprecision(3) << diff.count() << " sec\n";
            }
        } catch (const std::invalid_argument& e) {
            std::cout << "Fail! " << e.what() << "\n";
            return 1;
        } catch (const std::exception& e) {
            std::cout << "Fail! " << e.what() << "\n";
            return 1;
        }
    } else {
        std::cout << "Fail! Unknown option.\n";
        printHelp();
        return 1;
    }

    return 0;
}