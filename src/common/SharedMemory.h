#pragma once

#include <windows.h>
#include <cstdint>
#include <string>

// ReelCam Shared Memory Protocol
// Facilitates ultra-low-latency inter-process frame exchange between
// reelcam_core.exe (Producer) and mfsource.dll (Consumer).

namespace ReelCam {

static constexpr uint32_t SHM_MAGIC = 0x5245454C; // 'REEL'
static constexpr uint32_t SHM_VERSION = 1;
static constexpr uint32_t MAX_FRAME_WIDTH = 1920;
static constexpr uint32_t MAX_FRAME_HEIGHT = 1080;
static constexpr uint32_t NV12_MAX_BUFFER_SIZE = MAX_FRAME_WIDTH * MAX_FRAME_HEIGHT * 3 / 2; // 3,110,400 bytes

// Format identifiers
enum class FrameFormat : uint32_t {
    NV12 = 0,
    RGB32 = 1,
    YUY2 = 2
};

#pragma pack(push, 8)
struct SharedFrameHeader {
    uint32_t magic;              // Must be SHM_MAGIC
    uint32_t version;            // Must be SHM_VERSION
    uint64_t sequenceNumber;     // Monotonically increasing frame counter
    uint64_t timestampQPC;       // QueryPerformanceCounter at frame emission
    int64_t  ptsNanoSeconds;     // Presentation timestamp in 100ns units
    uint32_t width;              // e.g. 1920
    uint32_t height;             // e.g. 1080
    uint32_t format;             // FrameFormat
    uint32_t strideY;            // Luma stride in bytes
    uint32_t strideUV;           // Chroma stride in bytes
    uint32_t dataOffset;         // Offset from start of mapping to pixel buffer
    uint32_t dataSize;           // Total payload size in bytes
    uint32_t heartbeat;          // Heartbeat counter incremented every frame
    uint32_t isCoreActive;       // 1 if core is publishing, 0 if idle/stopped
    uint32_t watermarkEnabled;   // 1 if watermark badge is baked in
    uint32_t mirrorEnabled;      // 1 if horizontal flip applied
    uint32_t reserved[16];       // Future expansion
};
#pragma pack(pop)

static constexpr const wchar_t* SHM_MAPPING_NAME = L"Local\\ReelCam_FrameBuffer";
static constexpr const wchar_t* EVT_FRAME_AVAILABLE = L"Local\\ReelCam_FrameAvailable";
static constexpr const wchar_t* EVT_FRAME_CONSUMED = L"Local\\ReelCam_FrameConsumed";

// Total mapping size: header + double buffer to avoid tearing
static constexpr size_t SHM_TOTAL_SIZE = sizeof(SharedFrameHeader) + (NV12_MAX_BUFFER_SIZE * 2) + 4096;

class SharedMemoryProducer {
public:
    SharedMemoryProducer() = default;
    ~SharedMemoryProducer() { Close(); }

    bool Initialize() {
        m_hMapFile = CreateFileMappingW(
            INVALID_HANDLE_VALUE,
            nullptr,
            PAGE_READWRITE,
            0,
            static_cast<DWORD>(SHM_TOTAL_SIZE),
            SHM_MAPPING_NAME
        );
        if (!m_hMapFile) return false;

        m_pBuffer = MapViewOfFile(m_hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, SHM_TOTAL_SIZE);
        if (!m_pBuffer) {
            Close();
            return false;
        }

        m_hEvtAvailable = CreateEventW(nullptr, FALSE, FALSE, EVT_FRAME_AVAILABLE);
        m_hEvtConsumed = CreateEventW(nullptr, FALSE, FALSE, EVT_FRAME_CONSUMED);

        auto* header = reinterpret_cast<SharedFrameHeader*>(m_pBuffer);
        memset(header, 0, sizeof(SharedFrameHeader));
        header->magic = SHM_MAGIC;
        header->version = SHM_VERSION;
        header->dataOffset = static_cast<uint32_t>(sizeof(SharedFrameHeader));
        header->isCoreActive = 1;

        return true;
    }

    void PublishFrame(const uint8_t* pNv12Data, uint32_t width, uint32_t height,
                      uint32_t strideY, uint32_t strideUV, int64_t pts100ns,
                      bool watermark = true, bool mirror = false) {
        if (!m_pBuffer) return;

        auto* header = reinterpret_cast<SharedFrameHeader*>(m_pBuffer);
        uint8_t* pDst = static_cast<uint8_t*>(m_pBuffer) + header->dataOffset;

        uint32_t dataSize = height * strideY + (height / 2) * strideUV;
        if (dataSize <= NV12_MAX_BUFFER_SIZE && pNv12Data) {
            memcpy(pDst, pNv12Data, dataSize);
        }

        LARGE_INTEGER qpc;
        QueryPerformanceCounter(&qpc);

        header->sequenceNumber++;
        header->timestampQPC = static_cast<uint64_t>(qpc.QuadPart);
        header->ptsNanoSeconds = pts100ns;
        header->width = width;
        header->height = height;
        header->format = static_cast<uint32_t>(FrameFormat::NV12);
        header->strideY = strideY;
        header->strideUV = strideUV;
        header->dataSize = dataSize;
        header->heartbeat++;
        header->isCoreActive = 1;
        header->watermarkEnabled = watermark ? 1 : 0;
        header->mirrorEnabled = mirror ? 1 : 0;

        // Signal waiting consumer
        if (m_hEvtAvailable) {
            SetEvent(m_hEvtAvailable);
        }
    }

