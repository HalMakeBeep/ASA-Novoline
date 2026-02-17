#include "Injector/InjectorASA.h"
#include <windows.h>
#include <winhttp.h>
#include <wincrypt.h>
#include <dwmapi.h>
#include <intrin.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <ctime>
#include <cmath>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdi32.lib")

#define ID_KEY_ENTRY 1001
#define ID_ACTIVATE_BTN 1002
#define ID_STATUS_LABEL 1003
#define ID_ANIMATION_TIMER 2001
#define ID_HOVER_TIMER 2002

// Firebase Config
#include "firebase_config.h"

// Theme colors - Purple/Blue-Green dark mode
#define COLOR_BG_DARK       RGB(18, 18, 24)      // Very dark purple-tinted background
#define COLOR_BG_CARD       RGB(28, 28, 38)      // Card/panel background
#define COLOR_BG_INPUT      RGB(38, 38, 52)      // Input field background
#define COLOR_ACCENT        RGB(138, 99, 210)    // Purple accent
#define COLOR_ACCENT_HOVER  RGB(158, 119, 235)   // Purple hover (lighter)
#define COLOR_ACCENT2       RGB(64, 190, 180)    // Teal/cyan accent
#define COLOR_TEXT          RGB(235, 235, 245)   // Light text
#define COLOR_TEXT_DIM      RGB(150, 150, 175)   // Dimmed text
#define COLOR_SUCCESS       RGB(80, 200, 120)    // Green for success
#define COLOR_ERROR         RGB(255, 90, 90)     // Red for errors
#define COLOR_BORDER        RGB(65, 65, 90)      // Subtle border

// Global variables
HWND g_hMainWindow = NULL;
HWND g_hLoginWindow = NULL;
HWND g_hKeyEntry = NULL;
HWND g_hStatusLabel = NULL;
HWND g_hActivateBtn = NULL;
HINSTANCE g_hInstance = NULL;
HFONT g_hTitleFont = NULL;
HFONT g_hNormalFont = NULL;
HFONT g_hButtonFont = NULL;
HFONT g_hSmallFont = NULL;
HFONT g_hInputFont = NULL;
HFONT g_hLabelFont = NULL;
HWND g_hMainStatusLabel = NULL;
HBRUSH g_hBgBrush = NULL;
HBRUSH g_hCardBrush = NULL;
HBRUSH g_hInputBrush = NULL;
int g_daysRemaining = 0; // Store subscription days remaining
int g_hoursRemaining = 0; // Store hours when less than 1 day

// Animation state
float g_animationPhase = 0.0f;
bool g_buttonHovered = false;
float g_buttonHoverAnim = 0.0f; // 0.0 = normal, 1.0 = fully hovered
float g_glowPhase = 0.0f; // For subtle glow animation

struct LicenseData {
    std::string activationKey;
    std::string finalKey;
    std::string diskID;
    std::string systemUUID;
    std::string cpuID;
    std::string pcName;
    bool lifetime;
    time_t subscriptionEnd;
    bool isValid;
};

// Forward declarations
LRESULT CALLBACK LoginWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void ShowMainWindow();
void ShowPasswordPrompt();
void EnableDarkTitleBar(HWND hwnd);
std::string GetDiskSerial();
std::string GetSystemUUID();
std::string GetCPUID();
std::string GetPCName();
std::string ComputeFinalKey(const std::string& diskID, const std::string& moboID, const std::string& cpuID);
LicenseData CheckExistingLicense();
std::string HttpsRequest(const std::wstring& host, const std::wstring& path, const std::string& method, const std::string& data = "");
bool QueryFirestoreByFinalKey(const std::string& finalKey, LicenseData& outData);
bool RedeemActivationKey(const std::string& key, const std::string& diskID,
                         const std::string& moboID, const std::string& pcName);
void FinishStartup();
std::string ExtractJsonStringValue(const std::string& json, const std::string& field);

// HTTP Request function
std::string HttpsRequest(const std::wstring& host, const std::wstring& path, const std::string& method, const std::string& data) {
    std::string result;

    HINTERNET hSession = WinHttpOpen(L"Prisme/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(),
                                       INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return "";
    }

    std::wstring wmethod(method.begin(), method.end());
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, wmethod.c_str(), path.c_str(),
                                           NULL, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    if (method != "GET") {
        WinHttpAddRequestHeaders(hRequest,
                                L"Content-Type: application/json",
                                -1, WINHTTP_ADDREQ_FLAG_ADD);
    }

    BOOL bResults = FALSE;
    if (data.empty()) {
        bResults = WinHttpSendRequest(hRequest,
                                      WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                      WINHTTP_NO_REQUEST_DATA, 0,
                                      0, 0);
    } else {
        bResults = WinHttpSendRequest(hRequest,
                                      WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                      (LPVOID)data.c_str(), data.length(),
                                      data.length(), 0);
    }

    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, NULL);
    }

    if (bResults) {
        DWORD dwSize = 0;
        DWORD dwDownloaded = 0;
        do {
            dwSize = 0;
            if (WinHttpQueryDataAvailable(hRequest, &dwSize)) {
                char* pszOutBuffer = new char[dwSize + 1];
                ZeroMemory(pszOutBuffer, dwSize + 1);

                if (WinHttpReadData(hRequest, pszOutBuffer, dwSize, &dwDownloaded)) {
                    result.append(pszOutBuffer, dwDownloaded);
                }
                delete[] pszOutBuffer;
            }
        } while (dwSize > 0);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return result;
}

