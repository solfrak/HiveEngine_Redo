#!/bin/bash

# Script to create a new module for HiveEngine
# Usage: ./create_module.sh <ModuleName> [--with-module-class]

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if module name is provided
if [ -z "$1" ]; then
    echo -e "${RED}Error: Module name is required${NC}"
    echo "Usage: $0 <ModuleName> [--with-module-class]"
    echo "Example: $0 Nectar"
    echo "Example: $0 Nectar --with-module-class"
    exit 1
fi

MODULE_NAME=$1
MODULE_LOWER=$(echo "$MODULE_NAME" | tr '[:upper:]' '[:lower:]')
WITH_MODULE_CLASS=false

# Check for optional flags
if [ "$2" == "--with-module-class" ]; then
    WITH_MODULE_CLASS=true
fi

# Check if module already exists
if [ -d "$MODULE_NAME" ]; then
    echo -e "${RED}Error: Module '$MODULE_NAME' already exists!${NC}"
    exit 1
fi

echo -e "${GREEN}Creating module: $MODULE_NAME${NC}"

# Create directory structure
echo "Creating directory structure..."
mkdir -p "$MODULE_NAME/include/$MODULE_LOWER"
mkdir -p "$MODULE_NAME/src/$MODULE_LOWER"

# Create precomp.h
echo "Creating precomp.h..."
cat > "$MODULE_NAME/include/$MODULE_LOWER/precomp.h" << EOF
#pragma once

#include <hive/precomp.h>

// Add module-specific precompiled headers here
EOF

# Create CMakeLists.txt
echo "Creating CMakeLists.txt..."
cat > "$MODULE_NAME/CMakeLists.txt" << EOF
cmake_minimum_required(VERSION 3.30.5)

add_library($MODULE_LOWER STATIC)
target_include_directories($MODULE_LOWER PUBLIC include PRIVATE src)
target_precompile_headers($MODULE_LOWER PRIVATE include/$MODULE_LOWER/precomp.h)

# Link against Hive (core module)
target_link_libraries($MODULE_LOWER PUBLIC hive)

# Add source files here
# target_sources($MODULE_LOWER PRIVATE
#     src/$MODULE_LOWER/example.cpp
# )
EOF

# Create module class if requested
if [ "$WITH_MODULE_CLASS" = true ]; then
    echo "Creating module class..."

    # Create module header
    cat > "$MODULE_NAME/include/$MODULE_LOWER/${MODULE_LOWER}module.h" << EOF
#pragma once

#include <hive/core/module.h>

namespace $MODULE_LOWER
{
    class ${MODULE_NAME}Module : public hive::Module
    {
    public:
        ${MODULE_NAME}Module() = default;
        ~${MODULE_NAME}Module() override = default;

        const char* GetName() const override { return GetStaticName(); }
        static const char* GetStaticName() { return "$MODULE_NAME"; }

    protected:
        void DoConfigure(hive::ModuleContext& context) override;
        void DoInitialize() override;
        void DoShutdown() override;
    };
}
EOF

    # Create module implementation
    cat > "$MODULE_NAME/src/$MODULE_LOWER/${MODULE_LOWER}module.cpp" << EOF
#include <$MODULE_LOWER/precomp.h>
#include <$MODULE_LOWER/${MODULE_LOWER}module.h>
#include <hive/core/log.h>

namespace $MODULE_LOWER
{
    void ${MODULE_NAME}Module::DoConfigure(hive::ModuleContext& context)
    {
        // Add dependencies here
        // Example: context.AddDependency<OtherModule>();

        hive::LogInfo(hive::LogHiveRoot, "$MODULE_NAME module configured");
    }

    void ${MODULE_NAME}Module::DoInitialize()
    {
        hive::LogInfo(hive::LogHiveRoot, "$MODULE_NAME module initialized");

        // Initialize your module here
    }

    void ${MODULE_NAME}Module::DoShutdown()
    {
        hive::LogInfo(hive::LogHiveRoot, "$MODULE_NAME module shutdown");

        // Cleanup your module here
    }
}
EOF

    # Update CMakeLists.txt to include the module files
    cat >> "$MODULE_NAME/CMakeLists.txt" << EOF

# Module class sources
target_sources($MODULE_LOWER PRIVATE
    src/$MODULE_LOWER/${MODULE_LOWER}module.cpp
)
EOF
fi

# Create a README for the module
cat > "$MODULE_NAME/README.md" << EOF
# $MODULE_NAME Module

## Description
TODO: Add module description

## Dependencies
- Hive (core module)

## Usage
TODO: Add usage examples

## Integration
Add to root CMakeLists.txt:
\`\`\`cmake
add_subdirectory($MODULE_NAME)
\`\`\`

Link in your target:
\`\`\`cmake
target_link_libraries(your_target PRIVATE $MODULE_LOWER)
\`\`\`
EOF

echo ""
echo -e "${GREEN}✓ Module '$MODULE_NAME' created successfully!${NC}"
echo ""
echo -e "${YELLOW}Next steps:${NC}"
echo "1. Add 'add_subdirectory($MODULE_NAME)' to the root CMakeLists.txt"
echo "2. Implement your module functionality in $MODULE_NAME/src/$MODULE_LOWER/"
echo "3. Add public headers in $MODULE_NAME/include/$MODULE_LOWER/"
if [ "$WITH_MODULE_CLASS" = true ]; then
    echo "4. Register the module in your main application"
fi
echo ""
echo "Directory structure created:"
tree "$MODULE_NAME" 2>/dev/null || find "$MODULE_NAME" -print | sed -e 's;[^/]*/;|____;g;s;____|; |;g'
