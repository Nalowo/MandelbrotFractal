from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps
from conan.tools.files import copy, rmdir
import os

class GeometryAppConan(ConanFile):
    name = "sender_model_app"
    version = "0.0.1"
    settings = "os", "compiler", "build_type", "arch"
    
    def requirements(self):
        self.requires("gtest/1.13.0")
        self.tool_requires("cmake/3.30.0")
    
    def layout(self):
        self.folders.source = "."
        self.folders.build = "build"
        self.folders.generators = "build/generators"
    
    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        
        tc = CMakeToolchain(self)
        tc.generate()
    
    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
