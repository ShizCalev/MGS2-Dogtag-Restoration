#include "stdafx.h"
#include "helper.hpp"
#include <string>
#include <string_view>

#include <inipp/inipp.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>
#include <safetyhook.hpp>

HMODULE baseModule = GetModuleHandle(NULL);

// Version
string sFixName = "MGS2-Dogtag-Restoration";
string sFixVer = "1.0.1";

// Logger
std::shared_ptr<spdlog::logger> logger;
std::string sLogFile = sFixName + ".log";
std::filesystem::path sExePath;
std::string sExeName;
std::string sFixPath;


// Ini
inipp::Ini<char> ini;
std::string sConfigFile = sFixName + ".ini";


// Ini Variables
std::string pCustomNation;
std::string pConfigYear = "1983";
std::string pCustomsex;
std::string pbloodtype = "?";
std::string pNameField = "Name";



struct GameInfo
{
    std::string GameTitle;
    std::string ExeName;
    int SteamAppId;
};

enum class MgsGame
{
    Unknown,
    MGS2,
    MGS3,
    MG,
    Launcher
};

const std::map<MgsGame, GameInfo> kGames = {
    {MgsGame::MGS2, {"Metal Gear Solid 2 HD", "METAL GEAR SOLID2.exe", 2131640}},
    {MgsGame::MGS3, {"Metal Gear Solid 3 HD", "METAL GEAR SOLID3.exe", 2131650}},
    {MgsGame::MG, {"Metal Gear / Metal Gear 2 (MSX)", "METAL GEAR.exe", 2131680}},
};

const GameInfo* game = nullptr;
MgsGame eGameType = MgsGame::Unknown;

#pragma region Logging

#pragma region Spdlog sink
// Spdlog sink (truncate on startup, single file)
template<typename Mutex>
class size_limited_sink : public spdlog::sinks::base_sink<Mutex> {
public:
    explicit size_limited_sink(const std::string& filename, size_t max_size)
        : _filename(filename), _max_size(max_size) {
        truncate_log_file();

        _file.open(_filename, std::ios::app);
        if (!_file.is_open()) {
            throw spdlog::spdlog_ex("Failed to open log file " + filename);
        }
    }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        if (std::filesystem::exists(_filename) && std::filesystem::file_size(_filename) >= _max_size) {
            return;
        }

        spdlog::memory_buf_t formatted;
        this->formatter_->format(msg, formatted);

        _file.write(formatted.data(), formatted.size());
        _file.flush();
    }

    void flush_() override {
        _file.flush();
    }

private:
    std::ofstream _file;
    std::string _filename;
    size_t _max_size;

    void truncate_log_file() {
        if (std::filesystem::exists(_filename)) {
            std::ofstream ofs(_filename, std::ofstream::out | std::ofstream::trunc);
            ofs.close();
        }
    }
};

#pragma endregion Spdlog sink

void Logging()
{
    // Get game name and exe path
    WCHAR exePath[_MAX_PATH] = { 0 };
    GetModuleFileNameW(baseModule, exePath, MAX_PATH);
    sExePath = exePath;
    sExeName = sExePath.filename().string();
    sExePath = sExePath.remove_filename();

    std::string paths[4] = { "", "plugins\\", "scripts\\", "update\\" };
    for (int i = 0; i < (sizeof(paths) / sizeof(paths[0])); i++) {
        if (std::filesystem::exists(sExePath.string() + paths[i] + sFixName + ".asi")) {
            if (sFixPath.length()) { //multiple versions found
                AllocConsole();
                FILE* dummy;
                freopen_s(&dummy, "CONOUT$", "w", stdout);
                std::cout << "\n" << sFixName + " ERROR: Duplicate .asi installations found! Please make sure to delete any old versions!" << std::endl;
                FreeLibraryAndExitThread(baseModule, 1);
                return;
            }
            sFixPath = paths[i];
        }
    }
    // spdlog initialisation
    {
        try {
            if (!std::filesystem::is_directory(sExePath.string() + "logs"))
                std::filesystem::create_directory(sExePath.string() + "logs"); //create a "logs" subdirectory in the game folder to keep the main directoy tidy.
            // Create 10MB truncated logger
            logger = std::make_shared<spdlog::logger>(sLogFile, std::make_shared<size_limited_sink<std::mutex>>(sExePath.string() + "logs\\" + sLogFile, 10 * 1024 * 1024));
            spdlog::set_default_logger(logger);

            spdlog::flush_on(spdlog::level::debug);
            spdlog::info("----------");
            spdlog::info("{} v{} loaded.", sFixName.c_str(), sFixVer.c_str());
            spdlog::info("ASI plugin location: {}", sExePath.string() + sFixPath + sFixName + ".asi");

            spdlog::info("----------");
            spdlog::info("Log file: {}", sExePath.string() + "logs\\" + sLogFile);
            spdlog::info("----------");

            // Log module details
            spdlog::info("Module Name: {0:s}", sExeName.c_str());
            spdlog::info("Module Path: {0:s}", sExePath.string());
            spdlog::info("Module Address: 0x{0:x}", (uintptr_t)baseModule);
            spdlog::info("Module Timestamp: {0:d}", Memory::ModuleTimestamp(baseModule));
            spdlog::info("----------");
        }
        catch (const spdlog::spdlog_ex& ex) {
            AllocConsole();
            FILE* dummy;
            freopen_s(&dummy, "CONOUT$", "w", stdout);
            std::cout << "Log initialisation failed: " << ex.what() << std::endl;
            FreeLibraryAndExitThread(baseModule, 1);
        }
    }
}
#pragma endregion Logging

