#!/usr/bin/env python3
"""
ScarlettOS Package Manager
Simple package installation and management tool
"""

import os
import sys
import json
import shutil
import argparse
from pathlib import Path

PACKAGE_DB = "/var/lib/scpkg/packages.json"
PACKAGE_CACHE = "/var/cache/scpkg"
INSTALL_PREFIX = "/usr/local"

class PackageManager:
    def __init__(self):
        self.db_path = Path(PACKAGE_DB)
        self.cache_path = Path(PACKAGE_CACHE)
        self.packages = self.load_db()
    
    def load_db(self):
        """Load package database"""
        if self.db_path.exists():
            with open(self.db_path) as f:
                return json.load(f)
        return {}
    
    def save_db(self):
        """Save package database"""
        self.db_path.parent.mkdir(parents=True, exist_ok=True)
        with open(self.db_path, 'w') as f:
            json.dump(self.packages, f, indent=2)
    
    def install(self, package_file):
        """Install a package"""
        pkg_path = Path(package_file)
        if not pkg_path.exists():
            print(f"Error: Package file not found: {package_file}")
            return False
        
        # Read package manifest
        # TODO: Extract and read manifest.json from package
        print(f"Installing package: {pkg_path.name}")
        
        # For now, simple file copy
        # TODO: Proper package extraction and installation
        
        pkg_name = pkg_path.stem
        self.packages[pkg_name] = {
            "version": "1.0.0",
            "installed": True,
            "files": []
        }
        
        self.save_db()
        print(f"Successfully installed {pkg_name}")
        return True
    
    def uninstall(self, package_name):
        """Uninstall a package"""
        if package_name not in self.packages:
            print(f"Error: Package not installed: {package_name}")
            return False
        
        print(f"Uninstalling package: {package_name}")
        
        # Remove files
        pkg_info = self.packages[package_name]
        for file_path in pkg_info.get("files", []):
            try:
                os.remove(file_path)
            except OSError:
                pass
        
        del self.packages[package_name]
        self.save_db()
        print(f"Successfully uninstalled {package_name}")
        return True
    
    def list_packages(self):
        """List installed packages"""
        if not self.packages:
            print("No packages installed")
            return
        
        print("Installed packages:")
        for name, info in self.packages.items():
            version = info.get("version", "unknown")
            print(f"  {name} ({version})")
    
    def search(self, query):
        """Search for packages"""
        # TODO: Implement repository search
        print(f"Searching for: {query}")
        print("Repository search not yet implemented")

def main():
    parser = argparse.ArgumentParser(description="ScarlettOS Package Manager")
    subparsers = parser.add_subparsers(dest="command", help="Command")
    
    # Install command
    install_parser = subparsers.add_parser("install", help="Install a package")
    install_parser.add_argument("package", help="Package file to install")
    
    # Uninstall command
    uninstall_parser = subparsers.add_parser("uninstall", help="Uninstall a package")
    uninstall_parser.add_argument("package", help="Package name to uninstall")
    
    # List command
    list_parser = subparsers.add_parser("list", help="List installed packages")
    
    # Search command
    search_parser = subparsers.add_parser("search", help="Search for packages")
    search_parser.add_argument("query", help="Search query")
    
    args = parser.parse_args()
    
    if not args.command:
        parser.print_help()
        return 1
    
    pm = PackageManager()
    
    if args.command == "install":
        return 0 if pm.install(args.package) else 1
    elif args.command == "uninstall":
        return 0 if pm.uninstall(args.package) else 1
    elif args.command == "list":
        pm.list_packages()
        return 0
    elif args.command == "search":
        pm.search(args.query)
        return 0

if __name__ == "__main__":
    sys.exit(main())