// HTTP Request function (non-HTTPS for bot webhook)
std::string HttpRequest(const std::wstring& host, int port, const std::wstring& path, const std::string& method, const std::string& data) {
    std::string result;

    HINTERNET hSession = WinHttpOpen(L"Prisme/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return "";
    }

    std::wstring wmethod(method.begin(), method.end());
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, wmethod.c_str(), path.c_str(),
                                           NULL, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           0);  // No WINHTTP_FLAG_SECURE for HTTP
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    if (method != "GET") {
        WinHttpAddRequestHeaders(hRequest,
                                L"Content-Type: application/json",
                                -1, WINHTTP_ADDREQ_FLAG_ADD);
    }

    BOOL bResults = FALSE;
    if (data.empty()) {
        bResults = WinHttpSendRequest(hRequest,
                                     WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                     WINHTTP_NO_REQUEST_DATA, 0,
                                     0, 0);
    } else {
        bResults = WinHttpSendRequest(hRequest,
                                     WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                     (LPVOID)data.c_str(), data.length(),
                                     data.length(), 0);
    }

    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, NULL);
    }

    if (bResults) {
        DWORD dwSize = 0;
        DWORD dwDownloaded = 0;
        do {
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;

            if (dwSize == 0) break;

            char* pszOutBuffer = new char[dwSize + 1];
            ZeroMemory(pszOutBuffer, dwSize + 1);

            if (!WinHttpReadData(hRequest, (LPVOID)pszOutBuffer, dwSize, &dwDownloaded)) {
                delete[] pszOutBuffer;
                break;
            }

            result.append(pszOutBuffer, dwDownloaded);
            delete[] pszOutBuffer;

        } while (dwSize > 0);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return result;
}

// Extract JSON string value (simple parser)
// Enable dark mode title bar
void EnableDarkTitleBar(HWND hwnd) {
    BOOL value = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &value, sizeof(value)); // DWMWA_USE_IMMERSIVE_DARK_MODE = 20
}

std::string ExtractJsonStringValue(const std::string& json, const std::string& field) {
    std::string search = "\"" + field + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";

    pos = json.find("\"stringValue\":", pos);
    if (pos == std::string::npos) return "";

    pos = json.find("\"", pos + 15);
    if (pos == std::string::npos) return "";
    pos++;

    size_t endPos = json.find("\"", pos);
    if (endPos == std::string::npos) return "";

    return json.substr(pos, endPos - pos);
}

// Query WMI using PowerShell with hidden console window
std::string QueryWMIPowerShell(const std::string& wmiClass, const std::string& property) {
    std::string cmd = "powershell -WindowStyle Hidden -NoProfile -Command \"Get-WmiObject -Class " + wmiClass +
                      " | Select-Object -ExpandProperty " + property + " -First 1\" 2>nul";

    // Create process with hidden window
    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    // Create temp file for output
    char tempPath[MAX_PATH];
    char tempFile[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);
    GetTempFileNameA(tempPath, "wmi", 0, tempFile);

    std::string fullCmd = "cmd.exe /c " + cmd + " > \"" + tempFile + "\"";

    if (CreateProcessA(NULL, (LPSTR)fullCmd.c_str(), NULL, NULL, FALSE,
                      CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 5000); // Wait max 5 seconds
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    // Read result from temp file
    std::string result;
    FILE* file = fopen(tempFile, "r");
    if (file) {
        char buffer[512];
        while (fgets(buffer, sizeof(buffer), file)) {
            std::string line(buffer);
            line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());
            if (!line.empty()) {
                result = line;
                break;
            }
        }
        fclose(file);
    }

    DeleteFileA(tempFile);
    return result;
}

// Get disk serial number using Windows API (most reliable, works on all Windows versions)
std::string GetDiskSerial() {
    std::string result;

    // Method 1: GetVolumeInformationA for C: drive (most reliable, works XP+)
    char volumeName[MAX_PATH + 1] = {0};
    char fileSystemName[MAX_PATH + 1] = {0};
    DWORD serialNumber = 0;
    DWORD maxComponentLen = 0;
    DWORD fileSystemFlags = 0;

    if (GetVolumeInformationA(
        "C:\\",
        volumeName,
        sizeof(volumeName),
        &serialNumber,
        &maxComponentLen,
        &fileSystemFlags,
        fileSystemName,
        sizeof(fileSystemName))) {

        // Convert to hex string
        std::stringstream ss;
        ss << std::hex << std::uppercase << serialNumber;
        result = ss.str();

        if (!result.empty() && result != "0") {
            return result;
        }
    }

    // Fallback 1: Win32_DiskDrive via PowerShell
    result = QueryWMIPowerShell("Win32_DiskDrive", "SerialNumber");
    if (!result.empty()) {
        std::transform(result.begin(), result.end(), result.begin(), ::toupper);
        return result;
    }

    // Fallback 2: wmic command line (with hidden window)
    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    char tempPath[MAX_PATH];
    char tempFile[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);
    GetTempFileNameA(tempPath, "dsk", 0, tempFile);

    std::string cmd = "cmd.exe /c wmic diskdrive get SerialNumber > \"" + std::string(tempFile) + "\" 2>nul";

    if (CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, FALSE,
                      CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        FILE* file = fopen(tempFile, "r");
        if (file) {
            char buffer[512];
            int lineCount = 0;
            while (fgets(buffer, sizeof(buffer), file)) {
                lineCount++;
                if (lineCount == 1) continue;

                std::string line(buffer);
                line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());

                if (!line.empty() && line.length() > 2) {
                    result = line;
                    break;
                }
            }
            fclose(file);
        }
    }
    DeleteFileA(tempFile);

    if (result.empty()) {
        result = "UNKNOWN_DISK_ID";
    }

    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