static void ReadConfig()
{
    // Initialise config
    std::ifstream iniFile(sExePath.string() + sFixPath + sConfigFile);
    if (!iniFile) {
        AllocConsole();
        FILE* dummy;
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        std::cout << "" << sFixName.c_str() << " v" << sFixVer.c_str() << " loaded." << std::endl;
        std::cout << "ERROR: Could not locate config file." << std::endl;
        std::cout << "ERROR: Make sure " << sConfigFile.c_str() << " is located in " << sExePath.string().c_str() + sFixPath << std::endl;
        FreeLibraryAndExitThread(baseModule, 1);
    }
    else {
        spdlog::info("Config file: {}", sExePath.string() + sFixPath + sConfigFile);
        ini.parse(iniFile);
    }


    // Read ini file
    std::string name_field;
    std::string custom_nation;
    std::string custom_sex;
    std::string blood_type;
    std::string year_of_birth;

    inipp::get_value(ini.sections["Settings"], "name_field", name_field);
    inipp::get_value(ini.sections["Settings"], "custom_nation", custom_nation);
    inipp::get_value(ini.sections["Settings"], "custom_sex", custom_sex);
    inipp::get_value(ini.sections["Settings"], "blood_type", blood_type);
    inipp::get_value(ini.sections["Settings"], "year_of_birth", year_of_birth);

    if (name_field.length()) {
        pNameField = name_field;
        spdlog::info("Config Load - Name Field: {}", name_field.c_str());
    }
    if (custom_nation.length()) {
        pCustomNation = custom_nation;
        spdlog::info("Config Load - Custom Nation: {}", pCustomNation.c_str());
    }
    if (custom_sex.length()) {
        pCustomsex = custom_sex;
        spdlog::info("Config Load - Custom sex: {}", custom_sex.c_str());
    }
    if (blood_type.length()) {
        pbloodtype = blood_type;
        spdlog::info("Config Load - Blood Type: {}", blood_type.c_str());
    }
    if (year_of_birth.length()) {
        pConfigYear = year_of_birth;
        spdlog::info("Config Load - Year of Birth: {}", year_of_birth.c_str());
    }


}

static bool DetectGame()
{
    eGameType = MgsGame::Unknown;
    // Special handling for launcher.exe
    if (sExeName == "launcher.exe")
    {
        for (const auto& [type, info] : kGames)
        {
            auto gamePath = sExePath.parent_path() / info.ExeName;
            if (std::filesystem::exists(gamePath))
            {
                spdlog::info("Detected launcher for game: {} (app {})", info.GameTitle.c_str(), info.SteamAppId);
                eGameType = MgsGame::Launcher;
                game = &info;
                return false;
            }
        }

        spdlog::error("Failed to detect supported game, unknown launcher");
        return false;
    }

    for (const auto& [type, info] : kGames)
    {
        if (info.ExeName == sExeName)
        {
            spdlog::info("Detected game: {} (app {})", info.GameTitle.c_str(), info.SteamAppId);
            eGameType = type;
            game = &info;
            if (eGameType == MgsGame::MGS2) {
                return true;
            }
            
        }
    }

    spdlog::error("Failed to detect supported game, {} isn't supported by MGS2-Dogtag-Restoration", sExeName.c_str());
    return false;
}


