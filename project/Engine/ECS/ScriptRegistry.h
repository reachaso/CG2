#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include <vector>
#include "ScriptableEntity.h"

/// @brief A registry for mapping script type names to instantiation functions.
/// This allows us to instantiate C++ scripts dynamically by name from the Editor UI.
class ScriptRegistry {
public:
    using InstantiateFn = std::function<ScriptableEntity*()>;

    /// @brief Register a script type by name.
    static void Register(const std::string& name, InstantiateFn func);

    /// @brief Instantiate a script by name.
    static ScriptableEntity* Instantiate(const std::string& name);

    /// @brief Get a list of all registered script names.
    static const std::vector<std::string>& GetScriptNames();

private:
    static std::unordered_map<std::string, InstantiateFn>& GetRegistry();
    static std::vector<std::string>& GetNames();
};

/// @brief Macro to automatically register a ScriptableEntity class before main() executes.
/// Use this at the bottom of your script's .cpp file (or in a global scope in .h if you prefer, but .cpp is safer).
#define REGISTER_SCRIPT(Type) \
    namespace { \
        struct ScriptRegisterer_##Type { \
            ScriptRegisterer_##Type() { \
                ScriptRegistry::Register(#Type, []() { return new Type(); }); \
            } \
        }; \
        static ScriptRegisterer_##Type g_ScriptRegisterer_##Type; \
    }