// Get System UUID (BEST and MOST STABLE motherboard identifier)
// This is a unique ID burned into the motherboard and NEVER changes
// - Survives BIOS updates ✓
// - Survives Windows reinstalls ✓
// - Only changes if motherboard is physically replaced
std::string GetSystemUUID() {
    std::string result;

    // Method 1: Win32_ComputerSystemProduct UUID (most reliable)
    result = QueryWMIPowerShell("Win32_ComputerSystemProduct", "UUID");
    if (!result.empty() && result.length() > 10 &&
        result != "FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF") {
        std::transform(result.begin(), result.end(), result.begin(), ::toupper);
        return result;
    }

    // Method 2: wmic command (fallback with hidden window)
    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    char tempPath[MAX_PATH];
    char tempFile[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);
    GetTempFileNameA(tempPath, "uid", 0, tempFile);

    std::string cmd = "cmd.exe /c wmic csproduct get UUID > \"" + std::string(tempFile) + "\" 2>nul";

    if (CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, FALSE,
                      CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        FILE* file = fopen(tempFile, "r");
        if (file) {
            char buffer[512];
            int lineCount = 0;
            while (fgets(buffer, sizeof(buffer), file)) {
                lineCount++;
                if (lineCount == 1) continue;

                std::string line(buffer);
                line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());

                if (!line.empty() && line.length() > 10 &&
                    line != "FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF") {
                    result = line;
                    break;
                }
            }
            fclose(file);
        }
    }
    DeleteFileA(tempFile);

    // Method 3: Registry fallback for very old systems
    if (result.empty()) {
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                         "SOFTWARE\\Microsoft\\Cryptography",
                         0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            char buffer[256];
            DWORD bufferSize = sizeof(buffer);
            // MachineGuid is stable and unique per Windows installation
            if (RegQueryValueExA(hKey, "MachineGuid", NULL, NULL,
                                (LPBYTE)buffer, &bufferSize) == ERROR_SUCCESS) {
                result = std::string(buffer);
            }
            RegCloseKey(hKey);
        }
    }

    if (result.empty()) {
        result = "UNKNOWN_UUID";
    }

    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

// Get CPU ID using CPUID instruction
std::string GetCPUID() {
    std::string result;

    // Use __cpuid intrinsic to get processor info
    int cpuInfo[4] = {0};

    #if defined(_MSC_VER)
        __cpuid(cpuInfo, 1);
    #elif defined(__GNUC__) || defined(__clang__)
        __asm__ __volatile__(
            "cpuid"
            : "=a" (cpuInfo[0]), "=b" (cpuInfo[1]), "=c" (cpuInfo[2]), "=d" (cpuInfo[3])
            : "a" (1), "c" (0)
        );
    #endif

    // Create unique CPU identifier from processor signature (EAX register)
    std::stringstream ss;
    ss << std::hex << std::uppercase << cpuInfo[0];
    result = ss.str();

    // Fallback: Try WMI for processor ID
    if (result.empty() || result == "0") {
        result = QueryWMIPowerShell("Win32_Processor", "ProcessorId");
        if (!result.empty()) {
            std::transform(result.begin(), result.end(), result.begin(), ::toupper);
            return result;
        }
    }

    // Fallback: wmic command (with hidden window)
    if (result.empty() || result == "0") {
        STARTUPINFOA si = {sizeof(si)};
        PROCESS_INFORMATION pi;
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        char tempPath[MAX_PATH];
        char tempFile[MAX_PATH];
        GetTempPathA(MAX_PATH, tempPath);
        GetTempFileNameA(tempPath, "cpu", 0, tempFile);

        std::string cmd = "cmd.exe /c wmic cpu get ProcessorId > \"" + std::string(tempFile) + "\" 2>nul";

        if (CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, FALSE,
                          CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 5000);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);

            FILE* file = fopen(tempFile, "r");
            if (file) {
                char buffer[512];
                int lineCount = 0;
                while (fgets(buffer, sizeof(buffer), file)) {
                    lineCount++;
                    if (lineCount == 1) continue;

                    std::string line(buffer);
                    line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());

                    if (!line.empty() && line.length() > 2) {
                        result = line;
                        break;
                    }
                }
                fclose(file);
            }
        }
        DeleteFileA(tempFile);
    }

    if (result.empty()) {
        result = "UNKNOWN_CPU_ID";
    }

    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

// Get PC name
std::string GetPCName() {
    char buffer[256];
    DWORD size = sizeof(buffer);
    if (GetComputerNameA(buffer, &size)) {
        return std::string(buffer);
    }
    return "UNKNOWN_PC";
}

// Compute final key hash using SHA-256 with ONLY STABLE hardware components
// These identifiers NEVER change unless hardware is physically replaced
std::string ComputeFinalKey(const std::string& diskID, const std::string& moboID, const std::string& cpuID) {
    // Use ONLY stable identifiers that won't change:
    // 1. System UUID - Motherboard unique ID (doesn't change with BIOS updates)
    // 2. CPU ID - Processor identifier (stable)
    // 3. Disk Serial - C: drive serial (stable unless drive is replaced)

    std::string systemUUID = GetSystemUUID();  // Best motherboard identifier

    // Combine 3 STABLE components (ignoring weak moboID parameter for now)
    // This ensures customers won't have issues with BIOS updates, network changes, etc.
    std::string combined = diskID + "-" + systemUUID + "-" + cpuID;

    // Compute SHA-256 hash
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BYTE hash[32]; // SHA-256 produces 32 bytes
    DWORD hashLen = 32;

    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        return "";
    }

    if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        return "";
    }

    if (!CryptHashData(hHash, (const BYTE*)combined.c_str(), combined.length(), 0)) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return "";
    }

    if (!CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0)) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return "";
    }

    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);

    // Convert to hex string (lowercase like Python's hexdigest())
    std::stringstream ss;
    for (int i = 0; i < 32; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }

    return ss.str();
}