    void SetActive(bool active) {
        if (m_pBuffer) {
            auto* header = reinterpret_cast<SharedFrameHeader*>(m_pBuffer);
            header->isCoreActive = active ? 1 : 0;
        }
    }

    void Close() {
        if (m_pBuffer) {
            auto* header = reinterpret_cast<SharedFrameHeader*>(m_pBuffer);
            header->isCoreActive = 0;
            UnmapViewOfFile(m_pBuffer);
            m_pBuffer = nullptr;
        }
        if (m_hMapFile) {
            CloseHandle(m_hMapFile);
            m_hMapFile = nullptr;
        }
        if (m_hEvtAvailable) {
            CloseHandle(m_hEvtAvailable);
            m_hEvtAvailable = nullptr;
        }
        if (m_hEvtConsumed) {
            CloseHandle(m_hEvtConsumed);
            m_hEvtConsumed = nullptr;
        }
    }

private:
    HANDLE m_hMapFile = nullptr;
    void*  m_pBuffer = nullptr;
    HANDLE m_hEvtAvailable = nullptr;
    HANDLE m_hEvtConsumed = nullptr;
};

class SharedMemoryConsumer {
public:
    SharedMemoryConsumer() = default;
    ~SharedMemoryConsumer() { Close(); }

    bool Open() {
        if (m_pBuffer) return true; // Already opened

        m_hMapFile = OpenFileMappingW(FILE_MAP_READ, FALSE, SHM_MAPPING_NAME);
        if (!m_hMapFile) return false;

        m_pBuffer = MapViewOfFile(m_hMapFile, FILE_MAP_READ, 0, 0, 0);
        if (!m_pBuffer) {
            Close();
            return false;
        }

        m_hEvtAvailable = OpenEventW(SYNCHRONIZE, FALSE, EVT_FRAME_AVAILABLE);
        m_hEvtConsumed = OpenEventW(EVENT_MODIFY_STATE, FALSE, EVT_FRAME_CONSUMED);
        return true;
    }

    bool IsActive() {
        if (!m_pBuffer && !Open()) return false;
        const auto* header = reinterpret_cast<const SharedFrameHeader*>(m_pBuffer);
        if (header->magic != SHM_MAGIC || header->version != SHM_VERSION) return false;

        // Check heartbeat freshness
        static uint32_t lastHeartbeat = 0;
        static ULONGLONG lastChangeTick = 0;
        ULONGLONG now = GetTickCount64();

        if (header->heartbeat != lastHeartbeat) {
            lastHeartbeat = header->heartbeat;
            lastChangeTick = now;
        }

        // Active if marked active AND heartbeat changed within last 1500ms
        bool alive = (header->isCoreActive != 0) && (now - lastChangeTick < 1500);
        return alive;
    }

    // Attempts to read latest frame into user buffer. Returns true if valid frame copied.
    bool ReadLatestFrame(uint8_t* pOutData, size_t outCapacity,
                         uint32_t* pWidth, uint32_t* pHeight,
                         uint32_t* pStrideY, uint32_t* pStrideUV,
                         int64_t* pPts100ns) {
        if (!m_pBuffer && !Open()) return false;

        const auto* header = reinterpret_cast<const SharedFrameHeader*>(m_pBuffer);
        if (header->magic != SHM_MAGIC || header->version != SHM_VERSION) return false;
        if (!header->isCoreActive) return false;
        if (header->dataSize == 0 || header->dataSize > outCapacity) return false;

        const uint8_t* pSrc = static_cast<const uint8_t*>(m_pBuffer) + header->dataOffset;
        memcpy(pOutData, pSrc, header->dataSize);

        if (pWidth) *pWidth = header->width;
        if (pHeight) *pHeight = header->height;
        if (pStrideY) *pStrideY = header->strideY;
        if (pStrideUV) *pStrideUV = header->strideUV;
        if (pPts100ns) *pPts100ns = header->ptsNanoSeconds;

        if (m_hEvtConsumed) {
            SetEvent(m_hEvtConsumed);
        }
        return true;
    }

    HANDLE GetFrameAvailableEvent() const { return m_hEvtAvailable; }

    void Close() {
        if (m_pBuffer) {
            UnmapViewOfFile(m_pBuffer);
            m_pBuffer = nullptr;
        }
        if (m_hMapFile) {
            CloseHandle(m_hMapFile);
            m_hMapFile = nullptr;
        }
        if (m_hEvtAvailable) {
            CloseHandle(m_hEvtAvailable);
            m_hEvtAvailable = nullptr;
        }
        if (m_hEvtConsumed) {
            CloseHandle(m_hEvtConsumed);
            m_hEvtConsumed = nullptr;
        }
    }

private:
    HANDLE m_hMapFile = nullptr;
    const void* m_pBuffer = nullptr;
    HANDLE m_hEvtAvailable = nullptr;
    HANDLE m_hEvtConsumed = nullptr;
};

} // namespace ReelCam
