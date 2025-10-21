# Chapter 5: West - Theory

---
[Introduction](./README.md) | [Theory](./theory.md) | [Lab](./lab.md) | [Course Home](../index.md)

---

This theory section provides deep understanding of West's architecture, commands, and manifest system. You'll learn how West manages multi-repository workspaces and enables reproducible, scalable Zephyr development workflows.

---

## Understanding West Architecture

### West's Design Philosophy

West follows a **multi-repository** approach to project management, recognizing that professional embedded development involves multiple interdependent code repositories. Unlike monolithic repositories that contain everything in a single location, West orchestrates collections of specialized repositories, each serving a specific purpose.

**Core Principles:**

* **Declarative Configuration:** Manifest files specify *what* your workspace should contain, not *how* to build it
* **Reproducible Environments:** Identical manifest files produce identical workspaces across different machines and times
* **Version Control Integration:** Every component is precisely versioned, enabling rollback and change tracking
* **Workspace Isolation:** Each West workspace is self-contained, preventing project interference

### West Workspace Structure

A West workspace follows a standardized directory layout:

```bash
my_workspace/                  # Workspace root
├── .west/                     # West configuration and metadata
│   ├── config                 # Workspace-specific configuration
│   └── manifests/            # Manifest repository data
├── zephyr/                   # Zephyr RTOS source code
├── modules/                  # External modules and libraries
│   ├── lib/
│   ├── hal/
│   └── debug/
├── tools/                    # Development tools
├── bootloader/              # Bootloader source (if used)
└── my_application/          # Your application code
```

**Key Components:**

- **Manifest Repository:** Contains the `west.yml` file that defines the workspace
- **Zephyr Repository:** The core Zephyr RTOS codebase
- **Module Repositories:** External libraries, HALs, and third-party code
- **Application Repository:** Your custom application code

## Essential West Commands

### Workspace Initialization

**Creating a New Workspace:**

```bash
# Initialize workspace from official Zephyr manifest
west init ~/my_zephyr_workspace
cd ~/my_zephyr_workspace

# Update all repositories according to manifest
west update

# Verify workspace status
west status
```

**Initialize with Custom Manifest:**

```bash
# Initialize from custom manifest repository
west init -m https://github.com/mycompany/custom-manifest.git workspace
cd workspace
west update
```

### Repository Management Commands

**Status and Information:**

```bash
# Show status of all repositories
west status

# List all projects in workspace
west list

# Show detailed project information
west list -f "{name:12} {path:28} {revision:40}"

# Check for uncommitted changes across all repos
west diff

# Show commit logs across repositories
west log --oneline
```

**Repository Updates:**

```bash
# Update all repositories to manifest specifications
west update

# Update only specific projects
west update zephyr modules/hal/nordic

# Force update (discards local changes)
west update --rebase

# Update with statistics
west update --stats
```

**Branch and Tag Management:**

```bash
# Create feature branch across multiple repositories
west forall -c "git checkout -b feature/new-sensor"

# Show current branches across all repositories
west forall -c "git branch --show-current"

# Synchronize to specific manifest revision
west update --manifest-rev v3.5.0
```

### Build System Integration

**Building Applications:**

```bash
# Build for Raspberry Pi 4B
west build -b rpi_4b samples/hello_world

# Build with pristine (clean) build directory
west build -b rpi_4b --pristine samples/hello_world

# Build with custom configuration
west build -b rpi_4b -DCONF_FILE=prj_release.conf samples/hello_world
```

**Build Management:**

```bash
# Clean build directory
west build -t clean

# Show build configuration
west build -t kconfig-print

# Generate build system files
west build -t rebuild_cache

# Build specific targets
west build -t flash
west build -t debug
```

## West Manifest Files

### Manifest File Structure

The `west.yml` file defines your workspace configuration using YAML syntax:

```yaml
# Basic manifest structure
manifest:
  defaults:
    remote: upstream      # Default remote for projects
    revision: main       # Default branch/tag/commit
    
  remotes:
    - name: upstream
      url-base: https://github.com/zephyrproject-rtos
    - name: company
      url-base: https://github.com/mycompany
    
  projects:
    - name: zephyr
      revision: v3.5.0
      west-commands: scripts/west-commands.yml
      
    - name: custom-drivers
      remote: company
      revision: v2.1.0
      path: modules/drivers/custom
      
    - name: sensor-lib
      url: https://github.com/thirdparty/sensor-lib.git
      revision: abc123def456
      path: modules/lib/sensors

  self:
    west-commands: tools/west-commands.yml
```

**Key Manifest Elements:**

* **defaults:** Set common values for projects
* **remotes:** Define URL bases for repository groups
* **projects:** List repositories with specific versions and locations
* **self:** Configure the manifest repository itself

### Advanced Manifest Features

**Conditional Project Inclusion:**

```yaml
projects:
  - name: test-framework
    revision: v1.2.0
    groups:
      - testing
      
  - name: production-tools
    revision: v3.0.0
    groups:
      - release
```

**Using Group Filters:**

```bash
# Include only testing-related repositories
west config manifest.group-filter +testing

# Exclude testing repositories
west config manifest.group-filter -testing

# Complex filtering
west config manifest.group-filter "+testing,-experimental,+production"
```