// Query Firestore for existing license by FinalKey
bool QueryFirestoreByFinalKey(const std::string& finalKey, LicenseData& outData) {
    // Query PrismeAppAcces collection
    std::wstring path = L"/v1/projects/" + std::wstring(FIREBASE_PROJECT_ID.begin(), FIREBASE_PROJECT_ID.end()) +
                        L"/databases/(default)/documents/" + std::wstring(COLLECTION_PRISME_ACCESS.begin(), COLLECTION_PRISME_ACCESS.end()) +
                        L"?key=" + std::wstring(FIRESTORE_API_KEY.begin(), FIRESTORE_API_KEY.end());

    std::string response = HttpsRequest(L"firestore.googleapis.com", path, "GET");

    if (response.empty()) {
        return false;
    }

    // Parse response to find matching FinalKey
    // In production: use proper JSON parser
    // For now: simple string search
    if (response.find(finalKey) != std::string::npos) {
        outData.finalKey = finalKey;
        outData.activationKey = ExtractJsonStringValue(response, "ActivationKeyUsed");
        outData.diskID = ExtractJsonStringValue(response, "DiskID");
        outData.systemUUID = ExtractJsonStringValue(response, "SystemUUID");
        outData.cpuID = ExtractJsonStringValue(response, "CPUID");
        outData.pcName = ExtractJsonStringValue(response, "PCName");

        std::string lifetimeStr = ExtractJsonStringValue(response, "Lifetime");
        outData.lifetime = (lifetimeStr == "true");

        outData.isValid = true;
        return true;
    }

    return false;
}

// Registry functions removed - using pure hardware verification now

// Verify license with server
bool VerifyLicenseWithServer(const std::string& finalKey) {
    // Build JSON payload
    std::ostringstream jsonBody;
    jsonBody << "{\"finalKey\":\"" << finalKey << "\"}";

    // Send POST request to Discord bot
    std::string response = HttpRequest(L"prem-eu2.bot-hosting.net", 20734, L"/verify", "POST", jsonBody.str());

    if (response.empty()) {
        return false;  // No connection - deny access
    }

    // Check if valid
    bool isValid = (response.find("\"valid\":true") != std::string::npos ||
                    response.find("\"valid\": true") != std::string::npos);

    // Parse daysRemaining and hoursRemaining from response
    if (isValid) {
        // Parse days
        size_t pos = response.find("\"daysRemaining\"");
        if (pos != std::string::npos) {
            size_t colonPos = response.find(":", pos);
            if (colonPos != std::string::npos) {
                size_t numStart = response.find_first_of("-0123456789", colonPos);
                if (numStart != std::string::npos) {
                    size_t numEnd = response.find_first_not_of("0123456789", numStart + 1);
                    std::string daysStr = response.substr(numStart, numEnd - numStart);
                    g_daysRemaining = std::stoi(daysStr);
                }
            }
        }

        // Parse hours
        pos = response.find("\"hoursRemaining\"");
        if (pos != std::string::npos) {
            size_t colonPos = response.find(":", pos);
            if (colonPos != std::string::npos) {
                size_t numStart = response.find_first_of("0123456789", colonPos);
                if (numStart != std::string::npos) {
                    size_t numEnd = response.find_first_not_of("0123456789", numStart);
                    std::string hoursStr = response.substr(numStart, numEnd - numStart);
                    g_hoursRemaining = std::stoi(hoursStr);
                }
            }
        }
    }

    return isValid;
}

// Check existing license on startup - NO REGISTRY, pure hardware check
LicenseData CheckExistingLicense() {
    LicenseData license = {};
    license.isValid = false;

    // Compute FinalKey directly from hardware - no registry needed
    std::string diskID = GetDiskSerial();
    std::string systemUUID = GetSystemUUID();
    std::string cpuID = GetCPUID();
    std::string currentFinalKey = ComputeFinalKey(diskID, systemUUID, cpuID);

    // Verify with server that this hardware has a valid license
    if (!VerifyLicenseWithServer(currentFinalKey)) {
        // License not found, expired, or invalid
        return license;
    }

    // All checks passed
    license.finalKey = currentFinalKey;
    license.isValid = true;

    return license;
}

// Send activation request to Discord bot
bool SendActivationToBot(const std::string& key, const std::string& finalKey,
                         const std::string& diskID, const std::string& systemUUID,
                         const std::string& cpuID, const std::string& pcName) {
    // Build JSON payload
    std::ostringstream jsonBody;
    jsonBody << "{"
             << "\"key\":\"" << key << "\","
             << "\"finalKey\":\"" << finalKey << "\","
             << "\"diskID\":\"" << diskID << "\","
             << "\"systemUUID\":\"" << systemUUID << "\","
             << "\"cpuID\":\"" << cpuID << "\","
             << "\"pcName\":\"" << pcName << "\""
             << "}";

    // Send POST request to Discord bot (HTTP, not HTTPS)
    std::string response = HttpRequest(L"prem-eu2.bot-hosting.net", 20734, L"/activate", "POST", jsonBody.str());

    // Check if successful
    return (response.find("\"success\":true") != std::string::npos ||
            response.find("\"success\": true") != std::string::npos);
}

// Redeem activation key (now just sends to bot and waits)
bool RedeemActivationKey(const std::string& key, const std::string& diskID,
                         const std::string& systemUUID, const std::string& cpuID,
                         const std::string& pcName) {
    // Compute FinalKey
    std::string finalKey = ComputeFinalKey(diskID, systemUUID, cpuID);

    // Send activation request to Discord bot
    // The bot will validate the key and create the license
    // If it returns success, we trust it
    return SendActivationToBot(key, finalKey, diskID, systemUUID, cpuID, pcName);
}