static void dogtagHooks()
{



    static SafetyHookMid DOGTAG_CODENAMEHook{}; //code name
    DOGTAG_CODENAMEHook = safetyhook::create_mid(Memory::PatternScan(baseModule, "E8 ?? ?? ?? ?? 41 B8 ?? ?? ?? ?? C7 87 ?? ?? ?? ?? ?? ?? ?? ?? 48 8D 15 ?? ?? ?? ?? C7 87 ?? ?? ?? ?? ?? ?? ?? ?? 48 8B CF E8 ?? ?? ?? ?? 41 B8 ?? ?? ?? ?? C7 87 ?? ?? ?? ?? ?? ?? ?? ?? 48 8D 15 ?? ?? ?? ?? C7 87 ?? ?? ?? ?? ?? ?? ?? ?? 48 8B CF E8 ?? ?? ?? ?? 41 B8"),
        [](SafetyHookContext& ctx)
        {
            ctx.rdx = reinterpret_cast<uintptr_t>(pNameField.c_str());
        });

    static SafetyHookMid DOGTAG_SEXHook{};
    DOGTAG_SEXHook = safetyhook::create_mid(Memory::PatternScan(baseModule, "E8 ?? ?? ?? ?? 41 B8 ?? ?? ?? ?? C7 87 ?? ?? ?? ?? ?? ?? ?? ?? 48 8D 15 ?? ?? ?? ?? C7 87 ?? ?? ?? ?? ?? ?? ?? ?? 48 8B CF E8 ?? ?? ?? ?? 4C 8B 05"), //SEX
        [](SafetyHookContext& ctx)
        {
            ctx.rdx = reinterpret_cast<uintptr_t>(&"Sex                Blood");
        });

    static SafetyHookMid DOGTAG_YEARHook{}; //year
    DOGTAG_YEARHook = safetyhook::create_mid(Memory::PatternScan(baseModule, "48 8D 4C 24 ?? 44 8B 88"),
        [](SafetyHookContext& ctx)
        {
            std::string finalString = "%02d/%02d/";
            finalString += pConfigYear;
            ctx.rdx = reinterpret_cast<uintptr_t>(finalString.c_str());
        });

    static SafetyHookMid DOGTAG_sexHook{}; //sex
    DOGTAG_sexHook = safetyhook::create_mid(Memory::PatternScan(baseModule, "E8 ?? ?? ?? ?? 45 8B C5 C7 87"),
        [](SafetyHookContext& ctx)
        {
            std::string sex = pCustomsex.length() ? pCustomsex : reinterpret_cast<const char*>(ctx.r8);
            int length = sex.length();

            std::string finalString;
            for (int i = 0; i < ((21 + pbloodtype.length()) - length); i++) {
                finalString += " ";
            }
            finalString += sex;
            for (int i = 0; i < (24 - (2 * length) - (pbloodtype.length()-1)); i++) {
                finalString += " ";
            }
            transform(pbloodtype.begin(), pbloodtype.end(), pbloodtype.begin(), ::toupper);
            finalString += pbloodtype;

            ctx.r8 = reinterpret_cast<uintptr_t>(finalString.c_str());
               // ctx.r8 = reinterpret_cast<uintptr_t>(&"                  MALE                O");
               // ctx.r8 = reinterpret_cast<uintptr_t>(&"                FEMALE            O");
                                                    //    (22 - length)word(24 - (2 * length))
                                                        //          21  1  22
                                                        //          20  2  20
                                                        //          19  3  18
                                                        //          18  4  16
                                                        //          17  5  14
                                                        //          16  6  12
                                                        //          15  7  10
                                                        //          
                                                        //    

        });

    static SafetyHookMid DOGTAG_NATIONALITYHook{}; //Nation
    DOGTAG_NATIONALITYHook = safetyhook::create_mid(Memory::PatternScan(baseModule, "E8 ?? ?? ?? ?? 0F BE 44 24 ?? 48 8D 4C 24 ?? 45 8B C7"), //year
        [](SafetyHookContext& ctx)
        {
            if (!pCustomNation.length()) {

                const char* country = reinterpret_cast<const char*>(ctx.r8);
                if (strcmp(country, "Uk") == 0) {
                    ctx.r8 = reinterpret_cast<uintptr_t>(&"United Kingdom");
                }
                else if (strcmp(country, "Uae") == 0) {
                    ctx.r8 = reinterpret_cast<uintptr_t>(&"United Arab Emirates");
                }
                else if (strcmp(country, "Usa") == 0) {
                    ctx.r8 = reinterpret_cast<uintptr_t>(&"United States");
                }
                else if (strcmp(country, "Cote D") == 0) {
                    ctx.r8 = reinterpret_cast<uintptr_t>(&"Cote d'Ivoire");
                }
                else if (strcmp(country, "Turkey") == 0) {
                    ctx.r8 = reinterpret_cast<uintptr_t>(&"Turkiye");
                }
                else if (strcmp(country, "N. Korea") == 0) {
                    ctx.r8 = reinterpret_cast<uintptr_t>(&"North Korea");
                }
                else if (strcmp(country, "S. Korea") == 0) {
                    ctx.r8 = reinterpret_cast<uintptr_t>(&"South Korea");
                }
                else if (strcmp(country, "Macedonia") == 0) {
                    ctx.r8 = reinterpret_cast<uintptr_t>(&"North Macedonia");
                }
                else if (strcmp(country, "Swaziland") == 0) {
                    ctx.r8 = reinterpret_cast<uintptr_t>(&"Eswatini");
                }
                else if (strcmp(country, "Czech Rep.") == 0) {
                    ctx.r8 = reinterpret_cast<uintptr_t>(&"Czech Republic");
                }
                else if (strcmp(country, "St. Kitts & Nevis") == 0) {
                    ctx.r8 = reinterpret_cast<uintptr_t>(&"Saint Kitts and Nevis");
                }
            }
            else { //bConfigCustomNation = TRUE
                ctx.r8 = reinterpret_cast<uintptr_t>(pCustomNation.c_str());
            }
        });


}

