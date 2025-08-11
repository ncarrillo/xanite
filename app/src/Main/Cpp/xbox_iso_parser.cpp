#include "xbox_iso_parser.h"
#include <iomanip>
#include <vector>
#include <string>
#include <fstream>
#include <cctype>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <queue>
#include <set>
#include <zlib.h>
#include <android/log.h>
#include <functional>
#include <unordered_map>
#include <zlib.h>

namespace fs = std::filesystem;

uint16_t XboxISOParser::readU16(const uint8_t* data) {
    return data[0] | (data[1] << 8);
}

uint32_t XboxISOParser::readU32(const uint8_t* data) {
    return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

uint64_t XboxISOParser::readU64(const uint8_t* data) {
    return static_cast<uint64_t>(data[0]) |
           (static_cast<uint64_t>(data[1]) << 8) |
           (static_cast<uint64_t>(data[2]) << 16) |
           (static_cast<uint64_t>(data[3]) << 24) |
           (static_cast<uint64_t>(data[4]) << 32) |
           (static_cast<uint64_t>(data[5]) << 40) |
           (static_cast<uint64_t>(data[6]) << 48) |
           (static_cast<uint64_t>(data[7]) << 56);
}

std::string XboxISOParser::toUppercase(const std::string& str) {
    std::string res = str;
    std::transform(res.begin(), res.end(), res.begin(),
        [](unsigned char c) { return std::toupper(c); });
    return res;
}

XboxISOParser::XboxISOParser() : sectorSize(2048), xisoOffset(0), debugOutput(false) {
    rootEntry.name = "Root";
    rootEntry.isDirectory = true;
    rootEntry.fullPath = "";
}

XboxISOParser::~XboxISOParser() {
    if (isoFile.is_open()) {
        isoFile.close();
    }
}

void XboxISOParser::setDebugOutput(bool enabled) {
    debugOutput = enabled;
}

bool XboxISOParser::loadISO(const std::string& isoPath, std::string& errorMsg) {
    
    if (isoFile.is_open()) {
        isoFile.close();
        rootEntry.children.clear();
        pathCache.clear();
        xisoOffset = 0;
    }

    isoFile.open(isoPath, std::ios::binary);
    if (!isoFile.is_open()) {
        errorMsg = "Failed to open ISO file: " + isoPath;
        return false;
    }

    PartitionType partitionType = detectPartitionType();
    switch (partitionType) {
        case PartitionType::STANDARD:
            if (debugOutput) printf("[+] Detected standard ISO format\n");
            break;
        case PartitionType::XISO:
            if (debugOutput) printf("[+] Detected modified XISO format (offset: 0x%X)\n", xisoOffset);
            break;
        case PartitionType::HIDDEN:
            if (debugOutput) printf("[+] Detected hidden partition format\n");
            break;
        case PartitionType::UNKNOWN:
        default:
            errorMsg = "Unsupported or invalid Xbox ISO format";
            return false;
    }

    uint32_t rootSector, rootSize;
    if (!parseXDVDFS(rootSector, rootSize, errorMsg)) {
        errorMsg = "Failed to parse XDVDFS structure: " + errorMsg;
        isoFile.close();
        return false;
    }

    rootEntry.sector = rootSector;
    rootEntry.size = rootSize;

    if (debugOutput) {
        printf("[+] Root directory: sector=%u, size=%u bytes\n", rootSector, rootSize);
    }

    if (!parseDirectory(rootSector, rootSize, rootEntry, "", errorMsg)) {
        isoFile.close();
        return false;
    }

    buildPathCache(rootEntry);
    
    if (debugOutput) {
        printf("[+] ISO parsed successfully. Found %zu entries\n", pathCache.size());
    }
    
    return true;
}

bool XboxISOParser::loadISO(const uint8_t* data, size_t size, std::string& errorMsg) {
    if (data == nullptr || size == 0) {
        errorMsg = "Invalid ISO data.";
        return false;
    }

    iso_data.assign(data, data + size);
    return true;
}

bool XboxISOParser::parse(const uint8_t* data, size_t size, std::string& errorMsg) {
    if (!loadISO(data, size, errorMsg)) {
        return false;
    }

    std::string xbeName;
    uint32_t xbeSize = 0, xbeSector = 0;

    if (!findAnyXBE(xbeName, xbeSize, xbeSector, errorMsg)) {
        return false;
    }

    xbe_data = readFileData(xbeSector, xbeSize);

    if (xbe_data.empty()) {
        errorMsg = "Failed to read XBE data.";
        return false;
    }

    return true;
}

std::vector<uint8_t> XboxISOParser::getXbeData() {
    return xbe_data;
}

bool XboxISOParser::findAnyXBE(std::string& xbeName, uint32_t& xbeSize, uint32_t& xbeSector, std::string& errorMsg) {
    
    const uint8_t* buffer = iso_data.data();
    size_t size = iso_data.size();

    for (size_t i = 0; i < size - 4; ++i) {
        if (buffer[i] == 'X' && buffer[i+1] == 'B' && buffer[i+2] == 'E' && buffer[i+3] == 'H') {
            xbeSector = i / 2048;  
            xbeSize = 0x100000;    
            xbeName = "default.xbe";
            return true;
        }
    }

    errorMsg = "Could not find XBE file in ISO.";
    return false;
}

XboxISOParser::PartitionType XboxISOParser::detectPartitionType() {

    char magic[20] = {0};
    isoFile.seekg(32 * sectorSize, std::ios::beg);
    isoFile.read(magic, sizeof(magic));
    
    if (std::string(magic, sizeof(magic)) == "MICROSOFT*XBOX*MEDIA") {
        return PartitionType::STANDARD;
    }

    isoFile.seekg(0, std::ios::beg);
    isoFile.read(magic, 4);
    if (std::string(magic, 4) == "XISO") {
        xisoOffset = 0x10000;
        return PartitionType::XISO;
    }

    isoFile.seekg(0x8000, std::ios::beg);
    isoFile.read(magic, 4);
    if (std::string(magic, 4) == "CXBX") {
        xisoOffset = 0x8000;
        return PartitionType::XISO;
    }

    const uint32_t hiddenSectors[] = {0x100000, 0x200000, 0x300000, 0x80000};
    const char* hiddenSignatures[] = {"XBOX_HIDDEN", "HIDDEN_PART", "XBOX_SECRET"};

    for (uint32_t sector : hiddenSectors) {
        isoFile.seekg(sector * sectorSize, std::ios::beg);
        
        for (const char* sig : hiddenSignatures) {
            size_t sigLen = strlen(sig);
            std::vector<char> buffer(sigLen);
            isoFile.read(buffer.data(), sigLen);
            
            if (std::string(buffer.data(), sigLen) == sig) {
                xisoOffset = sector * sectorSize;
                return PartitionType::HIDDEN;
            }
        }
    }

    return PartitionType::UNKNOWN;
}

bool XboxISOParser::parseXDVDFS(uint32_t& rootSector, uint32_t& rootSize, std::string& errorMsg) {
    isoFile.seekg(xisoOffset + (32 * sectorSize), std::ios::beg);
    if (!isoFile) {
        errorMsg = "Failed to seek to volume descriptor";
        return false;
    }

    char magic[20];
    isoFile.read(magic, sizeof(magic));
    if (isoFile.gcount() != sizeof(magic)) {
        errorMsg = "Failed to read volume descriptor";
        return false;
    }

    if (std::string(magic, sizeof(magic)) != "MICROSOFT*XBOX*MEDIA") {
        errorMsg = "Invalid Xbox ISO signature";
        return false;
    }

    uint8_t buffer[8];
    isoFile.read(reinterpret_cast<char*>(buffer), sizeof(buffer));
    if (isoFile.gcount() != sizeof(buffer)) {
        errorMsg = "Failed to read root directory info";
        return false;
    }

    rootSector = readU32(buffer);
    rootSize = readU32(buffer + 4);
    return true;
}

bool XboxISOParser::parseDirectory(uint32_t sector, uint32_t size, XboxFileEntry& parent, 
                                 const std::string& parentPath, std::string& errorMsg) {
    if (size == 0) {
        if (debugOutput) {
            printf("[+] Skipping empty directory at sector %u\n", sector);
        }
        return true;
    }

    std::vector<uint8_t> data = readFileData(sector, size, false);
    if (data.size() != size) {
        errorMsg = "Failed to read directory data (expected: " + 
                  std::to_string(size) + ", got: " + 
                  std::to_string(data.size()) + ")";
        return false;
    }

    uint32_t offset = 0;
    const uint32_t dataSize = static_cast<uint32_t>(data.size());

    while (offset < dataSize) {
        const uint8_t entryLength = data[offset];
        if (entryLength == 0) {
            uint32_t sectorOffset = offset % sectorSize;
            uint32_t skip = (sectorOffset == 0) ? sectorSize : (sectorSize - sectorOffset);
            if (offset + skip > dataSize) break;
            offset += skip;
            continue;
        }

        if (offset + entryLength > dataSize) {
            errorMsg = "Directory entry length exceeds buffer size";
            return false;
        }

        const uint8_t attr = data[offset + 1];
        const uint16_t nameLen = readU32(&data[offset + 2]) & 0xFFFF;

        if (offset + 4 + nameLen > dataSize) {
            errorMsg = "Directory entry name exceeds buffer size";
            return false;
        }

        std::string name(reinterpret_cast<const char*>(&data[offset + 4]), nameLen);
        std::string fullPath = parentPath.empty() ? name : parentPath + "/" + name;

        uint32_t nameEnd = offset + 4 + nameLen;
        uint32_t padding = (4 - (nameLen % 4)) % 4;
        uint32_t dataOffset = nameEnd + padding;

        if (dataOffset + 8 > dataSize) {
            errorMsg = "Directory entry data exceeds buffer size";
            return false;
        }

        XboxFileEntry entry;
        entry.name = name;
        entry.sector = readU32(&data[dataOffset]);
        entry.size = readU32(&data[dataOffset + 4]);
        entry.isDirectory = (attr & 0x10) != 0;
        entry.isCompressed = (attr & 0x80) != 0;
        entry.isHidden = (attr & 0x02) != 0;
        entry.fullPath = fullPath;

        if (entry.isDirectory && entry.size > 0) {
            if (debugOutput) {
                printf("[+] Parsing directory: %s (sector=%u, size=%u)\n", 
                      fullPath.c_str(), entry.sector, entry.size);
            }
            
            if (!parseDirectory(entry.sector, entry.size, entry, fullPath, errorMsg)) {
                return false;
            }
        }

        parent.children.push_back(entry);
        offset += entryLength;
    }
    return true;
}

std::string XboxISOParser::sanitizeGameName(const std::string& name) {
    std::string sanitized = name;
    std::replace(sanitized.begin(), sanitized.end(), ' ', '_');
    sanitized.erase(std::remove_if(sanitized.begin(), sanitized.end(),
        [](char c) { return !isalnum(c) && c != '_'; }), sanitized.end());
    return sanitized;
}

std::string XboxISOParser::getAndroidGamePath(const std::string& gameName) {
    std::string sanitized = sanitizeGameName(gameName);
    return "/data/user/0/com.xanite/files/" + sanitized;
}

bool XboxISOParser::setupGameDirectory(const std::string& gameName) {
    std::string gamePath = getAndroidGamePath(gameName);
    try {
        if (!fs::exists(gamePath)) {
            fs::create_directories(gamePath);
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool XboxISOParser::convertISOToXBE(const std::string& isoPath, const std::string& outputDir) {
    std::string errorMsg;
    if (!loadISO(isoPath, errorMsg)) {
        __android_log_print(ANDROID_LOG_ERROR, "XboxParser", "Failed to load ISO: %s", errorMsg.c_str());
        return false;
    }

    std::string xbePath;
    uint32_t xbeSize, xbeSector;
    if (!findMainXBE(xbePath, xbeSize, xbeSector, errorMsg)) {
    __android_log_print(ANDROID_LOG_ERROR, "XboxParser", "No XBE found: %s", errorMsg.c_str());
    return false;
}
    
    std::vector<uint8_t> xbeData = readFileData(xbeSector, xbeSize);
    std::string outputXbePath = outputDir + "/default.xbe";
    std::ofstream outXbe(outputXbePath, std::ios::binary);
    outXbe.write(reinterpret_cast<const char*>(xbeData.data()), xbeData.size());
    outXbe.close();

    return extractAllFiles(outputDir);
}

bool XboxISOParser::processISO(const std::string& isoPath) {
 
    fs::path pathObj(isoPath);
    std::string gameName = pathObj.stem().string(); 
    
    std::string androidPath = getAndroidGamePath(gameName);
    if (!setupGameDirectory(gameName)) {
        __android_log_print(ANDROID_LOG_ERROR, "XboxParser", "Failed to create game directory: %s", androidPath.c_str());
        return false;
    }

    return convertISOToXBE(isoPath, androidPath);
}

void XboxISOParser::buildPathCache(XboxFileEntry& entry) {
    pathCache[toUppercase(entry.fullPath)] = &entry;
    if (debugOutput && !entry.isDirectory) {
        printf("[+] File: %s (sector=%u, size=%u, hidden=%s, compressed=%s)\n",
              entry.fullPath.c_str(), entry.sector, entry.size,
              entry.isHidden ? "yes" : "no", entry.isCompressed ? "yes" : "no");
    }
    
    for (auto& child : entry.children) {
        buildPathCache(child);
    }
}

const XboxFileEntry* XboxISOParser::findEntry(const std::string& path) const {
    auto it = pathCache.find(toUppercase(path));
    return (it != pathCache.end()) ? it->second : nullptr;
}

bool XboxISOParser::fileExists(const std::string& path) const {
    return findEntry(path) != nullptr;
}

const XboxFileEntry* XboxISOParser::getRootEntry() const {
    return &rootEntry;
}

bool XboxISOParser::findMainXBE(std::string& xbePath, uint32_t& xbeSize, 
                              uint32_t& xbeSector, std::string& errorMsg) {
    const std::vector<std::string> commonPaths = {
        "DEFAULT.XBE",
        "DASHBOARD.XBE",
        "VIDEO_TS/DEFAULT.XBE",
        "DATA/DEFAULT.XBE",
        "GAMES/DEFAULT.XBE",
        "ROOT/DEFAULT.XBE",
        "XBE/DEFAULT.XBE",
        "SYSTEM/DEFAULT.XBE"
    };

    for (const auto& path : commonPaths) {
        const XboxFileEntry* entry = findEntry(path);
        if (entry && !entry->isDirectory) {
            xbePath = entry->fullPath;
            xbeSize = entry->size;
            xbeSector = entry->sector;
            
            if (debugOutput) {
                printf("[+] Found XBE at common path: %s\n", xbePath.c_str());
            }
            
            return true;
        }
    }

    std::queue<const XboxFileEntry*> q;
    q.push(&rootEntry);
    int xbeCount = 0;

    while (!q.empty()) {
        const XboxFileEntry* current = q.front();
        q.pop();

        for (const auto& child : current->children) {
            if (child.isDirectory) {
                q.push(&child);
            } else {
                std::string upperName = toUppercase(child.name);
                if (upperName.size() > 4 && upperName.substr(upperName.size() - 4) == ".XBE") {
                    xbeCount++;
                    if (debugOutput) {
                        printf("[+] Found XBE candidate: %s\n", child.fullPath.c_str());
                    }
                    
                    
                    if (!child.isHidden && !child.isCompressed) {
                        xbePath = child.fullPath;
                        xbeSize = child.size;
                        xbeSector = child.sector;
                        
                        if (debugOutput) {
                            printf("[+] Selected XBE: %s\n", xbePath.c_str());
                        }
                        
                        return true;
                    }
                }
            }
        }
    }

    if (xbeCount > 0) {
        errorMsg = "Found " + std::to_string(xbeCount) + " XBE files but none selected as main";
    } else {
        errorMsg = "No XBE files found in ISO";
    }
    
    return false;
}

std::vector<uint8_t> XboxISOParser::readFileData(uint32_t sector, uint32_t size, bool decompress) {
    std::vector<uint8_t> data;
    if (size == 0) return data;

    if (!iso_data.empty()) {
        
        size_t offset = sector * 2048;
        if (offset + size > iso_data.size()) {
            size = iso_data.size() - offset;
        }

        data = std::vector<uint8_t>(iso_data.begin() + offset, iso_data.begin() + offset + size);
    } else {
      
        const std::streampos pos = xisoOffset + static_cast<std::streampos>(sector) * sectorSize;
        isoFile.seekg(pos, std::ios::beg);
        
        if (!isoFile) {
            return data;
        }

        data.resize(size);
        isoFile.read(reinterpret_cast<char*>(data.data()), size);
        
        if (static_cast<uint32_t>(isoFile.gcount()) != size) {
            data.resize(static_cast<size_t>(isoFile.gcount()));
        }
    }

    if (data.size() >= 256 && memcmp(data.data() + 0x10, "MICROSOFT*XBOX*MEDIA", 20) == 0) {
        if (debugOutput) {
            printf("[+] Found Xbox security sector, skipping first 256 bytes\n");
        }
        data.erase(data.begin(), data.begin() + 256);
    }

    if (data.size() >= 4 && memcmp(data.data(), "Xe", 2) == 0) {
        if (debugOutput) {
            printf("[+] Decrypting protected file\n");
        }
        data = decryptXboxFile(data);
    }

    if (decompress && isXboxCompressed(data.data(), data.size())) {
        if (debugOutput) {
            printf("[+] Decompressing XBC file\n");
        }
        data = decompressXBC(data);
    }

    return data;
}

std::vector<uint8_t> XboxISOParser::decryptXboxFile(const std::vector<uint8_t>& data) const {
    
    if (data.size() < 8) return data;

    if (memcmp(data.data(), "Xe", 2) != 0) return data;

    uint32_t decryptedSize = readU32(data.data() + 4);
    std::vector<uint8_t> decrypted(decryptedSize);

    const uint8_t* src = data.data() + 8;
    uint8_t* dst = decrypted.data();
    size_t size = std::min<size_t>(data.size() - 8, decryptedSize);    
    for (size_t i = 0; i < size; i++) {
        dst[i] = src[i] ^ 0xA5; 
    }

    return decrypted;
}

std::vector<uint8_t> XboxISOParser::decompressXBC(const std::vector<uint8_t>& data) const {

    if (data.size() < 8) return data;


    if (memcmp(data.data(), "XBC", 3) != 0) return data;


    uint32_t decompressedSize = readU32(data.data() + 4);
    std::vector<uint8_t> decompressed(decompressedSize);


    z_stream zs;
    memset(&zs, 0, sizeof(zs));

    if (inflateInit(&zs) != Z_OK) {
        return data;
    }

     zs.next_in = const_cast<uint8_t*>(data.data() + 8);
    zs.avail_in = static_cast<uInt>(data.size() - 8);
    zs.next_out = decompressed.data();
    zs.avail_out = decompressedSize;

    if (inflate(&zs, Z_FINISH) != Z_STREAM_END) {
        inflateEnd(&zs);
        return data;
    }

    inflateEnd(&zs);
    return decompressed;
}

bool XboxISOParser::isXboxCompressed(const uint8_t* data, size_t size) const {
    return size >= 4 && memcmp(data, "XBC", 3) == 0;
}

bool XboxISOParser::extractFile(const std::string& isoPath, const std::string& outputPath, 
                              std::string& errorMsg) {
    const XboxFileEntry* entry = findEntry(isoPath);
    if (!entry) {
        errorMsg = "File not found in ISO: " + isoPath;
        return false;
    }

    if (entry->isDirectory) {
        errorMsg = "Path is a directory: " + isoPath;
        return false;
    }

    std::vector<uint8_t> fileData = readFileData(entry->sector, entry->size, true);
    if (fileData.size() != entry->size) {
        errorMsg = "Failed to read complete file data (expected: " + 
                  std::to_string(entry->size) + ", got: " + 
                  std::to_string(fileData.size()) + ")";
        return false;
    }

    fs::path outPath(outputPath);
    if (!outPath.parent_path().empty()) {
        fs::create_directories(outPath.parent_path());
    }

    std::ofstream outFile(outputPath, std::ios::binary);
    if (!outFile) {
        errorMsg = "Failed to create output file: " + outputPath;
        return false;
    }

    outFile.write(reinterpret_cast<const char*>(fileData.data()), fileData.size());
    outFile.close();

    if (debugOutput) {
        printf("[+] Extracted file: %s -> %s (%zu bytes)\n", 
              isoPath.c_str(), outputPath.c_str(), fileData.size());
    }

    return true;
}

bool XboxISOParser::extractAllFiles(const std::string& outputDir, 
                                  std::function<void(const std::string&, bool)> progressCallback) {
    if (!isoFile.is_open() && iso_data.empty()) {
        return false;
    }

    std::queue<const XboxFileEntry*> directories;
    directories.push(&rootEntry);
    int fileCount = 0;
    int dirCount = 0;

    while (!directories.empty()) {
        const XboxFileEntry* currentDir = directories.front();
        directories.pop();
        
        std::string dirPath = outputDir + "/" + currentDir->fullPath;
        if (currentDir != &rootEntry) {
            fs::create_directories(dirPath);
            dirCount++;
            
            if (progressCallback) {
                progressCallback(currentDir->fullPath, true);
            }
        }

        for (const auto& entry : currentDir->children) {
            if (entry.isDirectory) {
                directories.push(&entry);
            } else {
                std::string filePath = outputDir + "/" + entry.fullPath;
                 
                auto fileData = readFileData(entry.sector, entry.size, true);
                        
                fs::path outPath(filePath);
                if (!outPath.parent_path().empty()) {
                    fs::create_directories(outPath.parent_path());
                }

                std::ofstream outFile(filePath, std::ios::binary);
                if (outFile) {
                    outFile.write(reinterpret_cast<const char*>(fileData.data()), fileData.size());
                    fileCount++;
                    
                    if (progressCallback) {
                        progressCallback(entry.fullPath, false);
                    }
                }
            }
        }
    }
    
    if (debugOutput) {
        printf("[+] Extracted %d files and %d directories to %s\n", 
              fileCount, dirCount, outputDir.c_str());
    }
    
    return true;
}