// Draw rounded rectangle helper
void DrawRoundedRect(HDC hdc, int x, int y, int width, int height, int radius, COLORREF fillColor, COLORREF borderColor) {
    HBRUSH hBrush = CreateSolidBrush(fillColor);
    HPEN hPen = CreatePen(PS_SOLID, 1, borderColor);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

    RoundRect(hdc, x, y, x + width, y + height, radius, radius);

    SelectObject(hdc, hOldBrush);
    SelectObject(hdc, hOldPen);
    DeleteObject(hBrush);
    DeleteObject(hPen);
}

// Draw gradient line (accent decoration)
void DrawAccentLine(HDC hdc, int x, int y, int width) {
    for (int i = 0; i < width; i++) {
        // Gradient from purple to teal
        int r = 138 - (74 * i / width);  // 138 -> 64
        int g = 99 + (91 * i / width);   // 99 -> 190
        int b = 210 - (30 * i / width);  // 210 -> 180
        SetPixel(hdc, x + i, y, RGB(r, g, b));
        SetPixel(hdc, x + i, y + 1, RGB(r, g, b));
    }
}

// Subclass procedure for vertically centered edit control
WNDPROC g_OriginalEditProc = NULL;

LRESULT CALLBACK CenteredEditProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_PAINT) {
        // Call original to handle background
        CallWindowProc(g_OriginalEditProc, hwnd, uMsg, wParam, lParam);
        return 0;
    }
    return CallWindowProc(g_OriginalEditProc, hwnd, uMsg, wParam, lParam);
}