#pragma region HookInit

std::mutex mainThreadFinishedMutex;
std::condition_variable mainThreadFinishedVar;
bool mainThreadFinished = false;

static DWORD __stdcall Main(void*)
{
    Logging();
    if (DetectGame())
    {
        ReadConfig();
        dogtagHooks();
    }

    // Signal any threads which might be waiting for us before continuing
    {
        std::lock_guard lock(mainThreadFinishedMutex);
        mainThreadFinished = true;
        mainThreadFinishedVar.notify_all();
    }

    return true;
}

std::mutex memsetHookMutex;
bool memsetHookCalled = false;
void* (__cdecl* memset_Fn)(void* Dst, int Val, size_t Size);
void* __cdecl memset_Hook(void* Dst, int Val, size_t Size)
{
    // memset is one of the first imports called by game (not the very first though, since ASI loader still has those hooked during our DllMain...)
    std::lock_guard lock(memsetHookMutex);
    if (!memsetHookCalled)
    {
        memsetHookCalled = true;

        // First we'll unhook the IAT for this function as early as we can
        Memory::HookIAT(baseModule, "VCRUNTIME140.dll", memset_Hook, memset_Fn);

        // Wait for our main thread to finish before we return to the game
        if (!mainThreadFinished)
        {
            std::unique_lock finishedLock(mainThreadFinishedMutex);
            mainThreadFinishedVar.wait(finishedLock, [] { return mainThreadFinished; });
        }
    }

    return memset_Fn(Dst, Val, Size);
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        // Try hooking IAT of one of the imports game calls early on, so we can make it wait for our Main thread to complete before returning back to game
        // This will only hook the main game modules usage of memset, other modules calling it won't be affected
        HMODULE vcruntime140 = GetModuleHandleA("VCRUNTIME140.dll");
        if (vcruntime140)
        {
            memset_Fn = decltype(memset_Fn)(GetProcAddress(vcruntime140, "memset"));
            Memory::HookIAT(baseModule, "VCRUNTIME140.dll", memset_Fn, memset_Hook);
        }

        HANDLE mainHandle = CreateThread(NULL, 0, Main, 0, NULL, 0);
        if (mainHandle)
        {
            SetThreadPriority(mainHandle, THREAD_PRIORITY_HIGHEST); // set our Main thread priority higher than the games thread
            CloseHandle(mainHandle);
        }
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

#pragma endregion HookInit
