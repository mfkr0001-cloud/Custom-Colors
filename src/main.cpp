#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/CharacterColorPage.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>

using namespace geode::prelude;

namespace CustomColors {
    static ccColor3B s_p1Color = { 255, 64, 64 };
    static ccColor3B s_p2Color = { 64, 128, 255 };
    static ccColor3B s_glowColor = { 255, 255, 255 };

    static std::string colorToHex(ccColor3B color) {
        std::ostringstream out;
        out << '#' << std::uppercase << std::hex << std::setfill('0')
            << std::setw(2) << static_cast<int>(color.r)
            << std::setw(2) << static_cast<int>(color.g)
            << std::setw(2) << static_cast<int>(color.b);
        return out.str();
    }

    static bool parseHex(std::string const& value, ccColor3B& result) {
        if (value.size() != 7 || value[0] != '#') return false;
        auto isHex = [](char c) { return std::isxdigit(static_cast<unsigned char>(c)) != 0; };
        for (size_t i = 1; i < value.size(); ++i) if (!isHex(value[i])) return false;

        try {
            result.r = static_cast<GLubyte>(std::stoul(value.substr(1, 2), nullptr, 16));
            result.g = static_cast<GLubyte>(std::stoul(value.substr(3, 2), nullptr, 16));
            result.b = static_cast<GLubyte>(std::stoul(value.substr(5, 2), nullptr, 16));
            return true;
        } catch (...) {
            return false;
        }
    }

    static std::string jsonStringField(std::string const& json, std::string const& key) {
        auto keyPos = json.find('"' + key + '"');
        if (keyPos == std::string::npos) return {};
        auto colon = json.find(':', keyPos);
        if (colon == std::string::npos) return {};
        auto firstQuote = json.find('"', colon + 1);
        if (firstQuote == std::string::npos) return {};
        auto lastQuote = json.find('"', firstQuote + 1);
        if (lastQuote == std::string::npos) return {};
        return json.substr(firstQuote + 1, lastQuote - firstQuote - 1);
    }

    // Простая salted-проверка пароля для защиты от случайного импорта.
    // Это НЕ шифрование: содержимое .ccprofile остаётся читаемым текстом.
    static std::string passwordVerifier(std::string const& salt, std::string const& password) {
        uint64_t hash = 14695981039346656037ull;
        std::string input = salt + ':' + password;
        for (int round = 0; round < 20000; ++round) {
            for (unsigned char c : input) {
                hash ^= c;
                hash *= 1099511628211ull;
            }
            input = std::to_string(hash) + salt + password;
        }
        std::ostringstream out;
        out << std::hex << std::setfill('0') << std::setw(16) << hash;
        return out.str();
    }

    static std::string makeSalt() {
        std::random_device random;
        std::ostringstream out;
        out << std::hex << std::setfill('0');
        for (int i = 0; i < 4; ++i) out << std::setw(8) << random();
        return out.str();
    }

    static void loadColors() {
        auto* mod = Mod::get();
        s_p1Color = mod->getSettingValue<ccColor3B>("p1-color");
        s_p2Color = mod->getSettingValue<ccColor3B>("p2-color");
        s_glowColor = mod->getSettingValue<ccColor3B>("glow-color");
    }

    static void applyToPlayer(PlayerObject* player) {
        if (!player) return;
        // P1 = первый цвет; P2 = второй цвет; Glow = цвет свечения.
        player->setColor(s_p1Color);
        player->setSecondColor(s_p2Color);
        player->enableCustomGlowColor(s_glowColor);
    }

    static void applyToSimplePlayer(SimplePlayer* player) {
        if (!player) return;
        player->setColors(s_p1Color, s_p2Color);
        player->enableCustomGlowColor(s_glowColor);
    }