// Login window procedure
LRESULT CALLBACK LoginWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            // Start animation timer for glow effect
            SetTimer(hwnd, ID_ANIMATION_TIMER, 50, NULL); // 20 FPS

            // Title - "Prisme" with accent styling
            HWND hTitle = CreateWindow("STATIC", "Prisme",
                        WS_VISIBLE | WS_CHILD | SS_CENTER,
                        0, 25, 520, 70,
                        hwnd, NULL, g_hInstance, NULL);
            SendMessage(hTitle, WM_SETFONT, (WPARAM)g_hTitleFont, TRUE);

            // Subtitle - larger
            HWND hSubtitle = CreateWindow("STATIC", "License Activation",
                        WS_VISIBLE | WS_CHILD | SS_CENTER,
                        0, 100, 520, 30,
                        hwnd, NULL, g_hInstance, NULL);
            SendMessage(hSubtitle, WM_SETFONT, (WPARAM)g_hSmallFont, TRUE);

            // Welcome message - larger
            HWND hWelcome = CreateWindow("STATIC",
                        "Enter your activation key to get started",
                        WS_VISIBLE | WS_CHILD | SS_CENTER,
                        40, 165, 440, 35,
                        hwnd, NULL, g_hInstance, NULL);
            SendMessage(hWelcome, WM_SETFONT, (WPARAM)g_hNormalFont, TRUE);

            // Input label - uppercase tracking
            HWND hLabel = CreateWindow("STATIC", "ACTIVATION KEY",
                        WS_VISIBLE | WS_CHILD | SS_CENTER,
                        110, 220, 300, 20,
                        hwnd, NULL, g_hInstance, NULL);
            SendMessage(hLabel, WM_SETFONT, (WPARAM)g_hLabelFont, TRUE);

            // Key input field - taller for vertical centering with larger font
            g_hKeyEntry = CreateWindowEx(0, "EDIT", "",
                        WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | ES_CENTER,
                        110, 248, 300, 55,
                        hwnd, (HMENU)ID_KEY_ENTRY, g_hInstance, NULL);
            SendMessage(g_hKeyEntry, WM_SETFONT, (WPARAM)g_hInputFont, TRUE);
            // Add padding inside edit control
            SendMessage(g_hKeyEntry, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(10, 10));

            // Activate button (pill shape)
            g_hActivateBtn = CreateWindow("BUTTON", "Activate License",
                        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_OWNERDRAW,
                        110, 325, 300, 58,
                        hwnd, (HMENU)ID_ACTIVATE_BTN, g_hInstance, NULL);
            SendMessage(g_hActivateBtn, WM_SETFONT, (WPARAM)g_hButtonFont, TRUE);

            // Status label
            g_hStatusLabel = CreateWindow("STATIC", "",
                        WS_VISIBLE | WS_CHILD | SS_CENTER,
                        40, 400, 440, 65,
                        hwnd, (HMENU)ID_STATUS_LABEL, g_hInstance, NULL);
            SendMessage(g_hStatusLabel, WM_SETFONT, (WPARAM)g_hNormalFont, TRUE);

            SetFocus(g_hKeyEntry);
            return 0;
        }

        case WM_TIMER: {
            if (wParam == ID_ANIMATION_TIMER) {
                // Update glow animation phase
                g_glowPhase += 0.08f;
                if (g_glowPhase > 6.28318f) g_glowPhase -= 6.28318f;

                // Update button hover animation
                if (g_buttonHovered && g_buttonHoverAnim < 1.0f) {
                    g_buttonHoverAnim += 0.15f;
                    if (g_buttonHoverAnim > 1.0f) g_buttonHoverAnim = 1.0f;
                } else if (!g_buttonHovered && g_buttonHoverAnim > 0.0f) {
                    g_buttonHoverAnim -= 0.15f;
                    if (g_buttonHoverAnim < 0.0f) g_buttonHoverAnim = 0.0f;
                }

                // Only redraw button area if hover animation is active
                if (g_hActivateBtn && (g_buttonHoverAnim > 0.0f || g_buttonHovered)) {
                    InvalidateRect(g_hActivateBtn, NULL, FALSE);
                }

                // Only redraw the accent line area, not the whole window
                RECT lineRect = {135, 148, 385, 155};
                InvalidateRect(hwnd, &lineRect, FALSE);
            }
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            // Fill background
            RECT rect;
            GetClientRect(hwnd, &rect);
            HBRUSH hBgBrush = CreateSolidBrush(COLOR_BG_DARK);
            FillRect(hdc, &rect, hBgBrush);
            DeleteObject(hBgBrush);

            // Draw animated accent line under title with glow
            int lineX = 135;
            int lineY = 150;
            int lineWidth = 250;

            // Calculate animated glow position
            float glowPos = (sin(g_glowPhase) + 1.0f) * 0.5f; // 0.0 to 1.0
            int glowCenter = lineX + (int)(glowPos * lineWidth);

            for (int i = 0; i < lineWidth; i++) {
                // Base gradient from purple to teal
                float t = (float)i / lineWidth;
                int baseR = 138 - (int)(74 * t);
                int baseG = 99 + (int)(91 * t);
                int baseB = 210 - (int)(30 * t);

                // Add glow effect near the animated position
                int dist = abs((lineX + i) - glowCenter);
                float glowIntensity = 1.0f - (float)dist / 60.0f;
                if (glowIntensity < 0.0f) glowIntensity = 0.0f;
                glowIntensity *= glowIntensity; // Make it sharper

                int r = baseR + (int)((255 - baseR) * glowIntensity * 0.5f);
                int g = baseG + (int)((255 - baseG) * glowIntensity * 0.5f);
                int b = baseB + (int)((255 - baseB) * glowIntensity * 0.5f);

                if (r > 255) r = 255;
                if (g > 255) g = 255;
                if (b > 255) b = 255;

                // Draw thicker line (3 pixels)
                SetPixel(hdc, lineX + i, lineY, RGB(r, g, b));
                SetPixel(hdc, lineX + i, lineY + 1, RGB(r, g, b));
                SetPixel(hdc, lineX + i, lineY + 2, RGB(r, g, b));
            }

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetBkMode(hdcStatic, TRANSPARENT);

            HWND hControl = (HWND)lParam;
            if (hControl == g_hStatusLabel) {
                // Check if status contains error
                char text[256];
                GetWindowText(g_hStatusLabel, text, 256);
                if (strstr(text, "success") || strstr(text, "Success")) {
                    SetTextColor(hdcStatic, COLOR_SUCCESS);
                } else if (strlen(text) > 0) {
                    SetTextColor(hdcStatic, COLOR_ERROR);
                } else {
                    SetTextColor(hdcStatic, COLOR_TEXT);
                }
            } else {
                // Check if it's a label (smaller text)
                char className[32];
                GetClassName(hControl, className, 32);
                RECT rect;
                GetWindowRect(hControl, &rect);
                int height = rect.bottom - rect.top;

                if (height <= 22) {
                    SetTextColor(hdcStatic, COLOR_TEXT_DIM); // Dim for labels
                } else {
                    SetTextColor(hdcStatic, COLOR_TEXT); // Normal text
                }
            }

            if (!g_hBgBrush) g_hBgBrush = CreateSolidBrush(COLOR_BG_DARK);
            return (LRESULT)g_hBgBrush;
        }

        case WM_CTLCOLOREDIT: {
            HDC hdcEdit = (HDC)wParam;
            SetTextColor(hdcEdit, COLOR_TEXT);
            SetBkColor(hdcEdit, COLOR_BG_INPUT);
            if (!g_hInputBrush) g_hInputBrush = CreateSolidBrush(COLOR_BG_INPUT);
            return (LRESULT)g_hInputBrush;
        }

        case WM_SETCURSOR: {
            // Track mouse over button for hover effect
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);

            RECT btnRect;
            if (g_hActivateBtn) {
                GetWindowRect(g_hActivateBtn, &btnRect);
                POINT btnTopLeft = {btnRect.left, btnRect.top};
                ScreenToClient(hwnd, &btnTopLeft);
                btnRect.right = btnTopLeft.x + (btnRect.right - btnRect.left);
                btnRect.bottom = btnTopLeft.y + (btnRect.bottom - btnRect.top);
                btnRect.left = btnTopLeft.x;
                btnRect.top = btnTopLeft.y;

                bool wasHovered = g_buttonHovered;
                g_buttonHovered = PtInRect(&btnRect, pt) != 0;

                if (wasHovered != g_buttonHovered) {
                    InvalidateRect(g_hActivateBtn, NULL, FALSE);
                }
            }
            break; // Let default handler set cursor
        }

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT lpDIS = (LPDRAWITEMSTRUCT)lParam;
            if (lpDIS->CtlID == ID_ACTIVATE_BTN) {
                HDC hdc = lpDIS->hDC;
                RECT rect = lpDIS->rcItem;

                // First fill entire button area with dark background
                HBRUSH hBgBrush = CreateSolidBrush(COLOR_BG_DARK);
                FillRect(hdc, &rect, hBgBrush);
                DeleteObject(hBgBrush);

                // Interpolate between normal and hover colors based on animation
                int normalR = 138, normalG = 99, normalB = 210;   // COLOR_ACCENT
                int hoverR = 168, hoverG = 129, hoverB = 245;     // Lighter purple

                int r = normalR + (int)((hoverR - normalR) * g_buttonHoverAnim);
                int g = normalG + (int)((hoverG - normalG) * g_buttonHoverAnim);
                int b = normalB + (int)((hoverB - normalB) * g_buttonHoverAnim);

                COLORREF bgColor = RGB(r, g, b);
                COLORREF textColor = RGB(255, 255, 255);

                if (lpDIS->itemState & ODS_SELECTED) {
                    // Pressed state - darker
                    bgColor = RGB(118, 79, 190);
                }

                // Draw subtle glow behind button when hovered
                if (g_buttonHoverAnim > 0.0f) {
                    int glowAlpha = (int)(30 * g_buttonHoverAnim);
                    HBRUSH hGlowBrush = CreateSolidBrush(RGB(138 + glowAlpha, 99 + glowAlpha, 210 + glowAlpha/2));
                    HPEN hGlowPen = CreatePen(PS_SOLID, 1, RGB(138 + glowAlpha, 99 + glowAlpha, 210 + glowAlpha/2));
                    SelectObject(hdc, hGlowBrush);
                    SelectObject(hdc, hGlowPen);
                    // Slightly larger rounded rect for glow
                    RoundRect(hdc, rect.left - 2, rect.top - 2, rect.right + 2, rect.bottom + 2, 54, 54);
                    DeleteObject(hGlowBrush);
                    DeleteObject(hGlowPen);
                }

                // Draw rounded button (pill shape)
                HBRUSH hBrush = CreateSolidBrush(bgColor);
                HPEN hPen = CreatePen(PS_SOLID, 1, bgColor);
                SelectObject(hdc, hBrush);
                SelectObject(hdc, hPen);
                RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, 50, 50);
                DeleteObject(hBrush);
                DeleteObject(hPen);

                // Draw text
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, textColor);
                SelectObject(hdc, g_hButtonFont);
                DrawText(hdc, "Activate License", -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                return TRUE;
            }
            break;
        }

        case WM_COMMAND: {
            if (LOWORD(wParam) == ID_ACTIVATE_BTN) {
                char keyBuffer[256];
                GetWindowText(g_hKeyEntry, keyBuffer, 256);
                std::string key(keyBuffer);

                if (key.empty()) {
                    SetWindowText(g_hStatusLabel, "Please enter a valid activation key.");
                    return 0;
                }

                SetWindowText(g_hStatusLabel, "Validating key with database...");

                std::string diskID = GetDiskSerial();
                std::string systemUUID = GetSystemUUID();
                std::string cpuID = GetCPUID();
                std::string pcName = GetPCName();

                if (RedeemActivationKey(key, diskID, systemUUID, cpuID, pcName)) {
                    // No registry needed - hardware fingerprint is checked directly with server
                    SetWindowText(g_hStatusLabel, "Key redeemed successfully!");
                    ShowWindow(hwnd, SW_HIDE);
                    ShowMainWindow();
                } else {
                    SetWindowText(g_hStatusLabel,
                        "The activation key you entered does not exist\nor has already been used.\nPlease contact an admin.");
                }
            }
            return 0;
        }

        case WM_DESTROY:
            KillTimer(hwnd, ID_ANIMATION_TIMER);
            PostQuitMessage(0);
            return 0;

        default:
            break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Main window procedure
LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_COMMAND: {
            if (LOWORD(wParam) == 1010) {
                if (Injector::InjectToASA()) {
                    SetWindowText(g_hMainStatusLabel, "F1 to open Prisme menu");
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            }
            return 0;
        }
        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetBkMode(hdcStatic, TRANSPARENT);
            HWND hControl = (HWND)lParam;
            if (hControl == g_hMainStatusLabel) {
                SetTextColor(hdcStatic, COLOR_ACCENT2); // Teal for status
            } else {
                SetTextColor(hdcStatic, COLOR_TEXT);
            }
            if (!g_hBgBrush) g_hBgBrush = CreateSolidBrush(COLOR_BG_DARK);
            return (LRESULT)g_hBgBrush;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT rect;
            GetClientRect(hwnd, &rect);

            // Fill dark background
            HBRUSH hBgBrush = CreateSolidBrush(COLOR_BG_DARK);
            FillRect(hdc, &rect, hBgBrush);
            DeleteObject(hBgBrush);

            SetBkMode(hdc, TRANSPARENT);

            // Draw accent line at top
            DrawAccentLine(hdc, 0, 0, rect.right);
            DrawAccentLine(hdc, 0, 2, rect.right);

            // Title "Prisme" with accent color
            SelectObject(hdc, g_hTitleFont);
            SetTextColor(hdc, COLOR_ACCENT);
            RECT titleRect = rect;
            titleRect.top = 50;
            DrawText(hdc, "Prisme", -1, &titleRect, DT_CENTER | DT_TOP | DT_SINGLELINE);

            // Status text
            SelectObject(hdc, g_hNormalFont);
            SetTextColor(hdc, COLOR_ACCENT2);
            RECT statusRect = rect;
            statusRect.top = 110;
            DrawText(hdc, "License Active", -1, &statusRect, DT_CENTER | DT_TOP | DT_SINGLELINE);

            // Draw card for subscription info (bigger)
            int cardX = (rect.right - 400) / 2;
            int cardY = 170;
            DrawRoundedRect(hdc, cardX, cardY, 400, 130, 16, COLOR_BG_CARD, COLOR_BORDER);

            // Subscription label
            SelectObject(hdc, g_hSmallFont);
            SetTextColor(hdc, COLOR_TEXT_DIM);
            RECT labelRect = {cardX + 30, cardY + 25, cardX + 380, cardY + 50};
            DrawText(hdc, "SUBSCRIPTION STATUS", -1, &labelRect, DT_LEFT | DT_TOP | DT_SINGLELINE);

            // Subscription time with color coding
            SelectObject(hdc, g_hButtonFont);
            std::ostringstream oss;

            if (g_daysRemaining == 0) {
                // Less than 1 day - show hours in RED
                SetTextColor(hdc, COLOR_ERROR);
                if (g_hoursRemaining <= 1) {
                    oss << g_hoursRemaining << " hour remaining";
                } else {
                    oss << g_hoursRemaining << " hours remaining";
                }
            } else if (g_daysRemaining < 5) {
                // Less than 5 days - RED
                SetTextColor(hdc, COLOR_ERROR);
                oss << g_daysRemaining << " day" << (g_daysRemaining != 1 ? "s" : "") << " remaining";
            } else if (g_daysRemaining < 10) {
                // Less than 10 days - ORANGE/WARNING
                SetTextColor(hdc, RGB(255, 180, 50));
                oss << g_daysRemaining << " days remaining";
            } else {
                // More than 10 days - GREEN/SUCCESS
                SetTextColor(hdc, COLOR_SUCCESS);
                oss << g_daysRemaining << " days remaining";
            }

            std::string subText = oss.str();
            RECT timeRect = {cardX + 30, cardY + 60, cardX + 380, cardY + 100};
            DrawText(hdc, subText.c_str(), -1, &timeRect, DT_LEFT | DT_TOP | DT_SINGLELINE);

            // Draw status indicator dot (bigger)
            COLORREF dotColor = (g_daysRemaining >= 10) ? COLOR_SUCCESS :
                               (g_daysRemaining >= 5) ? RGB(255, 180, 50) : COLOR_ERROR;
            HBRUSH hDotBrush = CreateSolidBrush(dotColor);
            HPEN hDotPen = CreatePen(PS_SOLID, 1, dotColor);
            SelectObject(hdc, hDotBrush);
            SelectObject(hdc, hDotPen);
            Ellipse(hdc, cardX + 355, cardY + 62, cardX + 380, cardY + 87);
            DeleteObject(hDotBrush);
            DeleteObject(hDotPen);

            // Footer text
            SelectObject(hdc, g_hSmallFont);
            SetTextColor(hdc, COLOR_TEXT_DIM);
            RECT footerRect = rect;
            footerRect.bottom -= 20;
            DrawText(hdc, "Powered by Prisme Licensing", -1, &footerRect, DT_CENTER | DT_BOTTOM | DT_SINGLELINE);

            EndPaint(hwnd, &ps);
            return 0;
        }
        default:
            break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void ShowMainWindow() {
    const char CLASS_NAME[] = "MainWindowClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc = MainWindowProc;
    wc.hInstance = g_hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(101));
    wc.hbrBackground = CreateSolidBrush(COLOR_BG_DARK);

    RegisterClass(&wc);

    g_hMainWindow = CreateWindowEx(
        0,
        CLASS_NAME,
        "Prisme ASA",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 520, 450,
        NULL, NULL, g_hInstance, NULL
    );

    if (g_hMainWindow != NULL) {
        EnableDarkTitleBar(g_hMainWindow);
        ShowWindow(g_hMainWindow, SW_SHOW);

        // Add Launch Cheat button
        HWND hLaunchBtn = CreateWindowEx(
            0, "BUTTON", "Launch Prisme",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            160, 320, 200, 45,
            g_hMainWindow, (HMENU)1010, g_hInstance, NULL
        );
        SendMessage(hLaunchBtn, WM_SETFONT, (WPARAM)g_hButtonFont, TRUE);

        // Status label below button
        g_hMainStatusLabel = CreateWindow("STATIC", "",
            WS_VISIBLE | WS_CHILD | SS_CENTER,
            60, 375, 400, 30,
            g_hMainWindow, NULL, g_hInstance, NULL);
        SendMessage(g_hMainStatusLabel, WM_SETFONT, (WPARAM)g_hNormalFont, TRUE);
    }
}

