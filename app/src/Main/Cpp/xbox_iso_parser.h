#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <cctype>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <unordered_map>

struct XboxFileEntry {
    std::string name;
    uint32_t sector;
    uint32_t size;
    bool isDirectory;
    bool isCompressed;
    bool isHidden;
    std::string fullPath;
    std::vector<XboxFileEntry> children;
};

class XboxISOParser {
public:
    enum class PartitionType {
        STANDARD,
        XISO,
        HIDDEN,
        UNKNOWN
    };

    XboxISOParser();
    ~XboxISOParser();

    bool loadISO(const std::string& isoPath, std::string& errorMsg);
    bool loadISO(const uint8_t* data, size_t size, std::string& errorMsg);
    bool parse(const uint8_t* data, size_t size, std::string& errorMsg);
    std::vector<uint8_t> getXbeData();
    void setDebugOutput(bool enabled);

    bool findMainXBE(std::string& xbePath, uint32_t& xbeSize, uint32_t& xbeSector, std::string& errorMsg);
    bool findAnyXBE(std::string& xbeName, uint32_t& xbeSize, uint32_t& xbeSector, std::string& errorMsg);
    const XboxFileEntry* findEntry(const std::string& path) const;
    bool fileExists(const std::string& path) const;
    const XboxFileEntry* getRootEntry() const;
    
    std::vector<uint8_t> readFileData(uint32_t sector, uint32_t size, bool decompress = false);
    
    bool extractFile(const std::string& isoPath, const std::string& outputPath, std::string& errorMsg);
    bool extractAllFiles(const std::string& outputDir, std::function<void(const std::string&, bool)> progressCallback = nullptr);

    static std::string sanitizeGameName(const std::string& name);
    static std::string getAndroidGamePath(const std::string& gameName);
    static bool setupGameDirectory(const std::string& gameName);
    bool convertISOToXBE(const std::string& isoPath, const std::string& outputDir);
    bool processISO(const std::string& isoPath);

private:
    
    static uint16_t readU16(const uint8_t* data);
    static uint32_t readU32(const uint8_t* data);
    static uint64_t readU64(const uint8_t* data);
    static std::string toUppercase(const std::string& str);
       
    PartitionType detectPartitionType();
    bool parseXDVDFS(uint32_t& rootSector, uint32_t& rootSize, std::string& errorMsg);
    bool parseDirectory(uint32_t sector, uint32_t size, XboxFileEntry& parent, 
                       const std::string& parentPath, std::string& errorMsg);
    void buildPathCache(XboxFileEntry& entry);
    
    std::vector<uint8_t> decryptXboxFile(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> decompressXBC(const std::vector<uint8_t>& data) const;
    bool isXboxCompressed(const uint8_t* data, size_t size) const;

    std::ifstream isoFile;
    std::vector<uint8_t> iso_data;
    std::vector<uint8_t> xbe_data;
    XboxFileEntry rootEntry;
    std::unordered_map<std::string, XboxFileEntry*> pathCache;
    const uint32_t sectorSize;
    uint32_t xisoOffset;
    bool debugOutput;
};