**Path Mapping and Imports:**

```yaml
manifest:
  projects:
    - name: hal-drivers
      path: modules/hal/drivers
      submodules: true        # Include Git submodules
      
    - name: shared-config
      import: config/west.yml  # Import manifest from this project
```

### Practical Manifest Examples

**Development Environment Manifest:**

```yaml
# dev-manifest.yml - Development environment
manifest:
  defaults:
    remote: origin
    revision: main
    
  remotes:
    - name: origin
      url-base: https://github.com/myteam
    - name: upstream
      url-base: https://github.com/zephyrproject-rtos
      
  projects:
    - name: zephyr
      remote: upstream
      revision: v3.5.0
      west-commands: scripts/west-commands.yml
      
    - name: application
      path: app
      revision: develop      # Development branch
      
    - name: custom-drivers
      path: drivers
      revision: main
      
    - name: test-suite
      path: tests
      groups: [testing]
      
  group-filter: [+testing]  # Include testing by default
```

**Production Release Manifest:**

```yaml
# release-manifest.yml - Locked production versions
manifest:
  defaults:
    remote: upstream
    
  remotes:
    - name: upstream
      url-base: https://github.com/zephyrproject-rtos
    - name: release
      url-base: https://github.com/mycompany/releases
      
  projects:
    - name: zephyr
      revision: 8a63b32f1c2d3e4f5g6h7i8j9k0l  # Exact commit hash
      west-commands: scripts/west-commands.yml
      
    - name: application
      remote: release
      revision: v2.1.0                       # Tagged release
      path: app
      
    - name: drivers
      remote: release
      revision: v1.5.2                       # Stable driver version
      path: drivers
      
  group-filter: [-testing, -development]    # Exclude dev/test repos
```

## Advanced West Operations

### Workspace Maintenance

**Cleaning and Resetting:**

```bash
# Remove untracked files from all repositories
west forall -c "git clean -fd"

# Reset all repositories to manifest state
west forall -c "git reset --hard HEAD"

# Completely reset workspace to manifest specification
west update --rebase --keep-descendants
```

**Configuration Management:**

```bash
# Show all West configuration
west config

# Set global configuration
west config --global user.name "Developer Name"
west config --global user.email "dev@company.com"

# Set workspace-specific configuration
west config manifest.path manifests
west config manifest.file custom-manifest.yml

# Configure build settings
west config build.board rpi_4b
west config build.pristine auto
```

### Multi-Target Development

**Board Configuration:**

```bash
# Set default board for workspace
west config build.board rpi_4b

# Build for multiple targets
west build -b rpi_4b
west build -b arduino_nano_33_ble --build-dir build-nano

# Compare builds across targets
west build -b rpi_4b --build-dir build-rpi
west build -b nordic_nrf52840dk --build-dir build-nordic
```

**Configuration Variants:**

```bash
# Debug builds
west build -b rpi_4b -DCONF_FILE="prj.conf debug.conf"

# Release builds  
west build -b rpi_4b -DCONF_FILE="prj.conf release.conf"

# Testing builds
west build -b rpi_4b -DCONF_FILE="prj.conf testing.conf"
```

### Troubleshooting West Issues

**Common Problems and Solutions:**

**Repository Synchronization Issues:**

```bash
# Check repository status
west status

# Force update with local change handling
west update --rebase

# Reset to clean state
west forall -c "git reset --hard @{u}"
west update
```

**Build Environment Problems:**

```bash
# Clear build cache
west build -t clean
rm -rf build/

# Rebuild with pristine environment
west build -b rpi_4b --pristine

# Check West command resolution
west --verbose build -b rpi_4b
```

**Manifest Resolution Issues:**

```bash
# Validate manifest syntax
west manifest --validate

# Show resolved manifest
west manifest --resolve

# Debug manifest parsing
west --verbose update
```

## Best Practices for West Development

### Manifest Design Principles

1. **Use Exact Revisions for Releases:** Always specify commit hashes or tags for production manifests
2. **Organize with Groups:** Use manifest groups to separate development, testing, and production dependencies
3. **Minimize Dependencies:** Only include repositories that are actually needed
4. **Document Manifest Changes:** Use clear commit messages when updating manifest files

### Team Development Workflows

**Feature Development Process:**

```bash
# 1. Create feature branch in manifest repo
git checkout -b feature/new-sensor-support
vim west.yml  # Update manifest for new dependencies

# 2. Update workspace to new manifest
west update

# 3. Develop across multiple repositories
west forall -c "git checkout -b feature/new-sensor-support"

# 4. Test and validate
west build -b rpi_4b
west build -t test

# 5. Commit and merge manifest changes
git add west.yml
git commit -m "Add sensor library dependency for new feature"
```

### Version Control Integration

**Manifest Repository Management:**

```bash
# Tag releases in manifest repository
git tag -a v2.1.0 -m "Release v2.1.0 with sensor support"
git push origin v2.1.0

# Create release branch
git checkout -b release/v2.1 v2.1.0

# Maintain stable manifest versions
git checkout main
git merge release/v2.1
```

---

This theoretical foundation prepares you for hands-on West usage in the Lab section, where you'll create custom manifests and implement professional development workflows using your Raspberry Pi 4B as the target platform.

[Next: West Lab](./lab.md)