void ShowPasswordPrompt() {
    const char LOGIN_CLASS_NAME[] = "LoginWindowClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc = LoginWindowProc;
    wc.hInstance = g_hInstance;
    wc.lpszClassName = LOGIN_CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(101));
    wc.hbrBackground = CreateSolidBrush(COLOR_BG_DARK);

    RegisterClass(&wc);

    g_hLoginWindow = CreateWindowEx(
        0,
        LOGIN_CLASS_NAME,
        "Prisme ASA",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 520, 530,
        NULL, NULL, g_hInstance, NULL
    );

    if (g_hLoginWindow != NULL) {
        EnableDarkTitleBar(g_hLoginWindow);
        ShowWindow(g_hLoginWindow, SW_SHOW);
    }
}

// Startup flow
void FinishStartup() {
    // First check if we have a valid license
    LicenseData license = CheckExistingLicense();

    if (license.isValid) {
        // Valid license - go straight to main app
        ShowMainWindow();
    } else {
        // No valid license - show login prompt
        ShowPasswordPrompt();
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_hInstance = hInstance;

    // Create fonts globally - larger sizes with better readability
    g_hTitleFont = CreateFont(56, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI Light");
    g_hNormalFont = CreateFont(22, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    g_hButtonFont = CreateFont(22, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI Semibold");
    g_hSmallFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    g_hInputFont = CreateFont(24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    g_hLabelFont = CreateFont(13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI Semibold");

    // Create brushes
    g_hBgBrush = CreateSolidBrush(COLOR_BG_DARK);
    g_hCardBrush = CreateSolidBrush(COLOR_BG_CARD);
    g_hInputBrush = CreateSolidBrush(COLOR_BG_INPUT);

    // Check for existing license or show login window
    FinishStartup();

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup fonts
    if (g_hTitleFont) DeleteObject(g_hTitleFont);
    if (g_hNormalFont) DeleteObject(g_hNormalFont);
    if (g_hButtonFont) DeleteObject(g_hButtonFont);
    if (g_hSmallFont) DeleteObject(g_hSmallFont);
    if (g_hInputFont) DeleteObject(g_hInputFont);
    if (g_hLabelFont) DeleteObject(g_hLabelFont);

    // Cleanup brushes
    if (g_hBgBrush) DeleteObject(g_hBgBrush);
    if (g_hCardBrush) DeleteObject(g_hCardBrush);
    if (g_hInputBrush) DeleteObject(g_hInputBrush);
    return 0;
}