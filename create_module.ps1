# PowerShell script to create a new module for HiveEngine
# Usage: .\create_module.ps1 <ModuleName> [-WithModuleClass]

param(
    [Parameter(Mandatory=$true, Position=0)]
    [string]$ModuleName,

    [Parameter(Mandatory=$false)]
    [switch]$WithModuleClass
)

$ErrorActionPreference = "Stop"

# Convert module name to lowercase
$ModuleLower = $ModuleName.ToLower()

# Check if module already exists
if (Test-Path $ModuleName) {
    Write-Host "Error: Module '$ModuleName' already exists!" -ForegroundColor Red
    exit 1
}

Write-Host "Creating module: $ModuleName" -ForegroundColor Green

# Create directory structure
Write-Host "Creating directory structure..."
New-Item -ItemType Directory -Path "$ModuleName\include\$ModuleLower" -Force | Out-Null
New-Item -ItemType Directory -Path "$ModuleName\src\$ModuleLower" -Force | Out-Null

# Create precomp.h
Write-Host "Creating precomp.h..."
$precompContent = @"
#pragma once

#include <hive/precomp.h>

// Add module-specific precompiled headers here
"@
Set-Content -Path "$ModuleName\include\$ModuleLower\precomp.h" -Value $precompContent

# Create CMakeLists.txt
Write-Host "Creating CMakeLists.txt..."
$cmakeContent = @"
cmake_minimum_required(VERSION 3.30.5)

add_library($ModuleLower STATIC)
target_include_directories($ModuleLower PUBLIC include PRIVATE src)
target_precompile_headers($ModuleLower PRIVATE include/$ModuleLower/precomp.h)

# Link against Hive (core module)
target_link_libraries($ModuleLower PUBLIC hive)

# Add source files here
# target_sources($ModuleLower PRIVATE
#     src/$ModuleLower/example.cpp
# )
"@
Set-Content -Path "$ModuleName\CMakeLists.txt" -Value $cmakeContent

# Create module class if requested
if ($WithModuleClass) {
    Write-Host "Creating module class..."

    # Create module header
    $moduleHeader = @"
#pragma once

#include <hive/core/module.h>

namespace $ModuleLower
{
    class ${ModuleName}Module : public hive::Module
    {
    public:
        ${ModuleName}Module() = default;
        ~${ModuleName}Module() override = default;

        const char* GetName() const override { return GetStaticName(); }
        static const char* GetStaticName() { return "$ModuleName"; }

    protected:
        void DoConfigure(hive::ModuleContext& context) override;
        void DoInitialize() override;
        void DoShutdown() override;
    };
}
"@
    Set-Content -Path "$ModuleName\include\$ModuleLower\${ModuleLower}module.h" -Value $moduleHeader

    # Create module implementation
    $moduleImpl = @"
#include <$ModuleLower/precomp.h>
#include <$ModuleLower/${ModuleLower}module.h>
#include <hive/core/log.h>

namespace $ModuleLower
{
    void ${ModuleName}Module::DoConfigure(hive::ModuleContext& context)
    {
        // Add dependencies here
        // Example: context.AddDependency<OtherModule>();

        hive::LogInfo(hive::LogHiveRoot, "$ModuleName module configured");
    }

    void ${ModuleName}Module::DoInitialize()
    {
        hive::LogInfo(hive::LogHiveRoot, "$ModuleName module initialized");

        // Initialize your module here
    }

    void ${ModuleName}Module::DoShutdown()
    {
        hive::LogInfo(hive::LogHiveRoot, "$ModuleName module shutdown");

        // Cleanup your module here
    }
}
"@
    Set-Content -Path "$ModuleName\src\$ModuleLower\${ModuleLower}module.cpp" -Value $moduleImpl

    # Update CMakeLists.txt to include the module files
    $moduleSourcesContent = @"

# Module class sources
target_sources($ModuleLower PRIVATE
    src/$ModuleLower/${ModuleLower}module.cpp
)
"@
    Add-Content -Path "$ModuleName\CMakeLists.txt" -Value $moduleSourcesContent
}

# Create a README for the module
$readmeContent = @"
# $ModuleName Module

## Description
TODO: Add module description

## Dependencies
- Hive (core module)

## Usage
TODO: Add usage examples

## Integration
Add to root CMakeLists.txt:
``````cmake
add_subdirectory($ModuleName)
``````

Link in your target:
``````cmake
target_link_libraries(your_target PRIVATE $ModuleLower)
``````
"@
Set-Content -Path "$ModuleName\README.md" -Value $readmeContent

Write-Host ""
Write-Host "✓ Module '$ModuleName' created successfully!" -ForegroundColor Green
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "1. Add 'add_subdirectory($ModuleName)' to the root CMakeLists.txt"
Write-Host "2. Implement your module functionality in $ModuleName\src\$ModuleLower\"
Write-Host "3. Add public headers in $ModuleName\include\$ModuleLower\"
if ($WithModuleClass) {
    Write-Host "4. Register the module in your main application"
}
Write-Host ""
Write-Host "Directory structure created:"
Get-ChildItem -Path $ModuleName -Recurse | Select-Object FullName
