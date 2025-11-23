#!/usr/bin/env python3
"""
ScarlettOS Package Manager
Simple package installation and management tool
"""

import os
import sys
import json
import shutil
import tarfile
import zipfile
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
            try:
                with open(self.db_path) as f:
                    return json.load(f)
            except json.JSONDecodeError:
                return {}
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
        
        print(f"Installing package: {pkg_path.name}")
        
        # Create temporary extraction directory
        extract_dir = self.cache_path / "tmp" / pkg_path.stem
        if extract_dir.exists():
            shutil.rmtree(extract_dir)
        extract_dir.mkdir(parents=True, exist_ok=True)
        
        try:
            # Extract package
            if pkg_path.suffix == '.zip':
                with zipfile.ZipFile(pkg_path, 'r') as zip_ref:
                    zip_ref.extractall(extract_dir)
            elif pkg_path.suffix in ['.tar', '.gz', '.tgz']:
                with tarfile.open(pkg_path, 'r:*') as tar_ref:
                    tar_ref.extractall(extract_dir)
            else:
                # Assume raw directory or unknown format, just copy for now if it's a dir
                if pkg_path.is_dir():
                    shutil.copytree(pkg_path, extract_dir, dirs_exist_ok=True)
                else:
                    print(f"Error: Unsupported package format: {pkg_path.suffix}")
                    return False

            # Read manifest
            manifest_path = extract_dir / "manifest.json"
            if not manifest_path.exists():
                print("Error: Package missing manifest.json")
                # Fallback: Create default manifest
                manifest = {
                    "name": pkg_path.stem,
                    "version": "1.0.0",
                    "description": "No description",
                    "files": []
                }
            else:
                with open(manifest_path) as f:
                    manifest = json.load(f)
            
            pkg_name = manifest.get("name", pkg_path.stem)
            
            # Install files
            installed_files = []
            content_dir = extract_dir / "content"
            if not content_dir.exists():
                # If no content dir, assume root of package is content
                content_dir = extract_dir
            
            for root, dirs, files in os.walk(content_dir):
                for file in files:
                    if file == "manifest.json":
                        continue
                        
                    src_file = Path(root) / file
                    rel_path = src_file.relative_to(content_dir)
                    dest_file = Path(INSTALL_PREFIX) / rel_path
                    
                    # Skip if it tries to overwrite system files (basic protection)
                    if str(dest_file).startswith("/bin") or str(dest_file).startswith("/boot"):
                        print(f"Warning: Skipping protected path: {dest_file}")
                        continue

                    dest_file.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(src_file, dest_file)
                    installed_files.append(str(dest_file))
                    print(f"  Installed: {rel_path}")
            
            # Update database
            self.packages[pkg_name] = {
                "version": manifest.get("version", "1.0.0"),
                "description": manifest.get("description", ""),
                "installed": True,
                "files": installed_files
            }
            
            self.save_db()
            print(f"Successfully installed {pkg_name}")
            return True
            
        except Exception as e:
            print(f"Error installing package: {e}")
            return False
        finally:
            # Cleanup
            if extract_dir.exists():
                shutil.rmtree(extract_dir)
    
    def uninstall(self, package_name):
        """Uninstall a package"""
        if package_name not in self.packages:
            print(f"Error: Package not installed: {package_name}")
            return False
        
        print(f"Uninstalling package: {package_name}")
        
        # Remove files
        pkg_info = self.packages[package_name]
        files_removed = 0
        for file_path in pkg_info.get("files", []):
            try:
                p = Path(file_path)
                if p.exists():
                    p.unlink()
                    files_removed += 1
                    # Try to remove empty parent directories
                    try:
                        p.parent.rmdir() 
                    except OSError:
                        pass # Directory not empty
            except OSError as e:
                print(f"Warning: Failed to remove {file_path}: {e}")
        
        del self.packages[package_name]
        self.save_db()
        print(f"Successfully uninstalled {package_name} ({files_removed} files removed)")
        return True
    
    def list_packages(self):
        """List installed packages"""
        if not self.packages:
            print("No packages installed")
            return
        
        print(f"{'Name':<20} {'Version':<10} {'Description'}")
        print("-" * 60)
        for name, info in self.packages.items():
            version = info.get("version", "unknown")
            desc = info.get("description", "")
            print(f"{name:<20} {version:<10} {desc}")
    
    def search(self, query):
        """Search for packages"""
        # Mock repository search
        print(f"Searching for: {query}")
        print("Repository search not yet implemented. Local matches:")
        
        found = False
        for name, info in self.packages.items():
            if query.lower() in name.lower() or query.lower() in info.get("description", "").lower():
                print(f"  {name} - {info.get('description', '')}")
                found = True
        
        if not found:
            print("  No local packages match.")

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