    static bool writeProfile(std::filesystem::path const& path) {
        if (path.empty()) return false;
        auto* mod = Mod::get();
        auto p1 = mod->getSettingValue<ccColor3B>("p1-color");
        auto p2 = mod->getSettingValue<ccColor3B>("p2-color");
        auto glow = mod->getSettingValue<ccColor3B>("glow-color");
        auto password = mod->getSettingValue<std::string>("profile-password");

        std::filesystem::path outputPath = path;
        if (outputPath.extension() != ".ccprofile") outputPath += ".ccprofile";

        std::ofstream file(outputPath, std::ios::binary | std::ios::trunc);
        if (!file) return false;

        file << "{\n"
             << "  \"format\": \"custom-colors-profile\",\n"
             << "  \"version\": 1,\n"
             << "  \"colors\": {\n"
             << "    \"p1\": \"" << colorToHex(p1) << "\",\n"
             << "    \"p2\": \"" << colorToHex(p2) << "\",\n"
             << "    \"glow\": \"" << colorToHex(glow) << "\"\n"
             << "  }";

        if (!password.empty()) {
            auto salt = makeSalt();
            file << ",\n  \"passwordProtection\": {\n"
                 << "    \"salt\": \"" << salt << "\",\n"
                 << "    \"verifier\": \"" << passwordVerifier(salt, password) << "\"\n"
                 << "  }";
        }
        file << "\n}\n";
        return file.good();
    }

    static bool readProfile(std::filesystem::path const& path) {
        if (path.empty()) return false;
        std::ifstream file(path, std::ios::binary);
        if (!file) return false;
        std::string json((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if (json.size() > 64 * 1024) return false;
        if (json.find("\"format\": \"custom-colors-profile\"") == std::string::npos) return false;
        if (json.find("\"version\": 1") == std::string::npos) return false;

        ccColor3B p1, p2, glow;
        if (!parseHex(jsonStringField(json, "p1"), p1) ||
            !parseHex(jsonStringField(json, "p2"), p2) ||
            !parseHex(jsonStringField(json, "glow"), glow)) {
            return false;
        }

        auto salt = jsonStringField(json, "salt");
        auto verifier = jsonStringField(json, "verifier");
        if (!salt.empty() || !verifier.empty()) {
            if (salt.empty() || verifier.empty()) return false;
            auto password = Mod::get()->getSettingValue<std::string>("profile-password");
            if (password.empty() || passwordVerifier(salt, password) != verifier) {
                log::warn("Custom Colors: неверный пароль профиля или пароль не введён");
                return false;
            }
        }

        // Меняем именно настройки Geode, чтобы интерфейс и игровые цвета совпадали.
        Mod::get()->setSettingValue<ccColor3B>("p1-color", p1);
        Mod::get()->setSettingValue<ccColor3B>("p2-color", p2);
        Mod::get()->setSettingValue<ccColor3B>("glow-color", glow);
        s_p1Color = p1;
        s_p2Color = p2;
        s_glowColor = glow;
        return true;
    }
}

using namespace CustomColors;

class $modify(CustomColorsPlayerObject, PlayerObject) {
    void update(float dt) {
        PlayerObject::update(dt);
        applyToPlayer(this);
    }
};

class $modify(CustomColorsColorPage, CharacterColorPage) {
    void updateIconColors() {
        CharacterColorPage::updateIconColors();
        if (!this->m_playerObjects) return;

        for (unsigned int i = 0; i < this->m_playerObjects->count(); ++i) {
            auto* player = typeinfo_cast<SimplePlayer*>(this->m_playerObjects->objectAtIndex(i));
            applyToSimplePlayer(player);
        }
    }
};

$on_mod(Loaded) {
    loadColors();

    listenForSettingChanges<ccColor3B>("p1-color", +[](ccColor3B color) { s_p1Color = color; });
    listenForSettingChanges<ccColor3B>("p2-color", +[](ccColor3B color) { s_p2Color = color; });
    listenForSettingChanges<ccColor3B>("glow-color", +[](ccColor3B color) { s_glowColor = color; });

    listenForSettingChanges<std::filesystem::path>("export-profile", +[](std::filesystem::path path) {
        if (path.empty()) return;
        if (writeProfile(path)) log::info("Custom Colors: профиль экспортирован");
        else log::error("Custom Colors: не удалось экспортировать профиль");
    });

    listenForSettingChanges<std::filesystem::path>("import-profile", +[](std::filesystem::path path) {
        if (path.empty()) return;
        if (readProfile(path)) log::info("Custom Colors: профиль импортирован");
        else log::error("Custom Colors: ошибка импорта профиля. Проверь файл и пароль");
    });

    log::info("Custom Colors 1.1.0 загружен");
}
