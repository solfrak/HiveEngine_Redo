#pragma once
#include <hive/core/module.h>
#include <hive/utils/singleton.h>

#include <memory>
#include <vector>
namespace hive
{

    class ModuleRegistry : public Singleton<ModuleRegistry>
    {
    public:
        ModuleRegistry() = default;
        ~ModuleRegistry() = default;

        using ModuleFactoryFn = std::unique_ptr<Module>(*)();
        void RegisterModule(ModuleFactoryFn fn);

        void CreateModules();
        void ConfigureModules();
        void InitModules();
        void ShutdownModules();

    private:
        std::vector<ModuleFactoryFn> m_ModuleFactories;
        std::vector<std::unique_ptr<Module>> m_Modules;
    };

    // Helper template for automatic module registration
    // Usage: static hive::ModuleRegistrar<MyModule> s_MyModuleRegistrar;
    template<typename ModuleClass>
    class ModuleRegistrar
    {
    public:
        ModuleRegistrar()
        {
            ModuleRegistry::GetInstance().RegisterModule([]() -> std::unique_ptr<Module> {
                return std::make_unique<ModuleClass>();
            });
        }
    };
}

