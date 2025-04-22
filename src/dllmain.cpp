#include "stdafx.h"
#include "helper.hpp"
#include <string>
#include <string_view>

#include <inipp/inipp.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>
#include <safetyhook.hpp>

#include "logging.hpp"
#include "version.h"
#include "version_checking.hpp"

HMODULE baseModule = GetModuleHandle(NULL);

// Version
std::string sFixName = FIX_NAME;
std::string sFixVer = VERSION_STRING;

// Logger
std::shared_ptr<spdlog::logger> logger;
std::string sLogFile = sFixName + ".log";
std::filesystem::path sExePath;
std::string sExeName;

bool bShouldCheckForUpdates;
bool bConsoleUpdateNotifications;

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

static void ReadConfig()
{
    // Initialise config
    std::ifstream iniFile((sExePath/ sFixPath / sConfigFile).string());
    if (!iniFile) {
        AllocConsole();
        FILE* dummy;
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        std::cout << "" << sFixName.c_str() << " v" << sFixVer.c_str() << " loaded." << std::endl;
        std::cout << "ERROR: Could not locate config file." << std::endl;
        std::cout << "ERROR: Make sure " << sConfigFile.c_str() << " is located in " << sExePath / sFixPath << std::endl;
        FreeLibraryAndExitThread(baseModule, 1);
    }
    else {
        spdlog::info("Config file: {}", (sExePath / sFixPath / sConfigFile).string());
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
	
	inipp::get_value(ini.sections["Update Notifications"], "CheckForUpdates", bShouldCheckForUpdates);
	inipp::get_value(ini.sections["Update Notifications"], "ConsoleNotifications", bConsoleUpdateNotifications);

	spdlog::info("[Config] bShouldCheckForUpdates: {}", bShouldCheckForUpdates);
	spdlog::info("[Config] bConsoleUpdateNotifications: {}", bConsoleUpdateNotifications);

    inipp::get_value(ini.sections["Update Notifications"], "CheckForUpdates", bShouldCheckForUpdates);
    inipp::get_value(ini.sections["Update Notifications"], "ConsoleNotifications", bConsoleUpdateNotifications);

    spdlog::info("[Config] bShouldCheckForUpdates: {}", bShouldCheckForUpdates);
    spdlog::info("[Config] bConsoleUpdateNotifications: {}", bConsoleUpdateNotifications);

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
    DOGTAG_CODENAMEHook = safetyhook::create_mid(Memory::PatternScan(baseModule, "E8 ?? ?? ?? ?? 41 B8 ?? ?? ?? ?? C7 87 ?? ?? ?? ?? ?? ?? ?? ?? 48 8D 15 ?? ?? ?? ?? C7 87 ?? ?? ?? ?? ?? ?? ?? ?? 48 8B CF E8 ?? ?? ?? ?? 41 B8 ?? ?? ?? ?? C7 87 ?? ?? ?? ?? ?? ?? ?? ?? 48 8D 15 ?? ?? ?? ?? C7 87 ?? ?? ?? ?? ?? ?? ?? ?? 48 8B CF E8 ?? ?? ?? ?? 41 B8", "codename"),
        [](SafetyHookContext& ctx)
        {
            ctx.rdx = reinterpret_cast<uintptr_t>(pNameField.c_str());
        });

    static SafetyHookMid DOGTAG_SEXHook{};
    DOGTAG_SEXHook = safetyhook::create_mid(Memory::PatternScan(baseModule, "E8 ?? ?? ?? ?? 41 B8 ?? ?? ?? ?? C7 87 ?? ?? ?? ?? ?? ?? ?? ?? 48 8D 15 ?? ?? ?? ?? C7 87 ?? ?? ?? ?? ?? ?? ?? ?? 48 8B CF E8 ?? ?? ?? ?? 4C 8B 05", "sex 1"), //SEX
        [](SafetyHookContext& ctx)
        {
            ctx.rdx = reinterpret_cast<uintptr_t>(&"Sex                Blood");
        });

    static SafetyHookMid DOGTAG_YEARHook{}; //year
    DOGTAG_YEARHook = safetyhook::create_mid(Memory::PatternScan(baseModule, "48 8D 4C 24 ?? 44 8B 88", "year 1"),
        [](SafetyHookContext& ctx)
        {
            std::string finalString = "%02d/%02d/";
            finalString += pConfigYear;
            ctx.rdx = reinterpret_cast<uintptr_t>(finalString.c_str());
        });

    static SafetyHookMid DOGTAG_sexHook{}; //sex
    DOGTAG_sexHook = safetyhook::create_mid(Memory::PatternScan(baseModule, "E8 ?? ?? ?? ?? 45 8B C5 C7 87", "sex"),
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
    DOGTAG_NATIONALITYHook = safetyhook::create_mid(Memory::PatternScan(baseModule, "E8 ?? ?? ?? ?? 0F BE 44 24 ?? 48 8D 4C 24 ?? 45 8B C7", "Year"), //year
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


static void CheckForUpdates()
{
    if (!bShouldCheckForUpdates)
    {
        spdlog::info("Mod update checking disabled via config.");
        return;
    }
    const std::filesystem::path cacheFilePath = (sExePath / "mgs2_savedata_win" / (sFixName + "_version_check.txt"));
    LatestVersionChecker checker(cacheFilePath);
    checker.checkForUpdates();
}

std::mutex mainThreadFinishedMutex;
std::condition_variable mainThreadFinishedVar;
bool mainThreadFinished = false;

static DWORD __stdcall Main(void*)
{
    Logging::Initialize();
    if (DetectGame())
    {
        ReadConfig();
        dogtagHooks();
		
        CheckForUpdates();

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

        // Unhook ourselves: restore previous pointer
        Memory::WriteIAT(baseModule, "VCRUNTIME140.dll", "memset", memset_Fn);

        // Wait for main thread to finish initialization
        std::unique_lock finishedLock(mainThreadFinishedMutex);
        mainThreadFinishedVar.wait(finishedLock, []
            {
                return mainThreadFinished;
            });
    }

    // Call the previous memset (whatever was there when we hooked)
    return reinterpret_cast<decltype(memset_Fn)>(memset_Fn)(Dst, Val, Size);
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
            // Read whatever is currently in IAT for memset (in case another mod is already hooking it!)
            void* currentIATMemset = Memory::ReadIAT(baseModule, "VCRUNTIME140.dll", "memset");
            memset_Fn = reinterpret_cast<decltype(memset_Fn)>(currentIATMemset);
            Memory::WriteIAT(baseModule, "VCRUNTIME140.dll", "memset", &memset_Hook);
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
